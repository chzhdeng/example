#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include "pms_global.h"
#include "message.h"

extern const char *action_type[ACTION_TOTAL+1];

//using namespace pmsBaseNamespace;
namespace pmsBaseNamespace
{
    pmsBase::pmsBase()
    {
        pmsServerPortGet();
        m_queue_empty = true;
        pmsSetClientModeSupport(true);
        pmsSetServerModeSupport(true);

        cgilog(SYSLOG_INFO, "pmsBase consturction.");
    }

    pmsBase::~pmsBase()
    {
        pmsQueueClean(&request_pms_queue);
        cgilog(SYSLOG_INFO, "~pmsBase.");
    }

    void pmsBase::pmsServerPortGet()
    {
        char *port = NULL;
        const char *cmd = "SELECT ucm_port from pms_settings;";

        port = runSQLGetOneVarcharField(cmd, "ucm_port", PBXMID_UCM_CONFIG_DB);
        if (port != NULL && port[0] != '\0')
        {
            m_server_port = atoi(port);
            cgilog(SYSLOG_DEBUG,"listen port is: %d \n" ,m_server_port);
        }
        else
        {
            m_server_port = PMS_SERVER_PORT_DEFAULT;
            cgilog(SYSLOG_DEBUG,"default listen port is: %d \n", m_server_port);
        }

        SAFE_FREE(port);
    }

    void pmsBase::pmsTaskFree(PMSTask *task)
    {
        if (task != NULL)
        {
            if (task->info != NULL)
            {
                freePMSInfo(task->info);
                task->info = NULL;
            }
            SAFE_FREE(task->acctid);
            SAFE_FREE(task->action);
            SAFE_FREE(task->room_info.address);
            SAFE_FREE(task);
        }
    }

    /*
     * get the event from sql if the send_status is 1
     * @a_type   the action type, like ST, WAKEUP
    */
    int pmsBase::pmsQueueAddTaskFromSQL(int a_type)
    {
        const char *action = NULL;
        char cmd[1024] = {0};
        StrList *address_list = NULL;
        StrList *tmp_address_list = NULL;
        const char *table = NULL;

        const char *key = NULL;
        char *address = NULL;
        int ret = MIDCODE_ERROR;

        /* get the table name and table key by action type */
        switch (a_type)
        {
            case ACTION_CHKI:
            case ACTION_UPDATE:
            case ACTION_MOV:
            case ACTION_DND:
            case ACTION_MSG:
            case ACTION_WAKE:
                table = TBL_PMS_WAKEUP;
                break;
            case ACTION_CHKO:
            case ACTION_CALL:
            case ACTION_ST:
                table = TBL_PMS_ROOM;
                key = SQL_NODE_PMS_ST;
                break;
            case ACTION_TOTAL:
            default:
                ret = MIDCODE_UNSUPPORT;
                return ret;
        }

        /* get the action name */
        action = action_type[a_type];

        /* get all the items that we need, and add them into taskQueue */
        snprintf(cmd, sizeof(cmd), "SELECT address from %s WHERE send_status=1", table);
        address_list = runSQLGetStrList(cmd, PBXMID_UCM_CONFIG_DB);
        tmp_address_list = address_list;

        while (tmp_address_list != NULL)
        {
            address = tmp_address_list->value;
            if (address == NULL || address[0] == '\0')
            {
                continue;
            }

            if (a_type == ACTION_WAKE)
            {
                /** get the wake up cmd by w_action
                 *  Action ID: 0 = cancelled, 1 = programmed, 2= executed
                 *  w_mode depending on action:
                 *  If programmed: w_mode = w_type
                 *  If executed: w_mode = w_status
                 *   */
                snprintf(cmd, sizeof(cmd), "SELECT %s from %s, %s where w_action=2 AND %s.address='%s' AND pms_wakeup.address  = pms_room.address"
                                    " UNION SELECT %s from %s, %s where w_action=1 AND %s.address='%s' AND pms_wakeup.address  = pms_room.address"
                                    " UNION SELECT %s from %s, %s where w_action=0 AND %s.address='%s' AND pms_wakeup.address  = pms_room.address",
                                    SQL_NODE_PMS_WAKEUP_EXE, TBL_PMS_ROOM, TBL_PMS_WAKEUP, TBL_PMS_ROOM, address,
                                    SQL_NODE_PMS_WAKEUP_SET, TBL_PMS_ROOM, TBL_PMS_WAKEUP, TBL_PMS_ROOM, address,
                                    SQL_NODE_PMS_WAKEUP_CANCEL, TBL_PMS_ROOM, TBL_PMS_WAKEUP, TBL_PMS_ROOM, address
                                    );
                /* if there is wakeup action, run the crontab to tack effect */
                system(CMD_PMS_WAKEUP_CRONTAB);
            }
            else
            {
                snprintf(cmd, sizeof(cmd), "SELECT %s FROM %s WHERE address='%s'", key, table, address);
            }

            cgilog(SYSLOG_DEBUG, "Check the event :%s address is %s.", action, address);

            /* get the action data by sql */
            pmsProtocolDataAdd2Queue(cmd, address, action);

            /* reset the send_status after push the event to task */
            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd) - 1, "UPDATE %s SET send_status=0 WHERE address='%s' ", table, address);
            cgilog(SYSLOG_DEBUG, "pms client send message to complete, clean status:%s. ", cmd);
            pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            if (a_type == ACTION_ST)
            {
                memset(cmd, 0, sizeof(cmd));
                snprintf(cmd, sizeof(cmd) - 1, "UPDATE %s SET status='0' WHERE address='%s' AND chki_status='no' AND status!='2' AND status!='3' ", table, address);
                pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            }

