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

#include "global.h"
#include "mcm.h"
#include "cgilog.h"
#include "database.h"

static int  mcm_action_id = 0;

static char *mcm_action_str[MCM_ACTION_TOTAL] = {
    "InviteUser",
    "MuteUserAudio",
    "UnmuteUserAudio",
    "PauseUserVideo",
    "PlayUserVideo",
    "AdjustVideoRate",
    "KickUser",
    "LockRoom",
    "UnlockRoom",
    "InviteRoom",
    "Authenticate",
    "TransferHost",
    "KickExtension",
    "Command"
};

static char *mcm_method[MCM_METHOD_TOTAL] = {
    "request",
    "response",
    "event"
};

WebCgiCode mcmStatusCode2WebCgiCode(MCM_STATUS_CODE status)
{
    WebCgiCode code = CGICODE_ERROR;

    switch (status)
    {
        case MCM_STATUS_CODE_SUCCESS:
            code = CGICODE_SUCCESS;
            break;
        case MCM_STATUS_CODE_FAILURE:
            code = CGICODE_ERROR;
            break;
        case MCM_STATUS_CODE_ERROR_FORMAT:
            code = CGICODE_INVALID_PARAM;
            break;
        case MCM_STATUS_CODE_ERROR_NO_PERMISSION:
            code = CGICODE_ACTION_NO_PERMISSION;
            break;
        case MCM_STATUS_CODE_ERROR_NO_RESOURCE:
            code = CGICODE_ERROR;
            break;
        case MCM_STATUS_CODE_ERROR_EXIST:
            code = CGICODE_NUMBER_EXIST;
            break;
        default:
            code = CGICODE_ERROR;
            break;
    }

    return code;
}

mcmTask * mcmTaskCreate()
{
    mcmTask *task = NULL;

    if ( (task = (mcmTask *)malloc(sizeof(mcmTask))) == NULL)
    {
        cgilog(SYSLOG_ERR, "mcm create task error.");
        return NULL;
    }

    memset(task, 0, sizeof(mcmTask));
    task->method = MCM_METHOD_REQUEST;

    task->buffer = NULL;
    task->msg = NULL;
    task->body = NULL;
    task->req_buf = NULL;
    task->req_len = MCM_BUF_MSG_SIZE;
    task->res_len = MCM_BUF_MSG_SIZE;
    task->body_len = 0;

    return task;
}

void mcmCleanTask(mcmTask * task)
{
    if (task)
    {
        MCM_JSON_SAFE_FREE(task->msg);
        MCM_JSON_SAFE_FREE(task->body);

        MCM_SAFE_FREE(task->buffer);
        MCM_SAFE_FREE(task->req_buf);
        MCM_SAFE_FREE(task->res_buf);

        MCM_SAFE_FREE(task);
    }
}

void mcmClientInit(mcmClientAddr * addr)
{
    if (addr == NULL)
    {
        return ;
    }

    addr->domain = MCM_AF_UNIX;
    addr->type = MCM_SOCK_STREAM;
    addr->protocol = 0;
    addr->clientfd = -1;

    /*set  server addr param*/
    addr->srv_addr.sun_family = MCM_AF_UNIX;
    addr->srv_addr_len = sizeof(addr->srv_addr);
    strncpy(addr->srv_addr.sun_path, MCM_CONNECT_SERVER_ADDR, sizeof(addr->srv_addr.sun_path) - 1);
}

int mcmConnectServer(mcmClientAddr * addr)
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
    }

    return ret;
}

/***free return pointer***/
char *mcmGetbodyContent(const char *source)
{
    int len = 0 ;
    char buf[256] = {0};
    const char *ptr_start = NULL;
    char *tmp = NULL;

    if (source == NULL)
    {
        return NULL;
    }

    if ((ptr_start=strstr(source, MCM_MSG_BLANK_LINE)) == NULL)
    {
        return NULL;
    }
    ptr_start = ptr_start + strlen(MCM_MSG_BLANK_LINE);

    tmp = (char *)ptr_start;
    while (tmp && *tmp != '\0' && len < 256)
    {
        buf[len++] = *tmp;
        tmp++;
    }
    //cgilog(SYSLOG_DEBUG, "Response body content: %s !", buf);
    tmp = buf;

    return strdup(tmp);
}

