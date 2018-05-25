#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#ifdef MEMWATCH
    #include "memwatch.h"
#endif

#include "mcm_manager_event.h"

static pthread_t mcm_event_phread;
static int is_mcm_event_running = 0;

static char *mcm_event[MCM_EVENT_Total] = {
    "MCMStart",
    "MCMEnd",
    "MCMLock",
    "MCMUnLock",
    "MCMJoin",
    "MCMLeave",
    "MCMMute",
    "MCMUnmute",
    "MCMPause",
    "MCMResume"
};

void mcmStart()
{
    cgilog(SYSLOG_DEBUG, "Start processes of MCM and AVS !");
    system(MCM_PROCESSES_ENABLE);
}

void mcmStop()
{
    cgilog(SYSLOG_DEBUG, "kill processes of MCM and AVS !");
    system(MCM_PROCESSES_DISABLE);
}

mcmEvent * createMcmEvent()
{
    mcmEvent *event = NULL;

    if ( (event = (mcmEvent *)malloc(sizeof(mcmEvent))) == NULL)
    {
        cgilog(SYSLOG_ERR, "mcm create event error.");
        return NULL;
    }

    memset(event, 0, sizeof(mcmEvent));

    event->buffer = NULL;

    return event;
}

void cleanMcmEvent(mcmEvent * e)
{
    if (e)
    {
        MCM_SAFE_FREE(e->buffer);
        MCM_SAFE_FREE(e->msg);
    }
    MCM_SAFE_FREE(e);
}

char * mcmEventType2String(MCM_EVENT_TYPE type)
{
    if (type >= MCM_EVENT_Total)
    {
        return "Invalid";
    }

    return mcm_event[type];
}

/*
{"type": "request","message": {"action": "xxx","action_id": "xxx","xxx_xxx": "xxx"}}
*/
char * addMcmSubscribeMessage()
{
    int len = 0;
    struct json_object* msg = NULL;
    struct json_object* body = NULL;
    char *tmp = NULL;
    char head_content[256] = {0};
    char request[512] = {0};

    msg = json_object_new_object();
    if (msg)
    {
        json_object_object_add(msg, MCM_MSG_NAME_ACTION, json_object_new_string("WaitEvent"));
        json_object_object_add(msg, MCM_MSG_NAME_ACTION_ID, json_object_new_string("65535"));
        json_object_object_add(msg, MCM_MSG_NAME_VALUE, json_object_new_string("1"));
    }
    else
    {
        return NULL;
    }

    body = json_object_new_object();
    if (body)
    {
        json_object_object_add(body, MCM_MSG_NAME_TYPE, json_object_new_string("request"));
        json_object_object_add(body, MCM_MSG_NAME_MESSAGE, json_object_get(msg));

        len = strlen(json_object_get_string(body));
        cgilog(SYSLOG_DEBUG, "The length of body is: %d ", len);
        cgilog(SYSLOG_DEBUG, "The content of body is: %s ", json_object_get_string(body));
    }
    else
    {
        MCM_JSON_SAFE_FREE(msg);
        return NULL;
    }

    MCM_ADD_HEADERS_CONTENT(head_content, MCM_HEAD_CONTENT_LEN, len);
    MCM_ADD_HEADERS_CONTENT(head_content, MCM_HEAD_STRING_FORMAT, MCM_HEAD_SPLIT_MARK);

   MCM_ADD_MSG(request, MCM_HEAD_STRING_FORMAT, head_content);
   MCM_ADD_MSG(request, MCM_HEAD_STRING_FORMAT, json_object_get_string(body));
   tmp = request;
   MCM_JSON_SAFE_FREE(body);

   return strdup(tmp);
}

void mcmEventClientInit(mcmConnectAddress * addr)
{
    if (addr == NULL)
    {
        return ;
    }

    addr->domain = MCM_AF_UNIX;
    addr->type = MCM_SOCK_STREAM;
    addr->protocol = 0;
    addr->clientfd = -1;
    addr->fp = NULL;
    memset(addr->buffer, 0, sizeof(addr->buffer));

    /*set  server addr param*/
    addr->srv_addr.sun_family = MCM_AF_UNIX;
    addr->srv_addr_len = sizeof(addr->srv_addr);
    strncpy(addr->srv_addr.sun_path, MCM_EVENT_CONNECT_ADDR, sizeof(addr->srv_addr.sun_path) - 1);
}