            tmp_address_list = tmp_address_list->next;
        }

        if (address_list != NULL)
        {
            freeStrList(address_list);
            address_list = NULL;
        }

        ret = MIDCODE_SUCCESS;

        return ret;
    }

    /* get CDR record and handle to PMS's xml format, then add it to pms event list */
    int pmsBase::pmsQueueAddCdrTask()
    {
        int i = 0, j =0;
        int ret = 0;
        int len = 0;
        int call_type = 0;
        int duration  = 0;
        int ring_time = 0;
        int internal_flag = 0;

        char *room      = NULL;
        char *ptr       = NULL;
        char *start     = NULL;
        char *extension = NULL;
        char *address   = NULL;

        char buf[1024]  = { 0 };
        char buf2[1024] = { 0 };
        char cmd[512]   = { 0 };
        char c_date[16] = { 0 };
        char c_time[16] = { 0 };

        CDR cdr;
        PMSTask *task = NULL;
        const char *val  = NULL;
        struct array_list *list = NULL;
        struct json_object *sub = NULL;
        struct json_object *result = NULL;
        const char *action = action_type[ACTION_CALL];

        if (cdr_pms_flag == 0)
        {
            /* printf("no get cdr, continue...\n"); */
            return 0;
        }

        memset(&cdr, 0, sizeof(cdr));

        snprintf(cmd, sizeof(cmd),
                 "SELECT userfield, AcctId, src, dst, start, duration, billsec "
                 "FROM cdr_upload WHERE pms_flag=1 limit 10");

        if (runSQLGetAllItem(cmd, &result, PBXMID_ASTERISK_EVENT_DB) < 0)
        {
            return -1;
        }

        if ( (list = json_object_get_array(result)) == NULL)
        {
            json_object_put(result);
            return -1;
        }

        /* get all need upload record in cdr_upload table and handle */
        while (1)
        {
            internal_flag = 0;
            if ( (sub = (json_object *)array_list_get_idx(list, i++)) == NULL)
            {
                break;  /* break after handle all record */
            }
            /* get every record and add to cdr struct */
            json_object_object_foreach(sub, key, value)
            {
                if ( (val = json_object_get_string(value)) == NULL)
                {
                    val = "";
                }
                if (strcmp(key, "userfield") == 0)
                {
                    cdr.call_type = strDup(val);
                }
                else if (strcmp(key, "AcctId") == 0)
                {
                    cdr.acctid = strDup(val);
                }
                else if (strcmp(key, "src") == 0)
                {
                    cdr.caller = strDup(val);
                }
                else if (strcmp(key, "dst") == 0)
                {
                    cdr.called = strDup(val);
                }
                else if (strcmp(key, "start") == 0)
                {
                    cdr.start = strDup(val);
                }
                else if (strcmp(key, "duration") == 0)
                {
                    cdr.call_time = atoi(val);
                }
                else if (strcmp(key, "billsec") == 0)
                {
                    cdr.duration_time = atoi(val);
                }
            }

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "<%s>", action);

            if (strcmp(cdr.call_type, "Outbound") == 0)
                call_type = 1;
            else if (strcmp(cdr.call_type, "Inbound") == 0)
                call_type = 2;
            else if (strcmp(cdr.call_type, "Internal") == 0)
            {
                call_type = 3;
                cdr_pms_flag++;
            }

            /* get the address and room parameter */
            if (call_type == 1 || call_type == 3)
                extension = cdr.caller;
            else
                extension = cdr.called;

            snprintf(cmd, sizeof cmd, "SELECT address FROM %s WHERE extension='%s'", TBL_PMS_ROOM, extension);
            if ((address = runSQLGetOneVarcharField(cmd, "address", PBXMID_UCM_CONFIG_DB)) == NULL)
            {
                if (call_type == 3)
                {
                    snprintf(cmd, sizeof cmd, "SELECT address FROM %s WHERE extension='%s'", TBL_PMS_ROOM, cdr.called);
                    if ((address = runSQLGetOneVarcharField(cmd, "address", PBXMID_UCM_CONFIG_DB)) != NULL)
                    {
                        internal_flag = 1;
                    }
                }

                if (address == NULL)
                {
                    snprintf(cmd, sizeof(cmd), "update cdr_upload set pms_flag=0 where AcctId=%s", cdr.acctid);
                    pmsRunSQLCmd(cmd, PBXMID_ASTERISK_EVENT_DB);
                    freeCdr(&cdr);
                    cdr_pms_flag--;
                    continue;
                }
            }

            snprintf(cmd, sizeof cmd, "SELECT room FROM %s WHERE extension='%s'",TBL_PMS_ROOM, extension);
            if ( (room = runSQLGetOneVarcharField(cmd, "room", PBXMID_UCM_CONFIG_DB)) == NULL)
            {
                if (call_type == 3)
                {
                    snprintf(cmd, sizeof cmd, "SELECT room FROM %s WHERE extension='%s'", TBL_PMS_ROOM, cdr.called);
                    room = runSQLGetOneVarcharField(cmd, "room", PBXMID_UCM_CONFIG_DB);
                }
                if (room == NULL)
                {
                    snprintf(cmd, sizeof(cmd), "update cdr_upload set pms_flag=0 where AcctId=%s", cdr.acctid);
                    pmsRunSQLCmd(cmd, PBXMID_ASTERISK_EVENT_DB);
                    SAFE_FREE(address);
                    freeCdr(&cdr);
                    cdr_pms_flag--;
                    continue;
                }
            }

            len = strlen(buf);
            snprintf(buf+len, sizeof(buf)-len, "<room>%s</room>", room);
            SAFE_FREE(room);

            len = strlen(buf);
            snprintf(buf+len, sizeof buf-len, "<c_type>%d</c_type><caller>%s</caller><called>%s</called>",
                     call_type, cdr.caller, cdr.called);

            len = strlen(buf);
            start = strDup(cdr.start);
            if ( (ptr = strchr(start, ' ')) != NULL)
            {
                j = 0;
                *ptr='\0';
                ptr = start;
                while (*ptr != '\0')
                {
                    if (*ptr != '-')
                        c_date[j++] = *ptr;
                    ptr++;
                }
                ptr++;
                j = 0;
                while (*ptr != '\0')
                {
                    if (*ptr != ':')
                        c_time[j++] = *ptr;
                    ptr++;
                }
                snprintf(buf+len, sizeof(buf)-len,
                         "<c_date>%s</c_date><c_time>%s</c_time>", c_date, c_time);
            }
            SAFE_FREE(start);

            duration = cdr.duration_time;
            ring_time = cdr.call_time - duration;
            len = strlen(buf);
            snprintf(buf+len, sizeof(buf)-len, "<ring_time>%02d%02d</ring_time>", ring_time/60, ring_time%60);
            len = strlen(buf);
            snprintf(buf+len, sizeof(buf)-len, "<duration>%02d%02d%02d</duration>",
                     duration/3600, duration%3600/60, duration%60);

            len = strlen(buf);
            snprintf(buf+len, sizeof(buf)-len, "</%s>", action);

            /* printf("%s\n", buf); */
            if ( (task = pmsTaskCreate()) != NULL)
            {
                task->room_info.address = strDup(address);
                task->acctid  = strDup(cdr.acctid);
                task->action  = strDup(action);
                task->info->data    = strDup(buf);
                if (task->info->credential)
                {
                    ((pms1__Tcredential*)task->info->credential)->requesttime = pmsGetCurrentTime();
                    setUserInformation(task->info);
                }
                pmsTaskAddEvent(task);
            }
            SAFE_FREE(address);

            if (call_type == 3 && internal_flag == 0)
            {
                snprintf(cmd, sizeof cmd, "SELECT address FROM %s WHERE extension='%s'", TBL_PMS_ROOM, cdr.called);
                if ((address = runSQLGetOneVarcharField(cmd, "address", PBXMID_UCM_CONFIG_DB)) == NULL)
                {
                    freeCdr(&cdr);
                    cdr_pms_flag--;
                    continue;
                }

                snprintf(cmd, sizeof cmd, "SELECT room FROM %s WHERE extension='%s'", TBL_PMS_ROOM, cdr.called);
                if ((room = runSQLGetOneVarcharField(cmd, "room", PBXMID_UCM_CONFIG_DB)) == NULL)
                {
                    SAFE_FREE(address);
                    freeCdr(&cdr);
                    cdr_pms_flag--;
                    continue;
                }

                memset(buf2, 0, sizeof(buf2));
                snprintf(buf2, sizeof(buf2), "<%s>", action);

                len = strlen(buf2);
                snprintf(buf2+len, sizeof(buf2)-len, "<room>%s</room>", room);
                SAFE_FREE(room);

                if((ptr = strstr(buf, "<c_type>")) != NULL)
                {
                    len = strlen(buf2);
                    snprintf(buf2+len, sizeof(buf2)-len, ptr);
                }

                if ( (task = pmsTaskCreate()) != NULL)
                {
                    task->room_info.address = strDup(address);
                    task->acctid  = strDup(cdr.acctid);
                    task->action  = strDup(action);
                    task->info->data    = strDup(buf);
                    if (task->info->credential)
                    {
                        ((pms1__Tcredential*)task->info->credential)->requesttime = pmsGetCurrentTime();
                        setUserInformation(task->info);
                    }
                    pmsTaskAddEvent(task);
                }

                SAFE_FREE(address);
            }

            freeCdr(&cdr);
        }

        /* printf("pms i = %d\n", i); */
        if (i == 1)
        {   /* i == 1 when no select any record */
            /* here we need set cdr_pms_flag to 0, to avoid loop select endless */
            cdr_pms_flag = 0;
        }

        json_object_put(result);
        return ret;
    }


    /* add the room status and wakeup event to the task */
    void pmsBase::pmsQueueHandler()
    {
        int ret = MIDCODE_SUCCESS;

        if (m_queue_empty)
        {
            ret = pmsQueueAddTaskFromSQL(ACTION_WAKE);
            if (MIDCODE_SUCCESS != ret)
            {
                cgilog(SYSLOG_DEBUG, "add ClientWakeup error, err code %d!", ret);
            }

            ret = pmsQueueAddTaskFromSQL(ACTION_ST);
            if (MIDCODE_SUCCESS != ret)
            {
                cgilog(SYSLOG_DEBUG, "add ClientStatus error, err code %d!", ret);
            }

            ret = pmsQueueAddCdrTask();
            if (MIDCODE_SUCCESS != ret)
            {
                cgilog(SYSLOG_DEBUG, "add ClientCDR error, err code %d!", ret);
            }

            m_queue_empty = false;
        }

    }


    /***Hotel Mamager Task enqueue***/
    int pmsBase::pmsTaskEnqueue(PMSQueue *q, void *task)
    {
        PMSQueueNode* node = NULL;

        node = (PMSQueueNode *)malloc(sizeof(PMSQueueNode));
        if (NULL == node)
        {
            cgilog(SYSLOG_ERR, "pmsTaskEnqueue malloc error");
            return -1;
        }

        node->value = task;
        node->next = NULL;

        if (0 == q->count_q_node)
        {
            q->head = q->end = node;
            q->count_q_node = 1;
        }
        else
        {
            q->end->next = node;
            q->end = node;
            q->count_q_node = q->count_q_node + 1;
        }

        return 0;
    }

    void *pmsBase::pmsTaskDequeue(PMSQueue *q)
    {
        PMSQueueNode *enq_node = NULL;
        void *pValue = NULL;

        if (0 == q->count_q_node)
        {
            return NULL;
        }

        if (q->head)
        {
            pValue = q->head->value;
            enq_node = q->head;
            q->head = q->head->next;
            free(enq_node);
        }

        q->count_q_node = q->count_q_node - 1;

        return pValue;
    }

    /* add the action to queue, and send to PMS */
    int pmsBase::pmsTaskAddEvent(PMSTask *task)
    {
        int ret = 0;

        if (task == NULL)
            return -1 ;

        pmsTaskAddMD5ForPassword(task);

        ret = pmsTaskEnqueue(&request_pms_queue, (void *)task);
        if (0 != ret)
        {
            pmsTaskFree(task);
        }

        return 0;
    }

    void pmsBase::pmsQueueClean(PMSQueue *queue)
    {
        PMSQueueNode *free_q_head_node = NULL;
        PMSTask *free_task = NULL;

        cgilog(SYSLOG_DEBUG,"pmsQueueClean   start ===== !");

        if (queue)
        {
            //pthread_mutex_lock(&queue->q_lock);
            while(queue->head != NULL)
            {
                free_q_head_node = queue->head->next;
                if (queue->head->value != NULL)
                {
                    free_task = (PMSTask *)(queue->head->value);
                    pmsTaskFree(free_task);
                    queue->head->value = NULL;
                }

                free(queue->head);
                queue->head = free_q_head_node;
            }
            queue->head = queue->end = NULL;
            queue->count_q_node = 0;
            //pthread_mutex_unlock(&queue->q_lock);
        }

        cgilog(SYSLOG_DEBUG,"pmsQueueClean   end ===== !");

    }

    int pmsBase::pmsClientHandler()
    {
        int i = 0;
        int ret = 0;

        if (pmsClientTimeoutCheck() < 0)
        {
            return -1;
        }

        for (i=0; i<m_send_num; i++)
        {
            ret = this->pmsClienthandleOneTask();
            if (0 != ret)
            {
                break;
            }
        }

        return 0;
    }

    int pmsBase::pmsClienthandleOneTask()
    {
        int ret = 0;
        void *tmp_deq_task =NULL;
        PMSTask * task = NULL;

        tmp_deq_task = pmsTaskDequeue(&request_pms_queue);
        if (NULL == tmp_deq_task)
        {
            m_queue_empty = true;
            return -1;
        }

        task = (PMSTask *)tmp_deq_task;
        ret = pmsClientSendReq2Pms(task);
        if (0 == ret) //success
        {
            ret = pmsClientHandleRspFromPms(task);
            if (0 == ret)
            {
                //pmsTaskFree(task);
            }
            else
            {
                return -1;
            }
        }
        #if 0
        else //fail
        {
            pmsTaskFree(task);
            return -1;
        }
        #endif

        return 0;
    }

    int pmsBase::pmsClientTaskClean()
    {
        int ret = 0;

        while(1)
        {
            ret = pmsClienthandleOneTask();
            if (0 != ret)
            {
                break;
            }
        }

        return 0;
    }

    int pmsBase::pmsServerUpdateRoomInformation(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_SUCCESS;
        struct pmsRoomInfo *tmp_room = NULL;
        int len = 0;
        char items[256] = {0};

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        tmp_room = &(task->room_info);
        cgilog(SYSLOG_DEBUG, "step 1: update room information");

        PMS_ADD_SQL_ITEM(items, account, tmp_room->account, account);
        PMS_ADD_SQL_ITEM(items, vipcode, tmp_room->vipcode, vipcode);
        PMS_ADD_SQL_ITEM(items, datein, tmp_room->datein, datein);
        PMS_ADD_SQL_ITEM(items, dateout, tmp_room->dateout, dateout);
        PMS_ADD_SQL_ITEM(items, credit, tmp_room->credit, credit);
        /***set checkin status***/
        PMS_SET_CHKI_STATUS(items, checkin);

        PMS_REMOVE_EXTRA_CHAR(items);
        len = strlen(items);
        if (len > 0 && tmp_room->address)
        {
            snprintf(cmd, sizeof(cmd) - 1,
                "UPDATE %s SET %s WHERE address = '%s'; ",
                TBL_PMS_ROOM, items, tmp_room->address);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
        }

        return ret;
    }

    int pmsBase::pmsServerUpdateUsersInformation(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_SUCCESS;
        struct pmsRoomInfo *tmp_room = NULL;
        int len = 0;
        char items[256] = {0};

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        tmp_room = &(task->room_info);
        cgilog(SYSLOG_DEBUG, "step 2: update Users information");

        PMS_ADD_SQL_ITEM(items, first_name, tmp_room->name, name);
        PMS_ADD_SQL_ITEM(items, last_name, tmp_room->surname, surname);

        pmsSetLanguageForSQL(tmp_room);
        PMS_ADD_SQL_ITEM(items, language, tmp_room->language, language);

        PMS_REMOVE_EXTRA_CHAR(items);
        len = strlen(items);
        if (len > 0)
        {
            snprintf(cmd, sizeof(cmd) - 1,
                "UPDATE %s SET %s WHERE user_name = '%s'; ",
                TBL_PMS_USERS, items, tmp_room->extension);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
        }

        return ret;
    }

    int pmsBase::pmsServerUpdateExtenInformation(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_SUCCESS;
        char *tablename = NULL;
        struct pmsRoomInfo *tmp_room = NULL;
        int len = 0;
        char items[256] = {0};

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }
        cgilog(SYSLOG_DEBUG, "step 3: update Extension information");

        tmp_room = &(task->room_info);
        pmsSetExtensionPermission(tmp_room);
        tablename = pmsGetTableNameByExt(tmp_room->extension);
        if (tablename != NULL)
        {
            PMS_ADD_SQL_ITEM(items, fullname, tmp_room->fullname, fullname);
            PMS_ADD_SQL_ITEM(items, permission, tmp_room->cos, cos);

            PMS_REMOVE_EXTRA_CHAR(items);
            len = strlen(items);
            if (len > 0)
            {
                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE %s SET %s WHERE extension = '%s'; ",
                    tablename, items, tmp_room->extension);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            }
        }

        return ret;
    }

    int pmsBase::pmsServerCheckin(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_SUCCESS;

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_DEBUG, "Enter Checkin Handle ... ...");
        /**clean wakeup messages and voicemail when room is being checkin**/
        cleanRoomExtVoicemail(task->room_info.extension);
        if (cleanWakeupInformation(task) == 0)
        {
            cgilog(SYSLOG_INFO, "successfuly clean wake up!");
        }

        if (isHandleTableInformation(task, TBL_PMS_ROOM))
        {
            ret = pmsServerUpdateRoomInformation(task);
        }

        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_USERS))
        {
            ret = pmsServerUpdateUsersInformation(task);
        }

        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            ret = pmsServerUpdateExtenInformation(task);
        }

        if (ret == MIDCODE_SUCCESS)
        {
            snprintf(cmd, sizeof(cmd) -1,
                "UPDATE global SET ischange=1 WHERE table_name IN ('%s', '%s', '%s'); ",
                  TBL_PMS_ROOM, TBL_PMS_USERS, TBL_PMS_SIP_ACCOUNTS);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            pmsRoomStatusNotify(task);
        }

        if (ret != MIDCODE_SUCCESS)
        {
            pmsDataErrRsp(task);
        }

        return ret;
    }


    int pmsBase::pmsServerCheckout(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_ERROR;
        struct pmsRoomInfo *tmp_room = NULL;

        if (task == NULL)
        {
            return ret;
        }

        cgilog(SYSLOG_DEBUG, "Enter Checkout handle ... ...");
        tmp_room = &(task->room_info);
        /***room information set default values***/
        if (isHandleTableInformation(task, TBL_PMS_ROOM) && tmp_room->address)
        {
            snprintf(cmd, sizeof(cmd)-1,
                "UPDATE %s SET %s WHERE address = '%s'; ",
                    TBL_PMS_ROOM, PMS_CLEAN_ROOM_INFO, tmp_room->address);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
        }

        /***user information set default values***/
        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_USERS))
        {
            memset(cmd, '\0', sizeof(cmd));
            snprintf(cmd, sizeof(cmd)-1,
                "UPDATE %s SET %s WHERE user_name = '%s'; ",
                TBL_PMS_USERS, PMS_CLEAN_USERS_INFO, tmp_room->extension);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
        }

        /***extension information set default values***/
        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            memset(cmd, '\0', sizeof(cmd));
            snprintf(cmd, sizeof(cmd) - 1,
                "UPDATE %s SET %s WHERE extension = '%s'; ",
                TBL_PMS_SIP_ACCOUNTS, PMS_CLEAN_EXTEN_INFO, tmp_room->extension);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            /*when presence status of extension is dnd, setting it is available*/
            if (ret == MIDCODE_SUCCESS)
            {
                ret = pmsSetPresenceStatus(tmp_room->extension, "no");
            }
        }

        if (ret == MIDCODE_SUCCESS)
        {
            memset(cmd, '\0', sizeof(cmd));
            snprintf(cmd, sizeof(cmd)-1,
                "UPDATE global SET ischange=1 WHERE table_name IN ('%s', '%s', '%s') ;",
                TBL_PMS_ROOM, TBL_PMS_USERS, TBL_PMS_SIP_ACCOUNTS);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            cleanRoomCHKOExtVoicemail(tmp_room->extension);
        }

        if (ret != MIDCODE_SUCCESS)
        {
            pmsDataErrRsp(task);
        }

        pmsRoomStatusNotify(task);

        return ret;

    }

    int pmsBase::pmsServerUPDATE(PMSTask* task)
    {
        char cmd[1024] = {0};
        int ret = MIDCODE_SUCCESS;

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_DEBUG, "Enter Update handle ... ...");
        if (isHandleTableInformation(task, TBL_PMS_ROOM))
        {
            ret = pmsServerUpdateRoomInformation(task);
        }

        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_USERS))
        {
            ret = pmsServerUpdateUsersInformation(task);
        }

        if (ret == MIDCODE_SUCCESS && isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            ret = pmsServerUpdateExtenInformation(task);
        }

        if (ret == MIDCODE_SUCCESS)
        {
            snprintf(cmd, sizeof(cmd),
                "UPDATE global SET ischange=1 WHERE table_name IN ('%s', '%s', '%s'); ",
                  TBL_PMS_ROOM, TBL_PMS_USERS, TBL_PMS_SIP_ACCOUNTS);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            pmsRoomStatusNotify(task);
        }

        if (ret != MIDCODE_SUCCESS)
        {
            pmsDataErrRsp(task);
        }


        return ret;
    }

    int pmsBase::pmsServerMOV(PMSTask* task)
    {
        char cmd[1024] = {0};
        struct pmsRoomInfo *tmp_room = NULL;
        int ret = MIDCODE_ERROR;
        int count = 0;

        if (task == NULL || task->room_info.address == NULL)
        {
            return ret;
        }

        cgilog(SYSLOG_DEBUG, "Enter Move handle ... ...");
        tmp_room = &(task->room_info);
        //destination room informations are updated.
        ret = updateDstRoomInfo(task, tmp_room);
        if (ret != MIDCODE_SUCCESS)
        {
           pmsDataErrRsp(task);
            return ret;
        }

        //move voicemail to destination room
        moveRoomExtInfo(task);

        //old room informations are set to the default values.
        snprintf(cmd, sizeof(cmd)-1,
                "UPDATE %s SET %s WHERE address = '%s'; ",
                TBL_PMS_ROOM, PMS_CLEAN_ROOM_INFO, tmp_room->address);
        ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

        /***move old wakeup to destination address***/
        if (ret == MIDCODE_SUCCESS)
        {
            snprintf(cmd, sizeof(cmd)-1, "WHERE address = '%s' ", tmp_room->address);
            count = runSQLGetItemNumber(TBL_PMS_WAKEUP, cmd, PBXMID_UCM_CONFIG_DB);
            if (count > 0)
            {
                memset(cmd, '\0', sizeof(cmd));
                snprintf(cmd, sizeof(cmd)-1,
                    "REPLACE INTO %s SELECT  '%s' AS address, w_action,w_type, w_status,w_date, w_time, send_status FROM %s WHERE  address = '%s'; ",
                       TBL_PMS_WAKEUP, task->room_info.d_address, TBL_PMS_WAKEUP, task->room_info.address);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
                if (ret == MIDCODE_SUCCESS)
                {
                    memset(cmd, '\0', sizeof(cmd));
                    snprintf(cmd, sizeof(cmd) - 1,
                        "DELETE FROM %s WHERE address = '%s'; ",
                        TBL_PMS_WAKEUP, task->room_info.address);
                    ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
                }
            }
        }

        if (ret == MIDCODE_SUCCESS)
        {
            memset(cmd, '\0', sizeof(cmd));
            snprintf(cmd, sizeof(cmd)-1,
                "UPDATE global SET ischange=1 WHERE table_name IN ('pms_room', 'users', 'sip_accounts', 'pms_wakeup') ;");
            pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            pmsRoomStatusNotify(task);
        }

        system(CMD_PMS_WAKEUP_CRONTAB);

        return MIDCODE_SUCCESS;

    }


    int pmsBase::pmsServerDND(PMSTask* task)
    {
        char cmd[1024] = {0};
        struct pmsRoomInfo *tmp_room = NULL;
        int ret = MIDCODE_SUCCESS;
        char items[256] = {0};
        char cti_status[256] = {0};

        if (task == NULL)
        {
            return ret;
        }

        cgilog(SYSLOG_DEBUG, "Enter DND handle ... ...");
        tmp_room = &(task->room_info);
        pmsSetExtensionDnd(tmp_room);
        if (isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            cgilog(SYSLOG_DEBUG, "Set dnd information!");

            PMS_ADD_SQL_ITEM(items, dnd, tmp_room->status, status);
            if (strcasecmp(tmp_room->status, "yes") == 0)
            {
                PMS_ADD_SQL_CONST_ITEM(items, presence_status, "dnd");
                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE %s SET %s WHERE extension = '%s'; ",
                    TBL_PMS_SIP_ACCOUNTS, items, tmp_room->extension);

                cgilog(SYSLOG_DEBUG, "pmsServerDND cmd : %s", cmd);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

                snprintf(cti_status, sizeof(cti_status) - 1, "%s %s dnd",
                    CMD_PMS_PRESENCE_STATUS, tmp_room->extension);
            }
            else
            {
                PMS_ADD_SQL_CONST_ITEM(items, presence_status, "available");
                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE %s SET %s WHERE extension = '%s' AND presence_status = 'dnd'; ",
                    TBL_PMS_SIP_ACCOUNTS, items, tmp_room->extension);
                cgilog(SYSLOG_DEBUG, "pmsServerDND cmd : %s", cmd);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

                if ( ret == MIDCODE_SUCCESS )
                {
                    char * tmp_presence = NULL;
                    snprintf(cmd, sizeof(cmd)-1, "SELECT presence_status FROM %s WHERE extension = '%s';",
                        TBL_PMS_SIP_ACCOUNTS, tmp_room->extension);
                    tmp_presence = runSQLGetOneVarcharField(cmd, "presence_status", PBXMID_UCM_CONFIG_DB);
                    if (tmp_presence)
                    {
                        snprintf(cti_status, sizeof(cti_status) - 1, "%s %s %s",
                            CMD_PMS_PRESENCE_STATUS, tmp_room->extension, tmp_presence);
                    }
                    SAFE_FREE(tmp_presence);
                }
            }

            if ( ret == MIDCODE_SUCCESS )
            {
                memset(cmd, 0, sizeof(cmd));
                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE global SET ischange=1 WHERE table_name IN ('%s') ;",
                       TBL_PMS_SIP_ACCOUNTS);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
                /*report dnd online status to cti*/
                cgilog(SYSLOG_DEBUG, "dnd online status to cti: %s", cti_status);
                system(cti_status);
                pmsRoomStatusNotify(task);
            }

            if (ret != MIDCODE_SUCCESS)
            {
                pmsDataErrRsp(task);
            }
        }

        return ret;

    }

    int pmsBase::pmsServerWake(PMSTask* task)
    {
        char cmd[1024] = {0};
        struct pmsRoomInfo *tmp_room = NULL;
        int ret = MIDCODE_ERROR;
        int len = 0;
        char keys[256] = {0};
        char vals[256] = {0};

        if (task == NULL)
        {
            return ret;
        }

        cgilog(SYSLOG_DEBUG, "Enter Wakeup handle ... ...");
        tmp_room = &(task->room_info);
        if (isHandleTableInformation(task, TBL_PMS_WAKEUP) && tmp_room->address)
        {
            PMS_ADD_WAKEUP_ITEM(keys, vals, address, tmp_room->address, address);
            PMS_ADD_WAKEUP_ITEM(keys, vals, w_action, tmp_room->w_action, w_action);
            PMS_ADD_WAKEUP_ITEM(keys, vals, w_type, tmp_room->w_mode, w_mode);
            PMS_ADD_WAKEUP_ITEM(keys, vals, w_date, tmp_room->w_date, w_date);
            PMS_ADD_WAKEUP_ITEM(keys, vals, w_time, tmp_room->w_time, w_time);

            PMS_REMOVE_EXTRA_CHAR(keys);
            PMS_REMOVE_EXTRA_CHAR(vals);
            snprintf(cmd, sizeof(cmd) - 1,
                "REPLACE INTO %s (%s) VALUES (%s)",
                TBL_PMS_WAKEUP, keys, vals);
            cgilog(SYSLOG_DEBUG, "pms server wakeup cmd : %s", cmd);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            if (ret != MIDCODE_SUCCESS)
            {
                pmsDataErrRsp(task);
                return ret;
            }
            pmsRoomStatusNotify(task);
            /* make the crontab take effect */
            system(CMD_PMS_WAKEUP_CRONTAB);
        }

        return MIDCODE_SUCCESS;
    }

    int pmsBase::pmsServerMsg(PMSTask* task)
    {
        char cmd[512] = {0};
        struct pmsRoomInfo *tmp_room = NULL;
        int ret = MIDCODE_SUCCESS;
        char items[256] = {0};

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_DEBUG, "Enter MWI handle ... ...");
        tmp_room = &(task->room_info);
        if (isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            PMS_ADD_SQL_ITEM(items, message, tmp_room->status, status);
            PMS_ADD_SQL_CONST_ITEM(items, msg_send, "1");

            snprintf(cmd, sizeof(cmd) - 1,
                "UPDATE %s SET %s WHERE extension = '%s'; ",
                TBL_PMS_SIP_ACCOUNTS, items, tmp_room->extension);

            cgilog(SYSLOG_DEBUG, "pmsServerMSG cmd : %s", cmd);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            if ( ret == MIDCODE_SUCCESS )
            {
                memset(cmd, 0, sizeof(cmd));
                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE global SET ischange=1 WHERE table_name IN ('%s') ;",
                    TBL_PMS_SIP_ACCOUNTS);
                pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            }

        }

        return ret;
    }

    int pmsBase::pmsServerCFU(PMSTask* task)
    {
        char cmd[512] = {0};
        struct pmsRoomInfo *tmp_room = NULL;
        int ret = MIDCODE_SUCCESS;
        int len = 0;
        char items[256] = {0};

        if (task == NULL)
        {
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_DEBUG, "Enter CFU handle ... ...");
        tmp_room = &(task->room_info);
        if (isHandleTableInformation(task, TBL_PMS_SIP_ACCOUNTS))
        {
            PMS_ADD_SQL_ITEM(items, cfu, tmp_room->cfu, cfu);
            PMS_REMOVE_EXTRA_CHAR(items);

            snprintf(cmd, sizeof(cmd) - 1,
                "UPDATE %s SET %s WHERE extension = '%s'; ",
                TBL_PMS_SIP_ACCOUNTS, items, tmp_room->extension);
            cgilog(SYSLOG_DEBUG, "pms Server cfu cmd : %s", cmd);
            ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);

            if (ret == MIDCODE_SUCCESS)
            {
                memset(items, 0, sizeof(items));
                PMS_ADD_SQL_ITEM(items, cfu, tmp_room->cfu, cfu);
                len = strlen(tmp_room->cfu);
                if (len > 0)
                {
                    PMS_ADD_SQL_CONST_ITEM(items, cfu_destination_type, "2");
                }
                else
                {
                    PMS_ADD_SQL_CONST_ITEM(items, cfu_destination_type, "0");
                }

                snprintf(cmd, sizeof(cmd) - 1,
                    "UPDATE %s SET %s WHERE extension='%s' AND presence_status IN "
                    "(SELECT presence_status FROM sip_accounts WHERE extension='%s'); ",
                    TBL_PMS_SIP_PRESENCE, items, tmp_room->extension, tmp_room->extension);
                cgilog(SYSLOG_DEBUG, "update presence status information: %s", cmd);
                ret = pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            }

            if ( ret == MIDCODE_SUCCESS )
            {
                memset(cmd, 0, sizeof(cmd));
                snprintf(cmd, sizeof(cmd) -1,
                    "UPDATE global SET ischange=1 WHERE table_name IN ('%s', '%s') ;",
                       TBL_PMS_SIP_ACCOUNTS, TBL_PMS_SIP_PRESENCE);
                pmsRunSQLCmd(cmd, PBXMID_UCM_CONFIG_DB);
            }

        }

        return ret;
    }

    int pmsBase::pmsServerMinibar(PMSTask* task)
    {
        if (task == NULL || task->room_info.address == NULL)
        {
            return MIDCODE_ERROR;
        }

        return MIDCODE_SUCCESS;
    }

    void pmsBase::pmsRoomStatusNotify(PMSTask* task)
    {
        json_object *json = NULL;
        char cmd[1024] = {0};
        int ret = MIDCODE_ERROR;
        EventType type = EVENT_PMS_ROOM_STATUS;

        if (task == NULL || task->action == NULL || task->room_info.address == NULL)
        {
            return;
        }

        /*hmobile and mitel*/
        if (strcasecmp(task->action, "CHKI") == 0 || strcasecmp(task->action, "CHKO") == 0)
        {
            snprintf(cmd, sizeof(cmd), "SELECT address,extension,first_name,last_name,room,status,user_name,account,maid,credit,vipcode,datein,dateout "
                                       "FROM (SELECT address,room,extension,status,account,maid,vipcode,credit,"
                                       "datein,dateout,s_date,s_time,last_address,last_room FROM %s WHERE address='%s') "
                                       "LEFT JOIN (SELECT first_name,last_name,user_name FROM %s) ON extension=user_name", TBL_PMS_ROOM, task->room_info.address, TBL_PMS_USERS);
            ret = runSQLGetOneItem(cmd, &json, PBXMID_UCM_CONFIG_DB);
            if (ret != MIDCODE_SUCCESS)
            {
                cgilog(SYSLOG_ERR, "couldn't do sql:%s", cmd);
                return;
            }
        }
        else if (strcasecmp(task->action, "WAKE") == 0)
        {
            type = EVENT_PMS_WAKEUP_STATUS;

            snprintf(cmd, sizeof(cmd), "SELECT * FROM (SELECT address,w_action,w_type,w_status,w_date,w_time,send_status FROM %s WHERE address='%s')"
                                       "LEFT JOIN (SELECT room,address AS tmp2 FROM %s) ON address=tmp2", TBL_PMS_WAKEUP, task->room_info.address, TBL_PMS_ROOM);
            ret = runSQLGetOneItem(cmd, &json, PBXMID_UCM_CONFIG_DB);
            if (ret != MIDCODE_SUCCESS)
            {
                cgilog(SYSLOG_ERR, "couldn't do sql:%s", cmd);
                return;
            }
        }
        /*hmobile only*/
        else if (strcasecmp(task->action, "UPDATE") == 0)
        {
            if (isHandleTableInformation(task, TBL_PMS_ROOM))
            {
                snprintf(cmd, sizeof(cmd), "SELECT address,extension,first_name,last_name,room,status,user_name,account,maid,credit,vipcode,datein,dateout "
                                           "FROM (SELECT address,room,extension,status,account,maid,vipcode,credit,"
                                           "datein,dateout,s_date,s_time,last_address,last_room FROM %s WHERE address='%s') "
                                           "LEFT JOIN (SELECT first_name,last_name,user_name FROM %s) ON extension=user_name", TBL_PMS_ROOM, task->room_info.address, TBL_PMS_USERS);
            }
            else
            {
                snprintf(cmd, sizeof(cmd), "SELECT first_name,last_name,user_name AS  extension FROM %s WHERE user_name = '%s';", TBL_PMS_USERS, task->room_info.address);
            }

            ret = runSQLGetOneItem(cmd, &json, PBXMID_UCM_CONFIG_DB);
            if (ret != MIDCODE_SUCCESS)
            {
                cgilog(SYSLOG_ERR, "couldn't do sql:%s", cmd);
                return;
            }
        }
        else if (strcasecmp(task->action, "MOV") == 0)
        {
            snprintf(cmd, sizeof(cmd), "SELECT address,extension,first_name,last_name,room,status,user_name,account,maid,credit,vipcode,datein,dateout "
                                       "FROM (SELECT address,room,extension,status,account,maid,vipcode,credit,"
                                       "datein,dateout,s_date,s_time,last_address,last_room FROM %s WHERE address IN ('%s', '%s')) "
                                       "LEFT JOIN (SELECT first_name,last_name,user_name FROM %s) ON extension=user_name", TBL_PMS_ROOM, task->room_info.address, task->room_info.d_address, TBL_PMS_USERS);
            ret = runSQLGetOneItem(cmd, &json, PBXMID_UCM_CONFIG_DB);
            if (ret != MIDCODE_SUCCESS)
            {
                cgilog(SYSLOG_ERR, "couldn't do sql:%s", cmd);
                return;
            }
        }
        /*mitel only*/
        else if (strcasecmp(task->action, "NAME") == 0 || strcasecmp(task->action, "CREDIT") == 0 ||
            strcasecmp(task->action, "DND") == 0)
        {
            json = json_object_new_object();
            if (json == NULL)
            {
                cgilog(SYSLOG_ERR, "couldn't new a json object.");
                return;
            }

            json_object_object_add(json, "address", json_object_new_string(task->room_info.address));
            if (strcasecmp(task->action, "NAME") == 0)
            {
                json_object_object_add(json, "first_name", json_object_new_string(task->room_info.name[0] == PMS_HMOBILE_PARSE_EMPTY_VALUE ? "" : task->room_info.name));
                json_object_object_add(json, "last_name", json_object_new_string(task->room_info.surname[0] == PMS_HMOBILE_PARSE_EMPTY_VALUE ? "" : task->room_info.surname));
            }
            else if (strcasecmp(task->action, "CREDIT") == 0)
            {
                json_object_object_add(json, "credit", json_object_new_string(task->room_info.credit));
            }
            else if (strcasecmp(task->action, "DND") == 0)
            {
                json_object_object_add(json, "dnd", json_object_new_string(task->room_info.status));
            }
        }

        if (json)
        {
            eventContentCreate(type, task->room_info.address, json);
            pmsRoomConfigReloadNotify(task);
        }
    }

    void pmsBase::pmsRoomConfigReloadNotify(PMSTask *task)
    {
        EventConfigReload event = {0};

        if (task == NULL || task->action == NULL || task->room_info.address == NULL)
        {
            return;
        }

        memset(&event, 0, sizeof(event));
        event.source = CFG_SOURCE_PMS;
        event.user_name = NULL;
        event.date = time(NULL);
        event.module = CFG_MODULE_PMS_ROOM;
        if (strcasecmp(task->action, "CHKI") == 0)
        {
            event.action = MSG_JSON_ACTION_CHECKIN;
        }
        else if (strcasecmp(task->action, "CHKO") == 0)
        {
            event.action = MSG_JSON_ACTION_CHECKOUT;
        }
        else
        {
            event.action = MSG_JSON_ACTION_UPDATE;
        }

        event.content = json_object_new_object();
        if (event.content)
        {
            json_object_object_add(event.content, "address", json_object_new_string(task->room_info.address));
            eventConfigReloadNotify(&event);
            json_object_put(event.content);
        }
    }

    int pmsBase::pmsGetClientModeSupport(void)
    {
        return this->client_mode;
    }

    int pmsBase::pmsGetServerModeSupport(void)
    {
        return this->server_mode;
    }

    void pmsBase::pmsSetClientModeSupport(int mode)
    {
        this->client_mode = mode;
    }

    void pmsBase::pmsSetServerModeSupport(int mode)
    {
        this->server_mode = mode;
    }

}