char *mcmGetContentLength(const char *source)
{
    int len = 0 ;
    int mcm_end_flag = 0;
    char buf[256] = {0};
    const char *ptr_start = NULL;
    char *tmp = NULL;
    char *next =NULL;

    if (source == NULL)
    {
        return NULL;
    }

    if ((ptr_start=strstr(source, MCM_HEAD_NAME_CONTENT)) == NULL)
    {
        return NULL;
    }
    ptr_start = ptr_start + strlen(MCM_HEAD_NAME_CONTENT) + 1;

    tmp = (char *)ptr_start;
    while (tmp && tmp != source && len < 256)
    {
        next = tmp +1;
        if (*tmp == MCM_FLAG_CR && *next == MCM_FLAG_LF)
        {
            mcm_end_flag = 1;
            break;
        }
        buf[len++] = *tmp;
        tmp++;
    }

    if (!mcm_end_flag)
    {
        return NULL;
    }
    //cgilog(SYSLOG_DEBUG, "Response body content length: %s !", buf);
    tmp = buf;

    return strdup(tmp);
}

int mcmIsReceiveFinish(char *receive)
{
    char *tmp = NULL;
    char *tmp_body = NULL;
    int content_len = 0;
    int body_len = 0;
    int receive_finish = 0;
    if (receive == NULL)
    {
        return receive_finish;
    }

    tmp_body = mcmGetbodyContent(receive);
    tmp = mcmGetContentLength(receive);
    if (tmp && tmp_body)
    {
        body_len = strlen(tmp_body);
        content_len = atoi(tmp);
        cgilog(SYSLOG_INFO, "the parse content length: %d, the real received length: %d  !\n", content_len, body_len);
        if (content_len == body_len)
        {
            receive_finish = 1;
        }
    }
    MCM_SAFE_FREE(tmp_body);
    MCM_SAFE_FREE(tmp);

    return receive_finish;
}

int mcmRequestMessage(mcmTask * task)
{
    int ret = CGICODE_ERROR;
    fd_set rset;
    struct timeval timeout = {3,0};
    mcmClientAddr mcm_addr;
    char buffer[1024] = {0};
    int recv_finish = 0;

    if (task == NULL || task->req_buf == NULL)
    {
        return ret;
    }

    mcmClientInit(&mcm_addr);
    ret = mcmConnectServer(&mcm_addr);
    if (ret == -1)
    {
        cgilog(SYSLOG_ERR, "connnect mcm  module fail !");
        close(mcm_addr.clientfd);
        return CGICODE_ERROR;
    }

    cgilog(SYSLOG_DEBUG, "Trying send request message: %s", task->req_buf);
    if (write(mcm_addr.clientfd, task->req_buf, task->req_len) < 0)
    {
        cgilog(SYSLOG_ERR, "connnect mcm  module fail !");
        close(mcm_addr.clientfd);
        return CGICODE_ERROR;
    }

    FD_ZERO(&rset);
    FD_SET(mcm_addr.clientfd, &rset);

    while (1)
    {
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        ret = select(mcm_addr.clientfd + 1, &rset, NULL, NULL, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(mcm_addr.clientfd, &rset))
            {
                ret = read(mcm_addr.clientfd, (buffer+strlen(buffer)), (sizeof(buffer)-strlen(buffer)-1));
                if (ret > 0)
                {
                    if (mcmIsReceiveFinish(buffer))
                    {
                        task->res_buf = mcmGetbodyContent(buffer);
                        ret = CGICODE_SUCCESS;
                        recv_finish = 1;
                        cgilog(SYSLOG_DEBUG, "receive all message is: %s", buffer);
                    }
                }
                else
                {
                    ret = CGICODE_ERROR;
                    recv_finish = 1;
                }
            }
        }
        else if (ret == 0)
        {
            cgilog(SYSLOG_ERR, "receive message timeout !");
            ret = CGICODE_ERROR;
            recv_finish = 1;
        }
        else
        {
            cgilog(SYSLOG_ERR, "request message error !");
            ret = CGICODE_ERROR;
            recv_finish = 1;
        }
        MCM_RECV_MSG_FINISH(recv_finish);
    }

    close(mcm_addr.clientfd);

    return ret;
}

/***free return pointer***/
char *mcmJsonContentParse(char *json_string, char *key)
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


