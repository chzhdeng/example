/****************************************************************************
 *
 * FILENAME:        $RCSfile: mcm_manager_event.h,v $
 *
 * LAST REVISION:   $Revision: 1.2 $
 * LAST MODIFIED:   $Date: 2018/02/28 09:24:42 $
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

#ifndef __MCM_MANAGER_EVENT_H__
#define __MCM_MANAGER_EVENT_H__

#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "cgilog.h"
#include "global.h"
#include "database.h"

#define MCM_STACK_SIZE                             (1 * 1024 * 1024)
#define MCM_MGR_BUF_SIZE                       (2*1024)
#define MCM_RECV_BUF_SIZE                      (1024)

typedef enum _MCM_EVENT_TYPE
{
    MCM_EVENT_Start = 0,
    MCM_EVENT_End,
    MCM_EVENT_Lock,
    MCM_EVENT_UnLock,
    MCM_EVENT_Join,
    MCM_EVENT_Leave,
    MCM_EVENT_Mute,
    MCM_EVENT_Unmute,
    MCM_EVENT_Pause,
    MCM_EVENT_Resume,
    MCM_EVENT_Total
}MCM_EVENT_TYPE;

typedef struct _mcmEvent
{
    char *buffer;                                               //receive mcm manager event information
    MCM_EVENT_TYPE type;                      //mcm manager event type
    char *msg;                                                   //mcm conference message
    int c_config_db;                                        //connect status database
    int c_status_db;                                        //connect status database
    int status;                                                   //status
}mcmEvent;

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

typedef struct _mcmConnectAddress {
    struct sockaddr_un srv_addr;
    int srv_addr_len;
    mcmNetworkDomain domain;
    mcmNetworkType type;
    int protocol;
    int clientfd;
    FILE *fp;
    char buffer[MCM_MGR_BUF_SIZE];
} mcmConnectAddress;

#define ENABLE_MCM_EVENT                      1
#define DISABLE_MCM_EVENT                     0

#define MCM_HEAD_CONTENT_LEN                 "Content-Length: %d\r\n"
#define MCM_HEAD_SPLIT_MARK                     "\r\n"
#define MCM_HEAD_STRING_FORMAT            "%s"
#define MCM_HEAD_NAME_CONTENT             "Content-Length"

#define MCM_MSG_TYPE_REQUEST                           "request"
#define MCM_MSG_TYPE_RESPONSE                         "response"
#define MCM_MSG_TYPE_EVENT                                 "event"

#define MCM_MSG_NAME_TYPE                                  "type"
#define MCM_MSG_NAME_MESSAGE                         "message"

#define MCM_MSG_NAME_ACTION                             "action"
#define MCM_MSG_NAME_ACTION_ID                      "action_id"
#define MCM_MSG_NAME_CONF_NUMBER             "conf_number"
#define MCM_MSG_NAME_USER_ID                           "user_id"
#define MCM_MSG_NAME_USER                                  "user"
#define MCM_MSG_NAME_VIDEO_RATE                  "video_rate"
#define MCM_MSG_NAME_VALUE                               "value"
#define MCM_MSG_NAME_EVENT                               "event"

#define MCM_MSG_NAME_MEMBER_NUM             "caller_number"
#define MCM_MSG_NAME_MEMBER_NAME           "caller_name"
#define MCM_MSG_NAME_ADMIN                              "admin"
#define MCM_MSG_NAME_MEMBER_ID                   "caller_uuid"

#define MCM_TABLE_CONF_STATUS                               "AMIST_multimedia_conference_status"
#define MCM_TABLE_CONF_MEMBERS_STATUS       "AMIST_multimedia_conference_members_status"

#define MCM_EVENT_CONNECT_ADDR                  "/tmp/mcmman.socket"

#define MCM_FLAG_CR                                  0x0D
#define MCM_FLAG_LF                                   0x0A
#define MCM_MSG_BLANK_LINE               "\r\n\r\n"

#define MCM_ADD_HEADERS_CONTENT(array, format, arg)   \
                snprintf(array + strlen(array), sizeof(array) - strlen(array), format, arg)

#define MCM_ADD_MSG(point, format, arg)   \
                snprintf(point + strlen(point), MCM_MGR_BUF_SIZE - strlen(point), format, arg)

#define MCM_SAFE_FREE(tmp)   if (tmp){ free(tmp); tmp = NULL;}
#define MCM_JSON_SAFE_FREE(tmp)   if (tmp){ json_object_put(tmp); tmp = NULL;}

#define MCM_PROCESSES_ENABLE    "/cfg/var/lib/asterisk/scripts/safe_media_server.sh &"
#define MCM_PROCESSES_DISABLE   "/cfg/var/lib/asterisk/scripts/stop_media_server.sh &"

void mcmStart();
void mcmStop();
mcmEvent *createMcmEvent();
void cleanMcmEvent(mcmEvent * event);
char * mcmEventType2String(MCM_EVENT_TYPE type);
char * addMcmSubscribeMessage();

void mcmEventClientInit(mcmConnectAddress * addr);
int mcmEventConnect(mcmConnectAddress * addr);
int mcmEventLogin(mcmConnectAddress *addr);
void mcmEventLogout(mcmConnectAddress *addr);
char *parseKeyByMcmEvent(char *json_string, char *key);
void getMcmEventType(mcmEvent * e);

int procMcmStartEvent(mcmEvent * e);
int procMcmEndEvent(mcmEvent * e);
int procMcmLockEvent(mcmEvent * e);
int procMcmUnLockEvent(mcmEvent * e);
int procMcmJoinEvent(mcmEvent * e);
int procMcmLeaveEvent(mcmEvent * e);
int procMcmMuteEvent(mcmEvent * e);
int procMcmUnmuteEvent(mcmEvent * e);
int procMcmPauseEvent(mcmEvent * e);
int procMcmResumeEvent(mcmEvent * e);
int procMcmEvent(mcmConnectAddress *addr, int config_db, int status_db);

int getMcmEventEnable();
void setMcmEventEnable(int status);
int receiveMcmManagerMessage(mcmConnectAddress *addr);

void mcmEventStart();
void mcmEventStop();

#endif
