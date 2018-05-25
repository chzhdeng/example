#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <netdb.h>

#include "app_hsc.h"

namespace pmsHscBaseNamespace
{
    pmsHscBase::pmsHscBase()
    {
        cgilog(SYSLOG_INFO, "pmsHscBase consturction.");
        this->hsc_addr = NULL;
        this->hsc_room_info = NULL;
        this->hsc_server_task = NULL;
    }

    pmsHscBase::~pmsHscBase()
    {
        cgilog(SYSLOG_INFO, "~pmsHscBase.");
    }

    PMSTask *pmsHscBase::hscServerTaskCreate()
    {
        PMSTask *task = NULL;

        if ( (task = (PMSTask *)malloc(sizeof(PMSTask))) == NULL)
        {
            cgilog(SYSLOG_ERR, "Create pms task error.");
            return NULL;
        }
        memset(task, 0, sizeof(PMSTask));

        return task;
    }

    void pmsHscBase::hscServerTaskFree()
    {
        if (this->hsc_server_task)
        {
            HSC_SAFE_FREE(this->hsc_server_task->room_info.address);
            HSC_SAFE_FREE(this->hsc_server_task->action);
        }
        HSC_SAFE_FREE(this->hsc_server_task);
    }

    void pmsHscBase::hscCreateRoomInfo()
    {
        if ( (this->hsc_room_info = (hscRoomInfo *)malloc(sizeof(hscRoomInfo))) != NULL)
        {
            memset(this->hsc_room_info, 0, sizeof(hscRoomInfo));
            this->hsc_room_info->req_body_len = 0;
            this->hsc_room_info->res_body_len = 0;
            this->hsc_room_info->req_body = NULL;
        }
        else
        {
            cgilog(SYSLOG_ERR, "Create hsc room information error.");
        }

        cgilog(SYSLOG_DEBUG, "Create hsc room information.");
    }

    void pmsHscBase::hscFreeRoomInfo()
    {
        hscRoomInfo *room_info = NULL;
        cgilog(SYSLOG_DEBUG, "Free hsc room information.");

        if ((room_info = this->hscGetRoomInfo()) != NULL)
        {

            //HSC_SAFE_FREE(room_info->req_buf);
            //HSC_SAFE_FREE(room_info->res_buf);
            HSC_SAFE_FREE(room_info->req_body);
            HSC_SAFE_FREE(room_info->param.key);
            HSC_SAFE_FREE(room_info->param.value);

            json_object_put(room_info->res_body);

            HSC_SAFE_FREE(room_info);
            this->hsc_room_info = NULL;
        }
    }

    hscAddr *pmsHscBase::hscGetSinAddr()
    {
        return this->hsc_addr;
    }

    hscRoomInfo *pmsHscBase::hscGetRoomInfo()
    {
        return this->hsc_room_info;
    }

    void pmsHscBase::hscAddRequestTask(PMSTask *task)
    {
        this->hsc_server_task = task;
    }

    PMSTask *pmsHscBase::hscGetRequestTask()
    {
        return this->hsc_server_task;
    }

    void pmsHscBase::hscSetExecuteStatus(hscExeStatus status)
    {
        this->hsc_status = status;
    }

    hscExeStatus pmsHscBase::hscGetExecuteStatus()
    {
        return this->hsc_status;
    }