int mcmProcResponseMessage(mcmTask * task)
{
    int ret = CGICODE_ERROR;
    int status = CGICODE_ERROR;
    char *msg = NULL;
    char *status_code = NULL;
    char *description = NULL;

    if (task == NULL || task->res_buf == NULL)
    {
        return ret;
    }

    /*** {"type":"response","message":{"action_id":"8","status_code":"0","description":"success"}}  ***/
    cgilog(SYSLOG_DEBUG, "handle response message : %s !", task->res_buf);
    msg = mcmJsonContentParse(task->res_buf, "message");
    if (msg)
    {
        status_code = mcmJsonContentParse(msg, "status_code");
        description = mcmJsonContentParse(msg, "description");

        if (status_code && description)
        {
            status = atoi(status_code);
            cgilog(SYSLOG_DEBUG, "response status : %d , response description: %s ", status, description);
            switch (task->actType)
            {
                case MCM_ACTION_InviteUser:
                case MCM_ACTION_MuteUserAudio:
                case MCM_ACTION_UnmuteUserAudio:
                case MCM_ACTION_PauseUserVideo:
                case MCM_ACTION_PlayUserVideo:
                case MCM_ACTION_AdjustVideoRate:
                case MCM_ACTION_KickUser:
                case MCM_ACTION_LockRoom:
                case MCM_ACTION_UnLockRoom:
                case MCM_ACTION_InviteRoom:
                case MCM_ACTION_TransferHost:
                case MCM_ACTION_Commad:
                case MCM_ACTION_Authenticate:
                case MCM_ACTION_KickExtension:
                    ret = mcmStatusCode2WebCgiCode(status);
                    break;
                default:
                    cgilog(SYSLOG_ERR, "mcm response conference action fail!");
                    ret = CGICODE_ERROR;
                    break;
            }
        }
    }
    SAFE_FREE(msg);
    SAFE_FREE(status_code);
    SAFE_FREE(description);

    return ret;
}

/*
{"type": "request","message": {"action": "xxx","action_id": "xxx","xxx_xxx": "xxx"}}
*/
int mcmCreateBodyContent(mcmTask* task)
{
    int ret = CGICODE_ERROR;
    char action_id[32] = {0};
    struct json_object* mcm_body = NULL;

    if (task == NULL ||task->msg == NULL ||
        task->method >= MCM_METHOD_TOTAL ||task->actType >= MCM_ACTION_TOTAL)
    {
        return ret;
    }

    /*** add action and action_id***/
    snprintf(action_id, sizeof(action_id)-1, "%d", mcmGetActionId());
    json_object_object_add(task->msg, MCM_AGR_NAME_ACTION, json_object_new_string(mcm_action_str[task->actType]));
    json_object_object_add(task->msg, MCM_AGR_NAME_ACTION_ID, json_object_new_string(action_id));

    /***add body message***/
    mcm_body = json_object_new_object();
    if (mcm_body)
    {
        json_object_object_add(mcm_body, MCM_MSG_JSON_TYPE, json_object_new_string(mcm_method[task->method]));
        json_object_object_add(mcm_body, MCM_MSG_JSON_MESSAGE, json_object_get(task->msg));

        task->body_len = strlen(json_object_get_string(mcm_body));
        cgilog(SYSLOG_DEBUG, "The length of body is: %d ", task->body_len);
        cgilog(SYSLOG_DEBUG, "The content of body is: %s ", json_object_get_string(mcm_body));

        ret = CGICODE_SUCCESS;
    }
    else
    {
        ret = CGICODE_ERROR;
        cgilog(SYSLOG_ERR, "create mcm message failure !!!");
    }
    task->body = mcm_body;

    return ret;
}

int mcmCreateRequestContent(mcmTask* task)
{
    int ret = CGICODE_ERROR;
    char head_content[512] = {0};

    if (task == NULL)
    {
        return ret;
    }

    if ((task->req_buf = (char *)malloc(MCM_BUF_MSG_SIZE)) == NULL)
    {
        return ret;
    }
    memset(task->req_buf, 0, MCM_BUF_MSG_SIZE);

    ret = mcmCreateBodyContent(task);
    if (ret != CGICODE_SUCCESS)
    {
        return ret;
    }

    MCM_ADD_HEADERS_CONTENT(head_content, MCM_HEAD_CONTENT_LEN, task->body_len);
    MCM_ADD_HEADERS_CONTENT(head_content, MCM_HEAD_STRING_FORMAT, MCM_HEAD_SPLIT_MARK);

   MCM_ADD_MSG(task->req_buf, MCM_HEAD_STRING_FORMAT, head_content);
   MCM_ADD_MSG(task->req_buf, MCM_HEAD_STRING_FORMAT, json_object_get_string(task->body));
   task->req_len = strlen(task->req_buf);

   cgilog(SYSLOG_DEBUG, "request message is: %s",  task->req_buf);

   return ret;
}