int mcmEventConnect(mcmConnectAddress * addr)
{
    int ret = -1;
    if (addr == NULL)
    {
        return ret;
    }

    addr->clientfd = socket(addr->domain, addr->type, addr->protocol);
    if (addr->clientfd > 0)
    {
        ret = connect(addr->clientfd, (struct sockaddr *)&(addr->srv_addr), addr->srv_addr_len);
        addr->fp = fdopen(addr->clientfd, "w+");
        if (addr->fp)
        {
            setvbuf(addr->fp, NULL, _IONBF, 0);
        }
    }

    return ret;
}

int mcmEventLogin(mcmConnectAddress *addr)
{
    int len = 0;
    int ret = MIDCODE_ERROR;
    char *message = NULL;

    if (addr == NULL)
    {
        return MIDCODE_ERROR;
    }

    mcmEventClientInit(addr);
    ret = mcmEventConnect(addr);
    if (ret == -1)
    {
        cgilog(SYSLOG_ERR, "connnect mcm  module fail !");
        mcmEventLogout(addr);
        return MIDCODE_ERROR;
    }

    message = addMcmSubscribeMessage();
    if (message == NULL)
    {
        mcmEventLogout(addr);
        return MIDCODE_ERROR;
    }
    len = strlen(message);
    cgilog(SYSLOG_DEBUG, "subscribe message: %s, len: %d ", message, len);

    if (write(addr->clientfd, message, len) < 0)
    {
        cgilog(SYSLOG_ERR, "send message fail !");
        mcmEventLogout(addr);
        ret = MIDCODE_ERROR;
    }
    else
    {
        ret = MIDCODE_SUCCESS;
        cgilog(SYSLOG_DEBUG, "Subscribe mcm event success through fd[%d]", addr->clientfd);
    }
    MCM_SAFE_FREE(message);

    return ret;
}

void mcmEventLogout(mcmConnectAddress *addr)
{
    if (addr)
    {
        if (addr->fp)
        {
            fflush(addr->fp);
            fclose(addr->fp);
            addr->fp = NULL;
            addr->clientfd = -1;
        }
        else if (addr->clientfd != -1)
        {
            close(addr->clientfd);
            addr->clientfd = -1;
        }
    }
}

/***free return pointer***/
char *parseKeyByMcmEvent(char *json_string, char *key)
{
    const char *tmp = NULL;
    char *value = NULL;
    struct json_object *json = NULL;
    struct json_object *json_key = NULL;

    if (json_string == NULL || key == NULL)
    {
        return NULL;
    }

    json = json_tokener_parse(json_string);
    if (is_error(json))
    {
        return NULL;
    }

    if (json_object_is_type(json, json_type_object))
    {
        json_key = json_object_object_get(json, key);
        if (json_key)
        {
            tmp = json_object_get_string(json_key);
            if(tmp)
            {
                //cgilog(SYSLOG_DEBUG, "parse [%s] is [%s] !", key, tmp);
                value = strdup(tmp);
            }
        }
    }
    json_object_put(json);

    return value;
}

void getMcmEventType(mcmEvent * e)
{
    int i = 0;
    char *tmp_event = NULL;
    char *msg = NULL;

    if (e == NULL || e->buffer == NULL)
    {
        return ;
    }

    msg = parseKeyByMcmEvent(e->buffer, MCM_MSG_NAME_MESSAGE);
    if (msg == NULL)
    {
        return ;
    }
    e->msg = msg;

    tmp_event = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_EVENT);
    if (tmp_event == NULL)
    {
        return ;
    }

    for ( i = 0; i < MCM_EVENT_Total; i++ )
    {
        if ( 0 == strcasecmp(tmp_event, mcm_event[i]) )
        {
            break;
        }
    }

    if ( i < MCM_EVENT_Total )
    {
        e->type = (MCM_EVENT_TYPE)i;
    }
    else
    {
        e->type = MCM_EVENT_Total;
    }

    MCM_SAFE_FREE(tmp_event);
}