    int pmsHscBase::hscServerMsgAddToTask()
    {
        int ret = MIDCODE_ERROR;
        int len = 0;
        char *key = NULL;
        char *value = NULL;
        hscRoomInfo *room_info = NULL;
        PMSTask *task = NULL;
        char tmp[128] = {0};
        hscAction action = HSC_ACTION_TOTAL;
        struct pmsRoomInfo *tmp_room = NULL;

        if ((task = this->hscGetRequestTask()) == NULL || (room_info = this->hscGetRoomInfo()) == NULL ||
            (value = room_info->param.value) == NULL)
        {
            return MIDCODE_ERROR;
        }

        /**add extension or address**/
        strncpy(task->room_info.extension, room_info->extension, sizeof(task->room_info.extension) -1);
        task->room_info.address = strdup(room_info->extension);

        tmp_room = &(task->room_info);
        action = room_info->action;
        switch(action)
        {
            case HSC_ACTION_NAME:
                key = tmp_room->fullname;
                len = sizeof(tmp_room->fullname) - 1;
                PMS_FLAG_ROOM_INFO(tmp_room, fullname);
                task->action = strdup("UPDATE");
                break;
            case HSC_ACTION_PERMISSION:
                key = tmp_room->cos;
                len = sizeof(tmp_room->cos) - 1;
                PMS_FLAG_ROOM_INFO(tmp_room, cos);
                task->action = strdup("UPDATE");
                break;
            case HSC_ACTION_MWI:
                key = tmp_room->status;
                len = sizeof(tmp_room->status) - 1;
                PMS_FLAG_ROOM_INFO(tmp_room, status);
                task->action = strdup("MSG");
                break;
            case HSC_ACTION_DND:
                key = tmp_room->status;
                len = sizeof(tmp_room->status) - 1;
                PMS_FLAG_ROOM_INFO(tmp_room, status);
                task->action = strdup("DND");
                break;
            case HSC_ACTION_CFWT:
                key = tmp_room->cfu;
                len = sizeof(tmp_room->cfu) - 1;
                PMS_FLAG_ROOM_INFO(tmp_room, cfu);
                task->action = strdup("CFU");
                break;
            default:
                break;
        }

        ret = MIDCODE_SUCCESS;
        if (room_info->method == HSC_RETRIEVE_METHOD)
        {
            snprintf(tmp, sizeof(tmp)-1, "0");
            if (action == HSC_ACTION_CFWT || action == HSC_ACTION_NAME)
            {
                memset(tmp, '\0', sizeof(tmp));
            }
        }
        else if (room_info->method == HSC_CHANGE_METHOD)
        {
            snprintf(tmp, sizeof(tmp)-1, "%s", value);
            if (action == HSC_ACTION_CFWT && atoi(value) == 0)
            {
                memset(tmp, '\0', sizeof(tmp));
            }

            if (action == HSC_ACTION_PERMISSION)
            {
                snprintf(tmp, sizeof(tmp)-1, "%d", (atoi(value)+1));
            }
        }
        else
        {
            ret = MIDCODE_ERROR;
        }

        /***add changing value***/
        //snprintf(key, len, "%s", tmp);
        strncpy(key, tmp, len);
        cgilog(SYSLOG_DEBUG, "Set key: %s", key);

        /***handle first name and last name***/
        if (action == HSC_ACTION_NAME)
        {
            cgilog(SYSLOG_DEBUG, "handle first name and last name!");
            pmsHandleExtensionName(tmp_room);
            PMS_FLAG_TABLE_INFO(tmp_room, users);

            if ((len = strlen(tmp_room->name)) > 32 ||
                (len = strlen(tmp_room->surname)) > 32)
            {
                ret = MIDCODE_ERROR;
                this->hscSetExecuteStatus(HSC_NAMR_INVALID);
            }
        }

        PMS_FLAG_TABLE_INFO(tmp_room, sip_accounts);

        return ret;
    }

    int pmsHscBase::pmsServerMsgReceive()
    {
        int recv_len = 0;
        int surplus_len = PMS_HSC_BUFSIZE - 1;
        int ret = MIDCODE_ERROR;
        int flag_recv_finish = 0;
        hscAddr *addr = NULL;
        hscRoomInfo *room_info = NULL;
        hscRequestMethod method = HSC_NONE_METHOD;

        if ((addr = this->hscGetSinAddr()) == NULL ||
            (room_info = this->hscGetRoomInfo()) == NULL)
        {
            return ret;
        }

        while ((recv_len = hscReceiveRequest(addr)) > 0 && (surplus_len > recv_len))
        {
            snprintf(room_info->req_buf + strlen(room_info->req_buf), surplus_len, "%s", addr->buf);
            memset(addr->buf, '\0', sizeof(addr->buf));
            surplus_len = (PMS_HSC_BUFSIZE - strlen(room_info->req_buf) - 1);

            method = hscGetRequestMethodType(room_info->req_buf);
            switch (method)
            {
                case HSC_RETRIEVE_METHOD:
                    flag_recv_finish = 1;
                    break;
                case HSC_CHANGE_METHOD:
                    flag_recv_finish = hscIsReceiveDataFinish(room_info->req_buf);
                    break;
                default:
                    break;
            }
            HSC_RECV_MSG_FINISH(flag_recv_finish);
        }

        room_info->method = method;
        cgilog(SYSLOG_INFO, "step 1: Receive Hsc Message ");
        cgilog(SYSLOG_INFO, "%s", room_info->req_buf);

        return MIDCODE_SUCCESS;
    }