int mcmGetActionId(void)
{
    int action_id = 0;
    if (mcm_action_id >= 65536)
    {
        mcm_action_id = 0;
    }
    action_id = mcm_action_id;
    mcm_action_id++;

    return action_id;
}

void mcmGetActionType(mcmTask *task)
{
    int i = 0;
    char *action = NULL;
    if (task == NULL || task->buffer == NULL)
    {
        return ;
    }

    action = parseQuery(task->buffer, MCM_AGR_NAME_ACTION);
    if (action == NULL)
    {
        return ;
    }

    for ( i = 0; i < MCM_ACTION_TOTAL; i++ )
    {
        if ( 0 == strcasecmp(action, mcm_action_str[i]) )
        {
            break;
        }
    }

    if ( i < MCM_ACTION_TOTAL )
    {
        task->actType = i;
    }
    else
    {
        task->actType = MCM_ACTION_TOTAL;
    }

    free(action);
}

int mcmAddConfParameter(char * buf, struct json_object* msg, char * key)
{
    int ret = CGICODE_ERROR;
    char * value = NULL;

    if (buf == NULL ||msg == NULL || key == NULL)
    {
        return ret;
    }

    value = parseQuery(buf, key);
    if (value)
    {
        cgilog(SYSLOG_DEBUG, "parse result is {%s: %s}", key, value);
        json_object_object_add(msg, key, json_object_new_string(value));
        free(value);
        ret = CGICODE_SUCCESS;
    }

    return ret;
}

int mcmAddConfKeyByParameter(char * buf, struct json_object* msg, char * key, char *param)
{
    int ret = CGICODE_ERROR;
    char * value = NULL;

    if (buf == NULL ||msg == NULL || key == NULL || param == NULL)
    {
        return ret;
    }

    value = parseQuery(buf, param);
    if (value)
    {
        cgilog(SYSLOG_DEBUG, "parse result is {%s: %s}", key, value);
        json_object_object_add(msg, key, json_object_new_string(value));
        free(value);
        ret = CGICODE_SUCCESS;
    }

    return ret;
}

char * mcmActionType2String(MCM_ACTION actionType)
{
    if (actionType >= MCM_ACTION_TOTAL)
    {
        return "Invalid";
    }

    return mcm_action_str[actionType];
}

/*!
action=InviteUser&conf_number=8201&users=5001&module-type=mcm
action=MuteUserAudio&conf_number=8201&user_id=1000
action=UnmuteUserAudio&conf_number=8201&user_id=1000
action=PauseUserVideo&conf_number=8201&user_id=1000
action=PlayUserVideo&conf_number=8201&user_id=1000
action=AdjustVideoRate&conf_number=8201&user_id=1000
action=KickUser&conf_number=8201&user_id=2000
action=LockRoom&conf_number=8201
action=UnlockRoom&conf_number=8201
*/
int mcmProcConfCtlMsg(mcmTask *task)
{
    int   ret = CGICODE_ERROR;
    struct json_object* msg = NULL;

    if (task == NULL  || task->buffer == NULL || task->actType >= MCM_ACTION_TOTAL)
    {
        return ret;
    }

    msg = json_object_new_object();
    if (msg == NULL)
    {
        return ret;
    }

    cgilog(SYSLOG_DEBUG, "parse mcm action: %s", mcmActionType2String(task->actType));
    switch (task->actType)
    {
        case MCM_ACTION_InviteUser:
            ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_CONF_NUMBER);
            if (ret == CGICODE_SUCCESS)
            {
                ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_USER);
            }
            break;
        case MCM_ACTION_MuteUserAudio:
        case MCM_ACTION_UnmuteUserAudio:
        case MCM_ACTION_PauseUserVideo:
        case MCM_ACTION_PlayUserVideo:
        case MCM_ACTION_KickUser:
        case MCM_ACTION_TransferHost:
            ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_CONF_NUMBER);
            if (ret == CGICODE_SUCCESS)
            {
                ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_USER_ID);
            }
            break;
        case MCM_ACTION_LockRoom:
        case MCM_ACTION_UnLockRoom:
            ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_CONF_NUMBER);
            break;
        case MCM_ACTION_AdjustVideoRate:
            ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_CONF_NUMBER);
            if (ret == CGICODE_SUCCESS)
            {
                ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_USER_ID);
            }
            if (ret == CGICODE_SUCCESS)
            {
                ret = mcmAddConfParameter(task->buffer, msg, MCM_AGR_NAME_VIDEO_RATE);
            }
            break;
        default:
            cgilog(SYSLOG_ERR, "mcm request conference action fail!");
            ret = CGICODE_ERROR;
            break;
    }

    if (ret != CGICODE_SUCCESS)
    {
        json_object_put(msg);
        msg = NULL;
        cgilog(SYSLOG_ERR, "add msg fail !");
    }
    task->msg = msg;

    return ret;
}