//{"type":"event","message":{"event":"MCMStart","conf_number":"6300"}}
int procMcmStartEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};
    char tmp_time[256] = {0};
    time_t nowtime;
    struct tm * timeinfo = NULL;

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    if (conf == NULL)
    {
        return ret;
    }

    time (&nowtime);
    timeinfo = localtime (&nowtime);
    strftime (tmp_time, sizeof(tmp_time) - 1, "%Y-%m-%d %H:%M", timeinfo);

    snprintf(cmd, sizeof(cmd)-1, "INSERT INTO %s (conf_number, start_time) VALUES ('%s', '%s');",
                                MCM_TABLE_CONF_STATUS, conf, tmp_time);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    return ret;
}

//{"type":"event","message":{"event":"MCMEnd","conf_number":"6300"}}
int procMcmEndEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    if (conf == NULL)
    {
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "DELETE FROM %s WHERE conf_number = %s;",
                                MCM_TABLE_CONF_STATUS, conf);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    return ret;
}

//{"type":"event","message":{"event":"MCMLock","conf_number":"6300"}}
int procMcmLockEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    if (conf == NULL)
    {
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_locked = 'yes' WHERE conf_number = %s;",
                                MCM_TABLE_CONF_STATUS, conf);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    return ret;
}

//{"type":"event","message":{"event":"MCMUnLock","conf_number":"6300"}}
int procMcmUnLockEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    if (conf == NULL)
    {
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_locked = 'no' WHERE conf_number = %s;",
                                MCM_TABLE_CONF_STATUS, conf);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    return ret;
}

//{"type":"event","message":{"event":"MCMJoin","conf_number":"6300", "caller_uuid":"1000_00001", "caller_number":"1000","caller_name":"John","admin":"yes/no"}}
int procMcmJoinEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};
    char tmp_time[256] = {0};
    time_t nowtime;
    struct tm * timeinfo = NULL;

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *member_num = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_NUM);
    char *member_name = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_NAME);
    char *is_admin = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_ADMIN);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || member_num == NULL || member_name == NULL  ||
        is_admin == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(member_num);
        MCM_SAFE_FREE(member_name);
        MCM_SAFE_FREE(is_admin);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    time (&nowtime);
    timeinfo = localtime (&nowtime);
    strftime (tmp_time, sizeof(tmp_time) - 1, "%Y-%m-%d %H:%M", timeinfo);

    snprintf(cmd, sizeof(cmd)-1, "INSERT INTO %s (conf_number,member_number,member_name,join_time,is_admin,user_id) VALUES ('%s','%s','%s',' %s','%s','%s');",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, member_num, member_name, tmp_time, is_admin, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(member_num);
    MCM_SAFE_FREE(member_name);
    MCM_SAFE_FREE(is_admin);
    MCM_SAFE_FREE(user_id);

    return ret;
}

//{"type":"event","message":{"event":"MCMLeave","conf_number":"6300", "caller_uuid":"1000_00001","caller_number":"1000","admin":"yes/no"}}
int procMcmLeaveEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "DELETE FROM %s WHERE conf_number = '%s' AND user_id = '%s';",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(user_id);

    return ret;
}

//{"type":"event","message":{"event":"MCMMute","conf_number":"6300", "caller_uuid":"1000_00001","caller_number":"1000","admin":"yes/no"}}
int procMcmMuteEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_audio_muted = 'yes' WHERE conf_number = '%s' AND user_id = '%s';",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(user_id);

    return ret;
}

//{"type":"event","message":{"event":"MCMUnmute","conf_number":"6300","caller_uuid":"1000_00001","caller_number":"1000","admin":"yes/no"}}
int procMcmUnmuteEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_audio_muted = 'no' WHERE conf_number = '%s' AND user_id = '%s';",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(user_id);

    return ret;
}