    hscAction pmsHscBase::hscServerGetAction(char *value)
    {
        hscAction action = HSC_ACTION_TOTAL;

        if (value == NULL)
        {
            return action;
        }

        if (strstr(value, HSC_PARAMETER_NAME) != NULL)
        {
            action = HSC_ACTION_NAME;
        }
        else if (strstr(value, HSC_PARAMETER_PERMISSION) != NULL)
        {
            action = HSC_ACTION_PERMISSION;
        }
        else if (strstr(value, HSC_PARAMETER_MWI) != NULL)
        {
            action = HSC_ACTION_MWI;
        }
        else if (strstr(value, HSC_PARAMETER_DND) != NULL)
        {
            action = HSC_ACTION_DND;
        }
        else if (strstr(value, HSC_PARAMETER_CFWT) != NULL)
        {
            action = HSC_ACTION_CFWT;
        }

        return action;
    }

    int pmsHscBase::hscServerMsgParseForRetrieve()
    {
        //int ret = MIDCODE_ERROR;
        char *tmp = NULL;
        char *save = NULL;
        char *seperator = NULL;
        hscAddr *addr = NULL;
        hscRoomInfo *room_info = NULL;
        char paramName[64] = {0};
        char paramValue[64] = {0};
        hscAction action = HSC_ACTION_TOTAL;

        if ((addr = this->hscGetSinAddr()) == NULL ||
            (room_info = this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        char *retrieve_content = hscGetRetrieveContent(room_info->req_buf);
        if (retrieve_content == NULL)
        {
            this->hscSetExecuteStatus(HSC_NAMR_INVALID);
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_INFO, "step 2-1: parse Retrieve message");
        cgilog(SYSLOG_INFO, "%s", retrieve_content);

        tmp = strtok_r(retrieve_content, "&", &save);
        while(tmp != NULL)
        {
            if ((seperator = strchr(tmp, '=')) != NULL)
            {
                snprintf(paramName, (seperator - tmp + 1) * sizeof(char), "%s", tmp);
                snprintf(paramValue, sizeof(paramValue)-1, "%s", seperator+1);
                cgilog(SYSLOG_DEBUG, "Key: {%s}, Value: {%s} ", paramName, paramValue);

                if (strcasecmp(paramName, HSC_PARAMETER_EXTENSION) == 0)
                {
                    snprintf(room_info->extension, sizeof(room_info->extension) - 1, "%s", paramValue);
                }
                else
                {
                    action = this->hscServerGetAction(paramName);
                    room_info->param.key = strdup(paramName);
                    room_info->param.value = strdup(paramValue);
                }
            }
            tmp = strtok_r(NULL, "&", &save);
        }

        room_info->action = action;
        cgilog(SYSLOG_INFO, "retrieve action is: %d, %s=%s ", action, room_info->param.key, room_info->param.value);

        HSC_SAFE_FREE(retrieve_content);

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::hscServerMsgParseForChange()
    {
        //int ret = MIDCODE_ERROR;
        hscAddr *addr = NULL;
        hscRoomInfo *room_info = NULL;
        hscAction action = HSC_ACTION_TOTAL;
        char key[64] = {0};
        char *exten = NULL;
        char *value = NULL;

        if ((addr = this->hscGetSinAddr()) == NULL ||
            (room_info = this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        char *post_content = hscGetbodyContent(room_info->req_buf);
        cgilog(SYSLOG_INFO, "step 2-1: parse Change message ");
        cgilog(SYSLOG_INFO, "%s", post_content);

        if (post_content)
        {
            room_info->req_body = post_content;
        }

        action = this->hscServerGetAction(post_content);
        switch(action)
        {
            case HSC_ACTION_NAME:
                HSC_ADD_PARSE_AGR(key, HSC_PARAMETER_NAME);
                break;
            case HSC_ACTION_PERMISSION:
                HSC_ADD_PARSE_AGR(key, HSC_PARAMETER_PERMISSION);
                break;
            case HSC_ACTION_MWI:
                HSC_ADD_PARSE_AGR(key, HSC_PARAMETER_MWI);
                break;
            case HSC_ACTION_DND:
                HSC_ADD_PARSE_AGR(key, HSC_PARAMETER_DND);
                break;
            case HSC_ACTION_CFWT:
                HSC_ADD_PARSE_AGR(key, HSC_PARAMETER_CFWT);
                break;
            default:
                break;
        }

        exten = hscJsonContentParse(post_content, HSC_PARAMETER_EXTENSION);
        if (exten == NULL)
        {
            cgilog(SYSLOG_ERR, "parse extension error!");
            this->hscSetExecuteStatus(HSC_NAMR_INVALID);
            return MIDCODE_ERROR;
        }
        cgilog(SYSLOG_DEBUG, "parse [%s] is: [%s], value len: [%d] !", HSC_PARAMETER_EXTENSION, exten, (int)(strlen(exten)));
        snprintf(room_info->extension, sizeof(room_info->extension)-1, "%s", exten);

        value = hscJsonContentParse(post_content, key);
        if (value == NULL)
        {
            cgilog(SYSLOG_ERR, "parse [%s] error!", key);
            this->hscSetExecuteStatus(HSC_NAMR_INVALID);
            HSC_SAFE_FREE(exten);
            return MIDCODE_ERROR;
        }
        cgilog(SYSLOG_DEBUG, "parse [%s] is: [%s], value len: [%d] !", key, value, (int)(strlen(value)));
        room_info->param.key = strdup(key);
        room_info->param.value = value;

        room_info->action = action;
        HSC_SAFE_FREE(exten);
        //HSC_SAFE_FREE(value);

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::pmsServerMsgParse()
    {
        int ret = MIDCODE_ERROR;
        hscAddr *addr = NULL;
        hscRoomInfo *room_info = NULL;

        if ((addr = this->hscGetSinAddr()) == NULL ||
            (room_info = this->hscGetRoomInfo()) == NULL)
        {
            return ret;
        }

        switch (room_info->method)
        {
            case HSC_RETRIEVE_METHOD:
                ret = this->hscServerMsgParseForRetrieve();
                break;
            case HSC_CHANGE_METHOD:
                ret = this->hscServerMsgParseForChange();
                break;
            default:
                break;
        }

        return ret;
    }

    int pmsHscBase::hscServerMsgCheckName()
    {
        //int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        if(!isUtf8String(room_info->param.value) ||
        pmsCheckSpecialCharacters(room_info->param.value))
        {
            cgilog(SYSLOG_ERR, "content encoding is not utf-8 or name exists especial character!");
            this->hscSetExecuteStatus(HSC_NAMR_INVALID);
            return MIDCODE_ERROR;
        }

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::hscServerMsgCheckPermission()
    {
        //int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::hscServerMsgCheckMWI()
    {
        //int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        return MIDCODE_SUCCESS;

    }
    int pmsHscBase::hscServerMsgCheckDND()
    {
        //int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::hscServerMsgCheckCfwt()
    {
        int i = 0;
        int len = 0;
        int ret = MIDCODE_SUCCESS;
        char cfu[64] = {0};
        hscRoomInfo *room_info = NULL;
        int flag_cfu_err = 0;

        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        snprintf(cfu, sizeof(cfu)-1, "%s", room_info->param.value);
        len = strlen(cfu);

        if (len <= 32)
        {
            for (i = 0; i < len; i++)
            {
                if ((cfu[i] >='0' && cfu[i] <= '9') ||(cfu[i] >='A' && cfu[i] <= 'Z') || (cfu[i] >='a' && cfu[i] <= 'a') ||
                    cfu[i] == '+' || cfu[i] == '-' || cfu[i] == '*' || cfu[i] == '#')
                {
                    continue;
                }
                else
                {
                    flag_cfu_err = 1;
                    break;
                }
            }
        }
        else
        {
            //modify Bug 93752
            flag_cfu_err = 1;
        }

        if (flag_cfu_err)
        {
            ret = MIDCODE_ERROR;
            this->hscSetExecuteStatus(HSC_CFU_INVALID);
        }

        return ret;
    }

    int pmsHscBase::pmsServerMsgCheck()
    {
        int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        if ((room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }
        cgilog(SYSLOG_INFO, "step 2-2: check message ");

        /***first, check extension exists or not***/
        if (!pmsCheckExtension(room_info->extension))
        {
            cgilog(SYSLOG_ERR, "the extension(%s) don't exist !!!", room_info->extension);
            this->hscSetExecuteStatus(HSC_NO_EXTENSION);
            return MIDCODE_ERROR;
        }

        /***check username and password***/
        if (hscCheckAuthorization(room_info))
        {
            cgilog(SYSLOG_ERR, "the username or password invalid !!!");
            this->hscSetExecuteStatus(HSC_AUTH_ERR);
            return MIDCODE_ERROR;
        }

        if (HSC_RETRIEVE_METHOD == room_info->method)
        {
            return MIDCODE_SUCCESS;
        }

        /***check request message***/
        switch(room_info->action)
        {
            case HSC_ACTION_NAME:
                ret = this->hscServerMsgCheckName();
                break;
            case HSC_ACTION_PERMISSION:
                ret = this->hscServerMsgCheckPermission();
                break;
            case HSC_ACTION_MWI:
                ret = this->hscServerMsgCheckMWI();
                break;
            case HSC_ACTION_DND:
                ret = this->hscServerMsgCheckDND();
                break;
            case HSC_ACTION_CFWT:
                ret = this->hscServerMsgCheckCfwt();
                break;
            default:
                break;
        }

        return ret;
    }

    int pmsHscBase::hscServerMsgExecute()
    {
        int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        hscAction action = HSC_ACTION_TOTAL;
        int apply_change = true;
        PMSTask *task = this->hscGetRequestTask();

        if ((room_info = this->hscGetRoomInfo()) == NULL || task == NULL)
        {
            return MIDCODE_ERROR;
        }

        action = room_info->action;
        cgilog(SYSLOG_DEBUG, "action is: %d", action);
        switch (action)
        {
            case HSC_ACTION_NAME:
                ret = MIDCODE_SUCCESS;
                ret = pmsServerUPDATE(task);
                break;
            case HSC_ACTION_PERMISSION:
                ret = MIDCODE_SUCCESS;
                ret = pmsServerUPDATE(task);
                break;
            case HSC_ACTION_MWI:
                ret = pmsServerMsg(task);
                break;
            case HSC_ACTION_DND:
                ret = pmsServerDND(task);
                break;
            case HSC_ACTION_CFWT:
                ret = pmsServerCFU(task);
                break;
            default:
                apply_change = false;
                break;
        }

        /* if this action is parse success, mark the nvram is change. */
        if (ret == MIDCODE_SUCCESS && apply_change == true)
        {
            pmsEnableChange();
        }

        return ret;
    }

    int pmsHscBase::pmsServerMsgExec()
    {
        cgilog(SYSLOG_INFO, "step 3: execute message ");

        int ret = MIDCODE_ERROR;
        hscRoomInfo *room_info = NULL;
        PMSTask *task = NULL;
        if ((room_info = this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        /***The old task will be cleaned Before Creating new task***/
        task = this->hscGetRequestTask();
        if (task)
        {
            this->hscServerTaskFree();
            task = NULL;
        }

        /***Creating new task***/
        cgilog(SYSLOG_DEBUG, "create pms Task ... ");
        task = this->hscServerTaskCreate();
        if (task)
        {
            this->hscAddRequestTask(task);
            ret = this->hscServerMsgAddToTask();
            if (ret == MIDCODE_SUCCESS)
            {
                ret = this->hscServerMsgExecute();
            }
        }
        /***Clean new task***/
        cgilog(SYSLOG_DEBUG, "Clean pms Task done!!! ");
        this->hscServerTaskFree();

        return ret;
    }

    int pmsHscBase::pmsServerMsgResponse()
    {
        //int ret = MIDCODE_ERROR;
        int len = 0;
        hscAddr *addr = NULL;
        hscRoomInfo *room_info = NULL;
        char tmp_date[64] = {0};
        char tmp_content[64] = {0};
        char body[128] = {0};
        time_t nowtime;
        struct tm * timeinfo = NULL;
        hscExeStatus exe_status = HSC_ERROR;
        const char *tmp_body = NULL;

        if ((addr=this->hscGetSinAddr()) == NULL || (room_info=this->hscGetRoomInfo()) == NULL)
        {
            return MIDCODE_ERROR;
        }

        cgilog(SYSLOG_DEBUG, "step 4: HSC Server response message ");

        memset(room_info->res_buf, '\0', PMS_HSC_BUFSIZE);

        /*get current date and time*/
        time (&nowtime);
        timeinfo = localtime (&nowtime);
        strftime (tmp_date, sizeof(tmp_date) - 1, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", timeinfo);

        /*get body content*/
        if (room_info->method == HSC_CHANGE_METHOD)
        {
            tmp_body = room_info->req_body;
        }
        else
        {
            hscAddResponseJsonContent(room_info);
            tmp_body = json_object_to_json_string(room_info->res_body);
        }
        if (tmp_body)
        {
            snprintf(body, sizeof(body)-1, "%s", tmp_body);
            len = strlen(body);
            snprintf(body+strlen(body), sizeof(body)-strlen(body)-1, "%s", HSC_SPLIT_MARK);
        }

        /*get body content length*/
        snprintf(tmp_content, sizeof(tmp_content)-1, "Content-Length: %d\r\n", len);

        /*add hsc response headers content*/
        exe_status = this->hscGetExecuteStatus();
        switch (exe_status)
        {
            case HSC_SUCCESS:
                HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_STATUS_200);
                break;
            case HSC_AUTH_ERR:
                HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_STATUS_401);
                break;
            case HSC_NO_EXTENSION:
                HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_STATUS_404);
                break;
            case HSC_NAMR_INVALID:
            case HSC_CFU_INVALID:
                HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_STATUS_406);
                break;
            default:
                HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_STATUS_500);
                break;
        }

        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, tmp_date);
        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_SERVER);
        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, tmp_content);
        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_CONN_TYPE);
        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_CONTENT_JSON);
        HSC_ADD_HEADERS_CONTENT(room_info->res_buf, HSC_SPLIT_MARK);

        HSC_ADD_BODY_CONTENT(room_info->res_buf, body);

        cgilog(SYSLOG_DEBUG, "%s", room_info->res_buf);
        hscSendResponse(addr, room_info);

        return MIDCODE_SUCCESS;
    }

    int pmsHscBase::pmsConnectInit()
    {
        hscAddr *addr = NULL;
        if ( (addr = (hscAddr *)malloc(sizeof(hscAddr))) == NULL)
        {
            cgilog(SYSLOG_ERR, "create hsc address fail!");
            return -1;
        }

        hscInitAddr(addr);
        addr->local.sin_port=htons(this->m_server_port);

        if((addr->sockfd = hscCreateSocket(addr)) < 0)
        {
            cgilog(SYSLOG_ERR, "create socket fail!");
            return -1;
        }

        hscSetSocketOption(addr);
        if (hscBind(addr) < 0)
        {
            cgilog(SYSLOG_ERR, "bind address fail!");
            close(addr->sockfd);
            return -1;
        }

        if (hscListen(addr) < 0)
        {
            cgilog(SYSLOG_ERR, "listen socket fail!");
            close(addr->sockfd);
            return -1;
        }
        this->hsc_addr = addr;
        fcntl(addr->sockfd, F_SETFL, O_NONBLOCK);

        return 0;
    }

    int pmsHscBase::pmsConnectExit()
    {
        hscAddr *addr = NULL;
        if ((addr=this->hscGetSinAddr()) != NULL)
        {
            close(addr->sockfd);
        }
        HSC_SAFE_FREE(addr);

        return 0;
    }

    void pmsHscBase::pmsServerHandler()
    {
        int ret = MIDCODE_ERROR;
        hscAddr *addr = NULL;

        if ((addr=this->hscGetSinAddr()) != NULL)
        {
            addr->remote_sockfd = hscAccept(addr);
            if (addr->remote_sockfd > 0)
            {
                cgilog(SYSLOG_INFO, ">>>>>>>>Enter HSC message handle ... ...");
                this->hscCreateRoomInfo();
                this->hscSetExecuteStatus(HSC_ERROR);
                /***step one: receive message***/
                ret = this->pmsServerMsgReceive();

                /***step two: parse and check message***/
                if (MIDCODE_SUCCESS == ret)
                {
                    ret = this->pmsServerMsgParse();
                }
                if (MIDCODE_SUCCESS == ret)
                {
                    ret = this->pmsServerMsgCheck();
                }

                /***step three: update correct message***/
                if (MIDCODE_SUCCESS == ret)
                {
                    ret = this->pmsServerMsgExec();
                }

                if (MIDCODE_SUCCESS == ret)
                {
                    this->hscSetExecuteStatus(HSC_SUCCESS);
                }

                /***step four: response execute status***/
                ret = this->pmsServerMsgResponse();

                this->hscFreeRoomInfo();
                close(addr->remote_sockfd);
                cgilog(SYSLOG_INFO, ">>>>>>>>handle HSC message done !!!");
            }
            else if(addr->remote_sockfd == -1)
            {
                ms_sleep(500);
            }
            else
            {
                ms_sleep(500);
            }
        }
    }

}