int mcmProcConfReloadMsg(mcmTask * task)
{
    int ret = CGICODE_ERROR;
    struct json_object* msg = NULL;

    if (task == NULL)
    {
        return ret;
    }

    msg = json_object_new_object();
    if (msg)
    {
        json_object_object_add(msg, MCM_AGR_NAME_CMD, json_object_new_string(MCM_EXE_CONFIGURE_RELOAD));
        task->msg = msg;
        ret = CGICODE_SUCCESS;
    }

    return ret;
}

int mcmProcConfReservationMsg(mcmTask * task)
{
    int ret = CGICODE_ERROR;
    struct json_object* msg = NULL;

    if (task == NULL)
    {
        return ret;
    }

    msg = json_object_new_object();
    if (msg)
    {
        ret = mcmAddConfKeyByParameter(task->buffer, msg, "book_id", "schedule_id");
        if (ret == CGICODE_SUCCESS)
        {
            ret = mcmAddConfKeyByParameter(task->buffer, msg, "user_id", "user");
        }

        if (ret != CGICODE_SUCCESS)
        {
            json_object_put(msg);
            msg = NULL;
        }
        task->msg = msg;
    }

    return ret;
}

int mcmProcOneExtenMsg(mcmTask * task)
{
    int ret = CGICODE_ERROR;
    struct json_object* msg = NULL;

    if (task == NULL)
    {
        return ret;
    }

    msg = json_object_new_object();
    if (msg)
    {
        ret = mcmAddConfKeyByParameter(task->buffer, msg, "extension", "user");
        if (ret == CGICODE_SUCCESS)
        {
            json_object_object_add(msg, "type", json_object_new_string("gswave"));
        }

        if (ret != CGICODE_SUCCESS)
        {
            json_object_put(msg);
            msg = NULL;
        }
        task->msg = msg;
    }

    return ret;
}

int mcmProcessAction(mcmTask * task)
{
    int ret = CGICODE_ERROR;

    if (task == NULL)
    {
        return ret;
    }
    cgilog(SYSLOG_DEBUG, "Enter mcm process ... ...");

    ret = mcmCreateRequestContent(task);

    if (ret == CGICODE_SUCCESS)
    {
        ret = mcmRequestMessage(task);
    }

    if (ret == CGICODE_SUCCESS)
    {
        ret = mcmProcResponseMessage(task);
    }

    return ret;
}

int mcmHandleMessage(void *data, MCM_TYPE type)
{
    int ret =CGICODE_ERROR;
    Task * tmp = (Task *)data;
    mcmTask * mcm_task = NULL;

    if (tmp == NULL  || tmp->buffer == NULL)
    {
        return ret;
    }

    mcm_task = mcmTaskCreate();
    if (mcm_task == NULL)
    {
        return ret;
    }
    mcm_task->buffer = strDup( tmp->buffer);
    cgilog(SYSLOG_DEBUG, "mcm buffer is: %s", mcm_task->buffer);

    switch(type)
    {
        case MCM_TYPE_CONF_CONTROL:
            mcmGetActionType(mcm_task);
            ret = mcmProcConfCtlMsg(mcm_task);
            break;
        case MCM_TYPE_CONF_RELOAD:
            mcm_task->actType = MCM_ACTION_Commad;
            ret = mcmProcConfReloadMsg(mcm_task);
            break;
        case MCM_TYPE_CONF_RESERVATION:
            mcm_task->actType = MCM_ACTION_Authenticate;
            ret = mcmProcConfReservationMsg(mcm_task);
            break;
        case MCM_TYPE_GSWAVE_LOGIN:
            mcm_task->actType = MCM_ACTION_KickExtension;
            ret = mcmProcOneExtenMsg(mcm_task);
            break;
        default:
            cgilog(SYSLOG_ERR, "request conference type fail!");
            break;
    }

    cgilog(SYSLOG_DEBUG, "handle message result: %d", ret);
    if (ret == CGICODE_SUCCESS)
    {
        ret = mcmProcessAction(mcm_task);
    }

    mcmCleanTask(mcm_task);

    return ret;
}