//{"type":"event","message":{"event":"MCMPause","conf_number":"6300", "caller_uuid":"1000_00001","caller_number":"1000","admin":"yes/no"}}
int procMcmPauseEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_video_pause = 'yes' WHERE conf_number = '%s' AND user_id = '%s';",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(user_id);

    return ret;
}

//{"type":"event","message":{"event":"MCMResume","conf_number":"6300", "caller_uuid":"1000_00001","caller_number":"1000","admin":"yes/no"}}
int procMcmResumeEvent(mcmEvent * e)
{
    int ret = MIDCODE_ERROR;
    char cmd[512] = {0};

    if (e == NULL)
    {
        return ret;
    }

    char *conf = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_CONF_NUMBER);
    char *user_id = parseKeyByMcmEvent(e->msg, MCM_MSG_NAME_MEMBER_ID);

    if (conf == NULL || user_id == NULL)
    {
        MCM_SAFE_FREE(conf);
        MCM_SAFE_FREE(user_id);
        return ret;
    }

    snprintf(cmd, sizeof(cmd)-1, "UPDATE %s SET is_video_pause = 'no' WHERE conf_number = '%s' AND user_id = '%s';",
                                MCM_TABLE_CONF_MEMBERS_STATUS, conf, user_id);
    cgilog(SYSLOG_DEBUG, "execute cmd: %s", cmd);
    ret = runSQLCmd(cmd, e->c_status_db);

    MCM_SAFE_FREE(conf);
    MCM_SAFE_FREE(user_id);

    return ret;
}

int procMcmEvent(mcmConnectAddress *addr, int config_db, int status_db)
{
    int ret = MIDCODE_ERROR;
    mcmEvent * event = NULL;
    char * msg_type = NULL;

    if (addr == NULL)
    {
        return ret;
    }

    cgilog(SYSLOG_DEBUG, "Process mcm event: %s", addr->buffer);

    msg_type = parseKeyByMcmEvent(addr->buffer, MCM_MSG_NAME_TYPE);
    if (msg_type)
    {
        if (strcasecmp(msg_type, MCM_MSG_TYPE_REQUEST) == 0 ||
                strcasecmp(msg_type, MCM_MSG_TYPE_RESPONSE) == 0)
        {
            MCM_SAFE_FREE(msg_type);
            cgilog(SYSLOG_DEBUG, "There is no need to do anything !");

            return MIDCODE_SUCCESS;
        }
        else
        {
            //handle event message
            MCM_SAFE_FREE(msg_type);
        }
    }

    event = createMcmEvent();
    if (event == NULL)
    {
        return ret;
    }
    event->buffer = strdup(addr->buffer);
    event->c_config_db = config_db;
    event->c_status_db = status_db;

    getMcmEventType(event);
    switch (event->type)
    {
        case MCM_EVENT_Start:
            procMcmStartEvent(event);
            break;
        case MCM_EVENT_End:
            procMcmEndEvent(event);
            break;
        case MCM_EVENT_Lock:
            procMcmLockEvent(event);
            break;
        case MCM_EVENT_UnLock:
            procMcmUnLockEvent(event);
            break;
        case MCM_EVENT_Join:
            procMcmJoinEvent(event);
            break;
        case MCM_EVENT_Leave:
            procMcmLeaveEvent(event);
            break;
        case MCM_EVENT_Mute:
            procMcmMuteEvent(event);
            break;
        case MCM_EVENT_Unmute:
            procMcmUnmuteEvent(event);
            break;
        case MCM_EVENT_Pause:
            procMcmPauseEvent(event);
            break;
        case MCM_EVENT_Resume:
            procMcmResumeEvent(event);
            break;
        default:
            cgilog(SYSLOG_ERR, "mcm response conference action fail!");
            ret = MIDCODE_ERROR;
            break;
    }
    cleanMcmEvent(event);

    return ret;
}

int getMcmEventEnable()
{
    return is_mcm_event_running;
}

void setMcmEventEnable(int status)
{
    is_mcm_event_running = status;
}

