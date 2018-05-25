/****************************************************************************
 *
 * FILENAME:        $RCSfile: mcm.h,v $
 *
 * LAST REVISION:   $Revision: 1.2 $
 * LAST MODIFIED:   $Date: 2017/11/21 09:24:42 $
 *
 * DESCRIPTION:
 *
 * vi: set ts=4:
 *
 * Copyright (c) 2012-2014 by Grandstream Networks, Inc.
 * All rights reserved.
 *
 * This material is proprietary to Grandstream Networks, Inc. and,
 * in addition to the above mentioned Copyright, may be
 * subject to protection under other intellectual property
 * regimes, including patents, trade secrets, designs and/or
 * trademarks.
 *
 * Any use of this material for any purpose, except with an
 * express license from Grandstream Networks, Inc. is strictly
 * prohibited.
 *
 ***************************************************************************/

#ifndef __MCM_H__
#define __MCM_H__

#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

typedef enum _MCM_TYPE
{
    MCM_TYPE_CONF_CONTROL = 0,
    MCM_TYPE_CONF_RELOAD,
    MCM_TYPE_CONF_RESERVATION,
    MCM_TYPE_GSWAVE_LOGIN,
    MCM_TYPE_TOTAL
}MCM_TYPE;

typedef enum _MCM_ACTION
{
    MCM_ACTION_InviteUser = 0,
    MCM_ACTION_MuteUserAudio,
    MCM_ACTION_UnmuteUserAudio,
    MCM_ACTION_PauseUserVideo,
    MCM_ACTION_PlayUserVideo,
    MCM_ACTION_AdjustVideoRate,
    MCM_ACTION_KickUser,
    MCM_ACTION_LockRoom,
    MCM_ACTION_UnLockRoom,
    MCM_ACTION_InviteRoom,
    MCM_ACTION_Authenticate,
    MCM_ACTION_TransferHost,
    MCM_ACTION_KickExtension,
    MCM_ACTION_Commad,
    MCM_ACTION_TOTAL
}MCM_ACTION;

typedef enum
{
    MCM_STATUS_CODE_SUCCESS                 = 0,  /*Success*/
    MCM_STATUS_CODE_FAILURE                 = 1,  /*Failure*/
    MCM_STATUS_CODE_ERROR_FORMAT            = 2,  /*Format Error*/
    MCM_STATUS_CODE_ERROR_NO_PERMISSION     = 3,  /*No Permission*/
    MCM_STATUS_CODE_ERROR_NO_RESOURCE       = 4,  /*No Resource*/
    MCM_STATUS_CODE_ERROR_EXIST             = 5,  /*Existed*/
}MCM_STATUS_CODE;

typedef enum _MCM_METHOD
{
    MCM_METHOD_REQUEST = 0,
    MCM_METHOD_RESPONSE,
    MCM_METHOD_EVENT,
    MCM_METHOD_TOTAL
}MCM_METHOD;

typedef struct _mcmTask
{
    char *buffer;
    char * req_buf;
    char * res_buf;
    struct json_object * body;
    struct json_object * msg;
    MCM_METHOD method;
    MCM_ACTION actType;
    int req_len;
    int res_len;
    int body_len;
}mcmTask;

typedef enum _mcm_network_domain
{
    MCM_AF_UNSPEC = AF_UNSPEC,
    MCM_AF_INET   = AF_INET,
    MCM_AF_INET6  = AF_INET6,
    MCM_AF_UNIX   = AF_UNIX,
} mcmNetworkDomain;

typedef enum _mcm_network_type
{
    MCM_SOCK_STREAM     = SOCK_STREAM,
    MCM_SOCK_DGRAM      = SOCK_DGRAM,
} mcmNetworkType;

typedef struct _mcmClientAddr {
    struct sockaddr_un srv_addr;
    int srv_addr_len;
    mcmNetworkDomain domain;
    mcmNetworkType type;
    int protocol;
    int clientfd;
} mcmClientAddr;

#define MCM_BUF_MSG_SIZE                      (1024)

#define MCM_HEAD_CONTENT_LEN                 "Content-Length: %d\r\n"
#define MCM_HEAD_SPLIT_MARK                     "\r\n"
#define MCM_HEAD_STRING_FORMAT            "%s"
#define MCM_HEAD_NAME_CONTENT             "Content-Length"

#define MCM_MSG_JSON_TYPE                   "type"
#define MCM_MSG_JSON_MESSAGE          "message"

#define MCM_AGR_NAME_ACTION                             "action"
#define MCM_AGR_NAME_ACTION_ID                      "action_id"
#define MCM_AGR_NAME_CONF_NUMBER             "conf_number"
#define MCM_AGR_NAME_USER_ID                           "user_id"
#define MCM_AGR_NAME_USER                                  "user"
#define MCM_AGR_NAME_VIDEO_RATE                   "video_rate"
#define MCM_AGR_NAME_CMD                                     "cmd"

#define MCM_EXE_CONFIGURE_RELOAD                 "configuration reload"
#define MCM_EXE_CONF_RESERVATION                 "conference reservation"

#define MCM_CONNECT_SERVER_ADDR                  "/tmp/mcmman.socket"

#define MCM_FLAG_CR                                  0x0D
#define MCM_FLAG_LF                                   0x0A
#define MCM_MSG_BLANK_LINE               "\r\n\r\n"

#define MCM_ADD_HEADERS_CONTENT(array, format, arg)   \
                snprintf(array + strlen(array), sizeof(array) - strlen(array), format, arg)

#define MCM_ADD_MSG(point, format, arg)   \
                snprintf(point + strlen(point), MCM_BUF_MSG_SIZE - strlen(point), format, arg)

#define MCM_SAFE_FREE(tmp)   if (tmp){ free(tmp); tmp = NULL;}
#define MCM_JSON_SAFE_FREE(tmp)   if (tmp){ json_object_put(tmp);; tmp = NULL;}

#define MCM_RECV_MSG_FINISH(flag_recv)   if (flag_recv){ break;}

int mcmRequestMessage(mcmTask * task);
int mcmProcResponseMessage(mcmTask * task);
int mcmCreateBodyContent(mcmTask* task);
int mcmCreateRequestContent(mcmTask* task);
int mcmGetActionId(void);
void mcmGetActionType(mcmTask *task);
int mcmAddConfParameter(char * buf, struct json_object* msg, char * key);
int mcmAddConfKeyByParameter(char * buf, struct json_object* msg, char * key, char *param);
char * mcmActionType2String(MCM_ACTION actionType);
mcmTask * mcmTaskCreate();
void mcmCleanTask(mcmTask * task);

int mcmProcConfCtlMsg(mcmTask * task);
int mcmProcConfReloadMsg(mcmTask * task);
int mcmProcConfReservationMsg(mcmTask * task);
int mcmProcessAction(mcmTask * task);

int mcmHandleMessage(void *data, MCM_TYPE type);
char *mcmJsonContentParse(char *json_string, char *key);

int mcmProcOneExtenMsg(mcmTask * task);

#endif