int receiveMcmManagerMessage(mcmConnectAddress *addr)
{
    int ret = 0;
    int real_len = 0;
    int recv_len = 0;
    int surplus_len = 0;
    char buf[MCM_RECV_BUF_SIZE] = {0};

    while (addr && fgets(buf, sizeof(buf), addr->fp))
    {
        if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n"))
        {
            break;
        }
        else if (!strncasecmp(buf, "Content-Length:", strlen("Content-Length:")))
        {
            real_len = atoi(buf + strlen("Content-Length:"));
        }
    }

    if (real_len == 0 || real_len > MCM_MGR_BUF_SIZE)
    {
        return ret;
    }
    cgilog(SYSLOG_INFO, "the content length: %d \n", real_len);

    memset(addr->buffer, 0, sizeof(addr->buffer));
    while ((surplus_len = real_len - recv_len) > 0)
    {
        ret = read(addr->clientfd, buf, (surplus_len > MCM_RECV_BUF_SIZE ? MCM_RECV_BUF_SIZE : surplus_len));
        if (ret <= 0)
        {
            break;
        }
        else
        {
            snprintf(addr->buffer+strlen(addr->buffer), sizeof(addr->buffer)-strlen(addr->buffer), "%s", buf);
            recv_len = strlen(addr->buffer);
        }
    }
    ret = strlen(addr->buffer);
    cgilog(SYSLOG_INFO, "receive message: %s\n", addr->buffer);

    return ret;
}

static void *mcmManagerEvent()
{
    int ret = MIDCODE_ERROR;
    mcmConnectAddress mcm_addr;
    fd_set rset;
    struct timeval timeout = {3,0};

    cgilog(SYSLOG_DEBUG, "Enter mcm manager events handle ... ... !");

    mcmEventClientInit(&mcm_addr);

    connectConfigDB(PBXMID_UCM_CONFIG_DB, __FUNCTION__);
    connectConfigDB(PBXMID_AMI_STATUS_DB, __FUNCTION__);

    while (is_mcm_event_running)
    {
        if (mcm_addr.clientfd == -1)
        {
            mcmEventLogin(&mcm_addr);
            if (mcm_addr.clientfd == -1)
            {
                sleep(1);
                continue;
            }
        }

        FD_ZERO(&rset);
        FD_SET(mcm_addr.clientfd, &rset);

        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        ret = select(mcm_addr.clientfd + 1, &rset, NULL, NULL, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(mcm_addr.clientfd, &rset))
            {
                ret = receiveMcmManagerMessage(&mcm_addr);
                if (ret > 0)
                {
                    procMcmEvent(&mcm_addr, PBXMID_UCM_CONFIG_DB, PBXMID_AMI_STATUS_DB);
                }
                else
                {
                    mcmEventLogout(&mcm_addr);
                    sleep(1);
                }
            }
        }
        else if (ret == 0)
        {
            //cgilog(SYSLOG_DEBUG, "receive message timeout !");
            continue;
        }
        else
        {
            cgilog(SYSLOG_ERR, "mcm process: socket select error(%s)", strerror(errno));
            mcmEventLogout(&mcm_addr);
            sleep(1);
        }
    }
    mcmEventLogout(&mcm_addr);

    disconnectDB(PBXMID_UCM_CONFIG_DB);
    disconnectDB(PBXMID_AMI_STATUS_DB);

    return NULL;
}

void mcmEventStart()
{
    pthread_attr_t mcm_event_monitor;

    setMcmEventEnable(ENABLE_MCM_EVENT);

    pthread_attr_init(&mcm_event_monitor);
    pthread_attr_setstacksize((&mcm_event_monitor), MCM_STACK_SIZE);
    pthread_attr_setdetachstate((&mcm_event_monitor), PTHREAD_CREATE_JOINABLE);
    pthread_create(&mcm_event_phread, &mcm_event_monitor, (void* (*)(void*))mcmManagerEvent, NULL);
}

void mcmEventStop()
{
    if (getMcmEventEnable())
    {
        setMcmEventEnable(DISABLE_MCM_EVENT);
        pthread_join(mcm_event_phread, NULL);
    }
}
