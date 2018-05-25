#ifndef __APP_HSC_H__
#define __APP_HSC_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "pms_global.h"

#define PMS_HSC_BUFSIZE 1024
#define PMS_HSC_LISTEN_LEN 10

typedef enum
{
    HSC_RETRIEVE_METHOD = 0,
    HSC_CHANGE_METHOD,
    HSC_NONE_METHOD,
}hscRequestMethod;

typedef enum
{
    HSC_ACTION_NAME = 0,
    HSC_ACTION_PERMISSION,
    HSC_ACTION_MWI,
    HSC_ACTION_DND,
    HSC_ACTION_CFWT,
    HSC_ACTION_TOTAL,
}hscAction;

typedef enum
{
    HSC_SUCCESS = 0,     //200
    HSC_AUTH_ERR,        //401
    HSC_NO_EXTENSION,   //404
    HSC_NAMR_INVALID,   //416
    HSC_CFU_INVALID,     //416
    HSC_ERROR,            //500
}hscExeStatus;

typedef struct _hscParameter {
    char *key;
    char *value;
} hscParameter;

typedef struct _hscAddr {
    struct sockaddr_in local;
    struct sockaddr_in remote;
    socklen_t sin_len;
    int sin_domain;
    int sin_type;
    int sin_protocol;
    int sockfd;
    int remote_sockfd;
    char buf[512];
} hscAddr;

typedef struct
{
    char extension[PMS_MAX_DATA];
    char req_buf[PMS_HSC_BUFSIZE];            /*store HSC Client request data*/
    char res_buf[PMS_HSC_BUFSIZE];            /*store HSC Server response data*/
    char *req_body;                              /*request body content*/
    struct json_object* res_body;                  /*receive HSC Client request data*/
    int req_body_len;                             /*receive HSC Client request data*/
    int res_body_len;                             /*receive HSC Client request data*/
    char* res_describe;                           /*receive HSC Client request data*/
    int  res_status;                               /*receive HSC Client request data*/
    hscRequestMethod method;                    /*receive HSC Client request data*/
    hscAction action;
    hscParameter param;
}hscRoomInfo;

namespace pmsHscBaseNamespace
{
    using namespace pmsBaseNamespace;

    #define HSC_HTTP_END_FLAG_CR         0x0D
    #define HSC_HTTP_END_FLAG_LF         0x0A
    #define HSC_HTTP_BLANK_LINE           "\r\n\r\n"

    #define HSC_KEY_SPLIT_MARK_LEN       2
    #define HSC_HEAD_BUF_LEN              256

    #define HSC_RETRIEVE_ACTION             "GET"
    #define HSC_CHANGE_ACTION              "POST"

    #define HSC_PARAMETER_EXTENSION          "extension"
    #define HSC_PARAMETER_NAME                "name"
    #define HSC_PARAMETER_PERMISSION         "permission"
    #define HSC_PARAMETER_MWI                  "mwi"
    #define HSC_PARAMETER_DND                  "dnd"
    #define HSC_PARAMETER_CFWT                "cfwt"

    /***hsc response headers***/
    #define HSC_STATUS_200           "HTTP/1.0 200 OK\r\n"
    #define HSC_STATUS_401           "HTTP/1.0 401 Unauthorized\r\n"
    #define HSC_STATUS_404           "HTTP/1.0 404 Extension not found\r\n"
    #define HSC_STATUS_406           "HTTP/1.0 406 Request data error\r\n"
    #define HSC_STATUS_500           "HTTP/1.0 500 Error text\r\n"

    #define HSC_SERVER                "Server: Pbxmid\r\n"
    #define HSC_CONN_TYPE            "Connection: close\r\n"
    #define HSC_CONTENT_JSON        "Content-Type: application/json\r\n"
    #define HSC_SPLIT_MARK            "\r\n"

    typedef enum
    {
        HSC_HEADER_CONNECTION = 0,
        HSC_HEADER_CONTENT_TYPE,
        HSC_HEADER_CONTENT_LEN,
        HSC_HEADER_HOST,
        HSC_HEADER_ACCEPT,
        HSC_HEADER_USER_AGENT,
        HSC_HEADER_AUTH,
        HSC_HEADER_TOTAL,
    }hscHeadKey;

    class pmsHscBase : public pmsBase
    {
        public:
            pmsHscBase();
            ~pmsHscBase();

            PMSTask *hscServerTaskCreate();
            void hscServerTaskFree();

            void hscCreateRoomInfo();
            void hscFreeRoomInfo();
            hscRoomInfo *hscGetRoomInfo();
            hscAddr *hscGetSinAddr();
            void hscSetExecuteStatus(hscExeStatus status);
            hscExeStatus hscGetExecuteStatus();

            void hscAddRequestTask(PMSTask *task);
            PMSTask *hscGetRequestTask();
            int hscServerMsgAddToTask();

            /***hsc server parse message***/
            int hscServerMsgParseForRetrieve();
            int hscServerMsgParseForChange();
            hscAction hscServerGetAction(char *value);

            /***hsc server check message***/
            //int hscServerMsgCheckExten();
            int hscServerMsgCheckName();
            int hscServerMsgCheckPermission();
            int hscServerMsgCheckMWI();
            int hscServerMsgCheckDND();
            int hscServerMsgCheckCfwt();
            int hscServerMsgCheckAuthorization();

            int hscServerMsgExecute();

            int pmsConnectInit();
            void pmsServerHandler();
            int pmsServerMsgReceive();
            int pmsServerMsgParse();
            int pmsServerMsgCheck();
            int pmsServerMsgExec();
            int pmsServerMsgResponse();
            int pmsConnectExit();

        private:
            hscAddr *hsc_addr;
            hscRoomInfo *hsc_room_info;
            PMSTask *hsc_server_task;
            hscExeStatus hsc_status;
    };
}

#define HSC_SAFE_FREE(tmp)   if (tmp){ free(tmp); tmp = NULL;}
#define ms_sleep(ms)   usleep(ms*1000)
#define HSC_RECV_MSG_FINISH(flag_recv)   if (flag_recv){ break;}
#define HSC_ADD_PARSE_AGR(array, arg)   snprintf(array, sizeof(array) - 1, "%s", arg)
#define HSC_ADD_HEADERS_CONTENT(array, arg)   snprintf(array + strlen(array), sizeof(array) - strlen(array), "%s", arg)
#define HSC_ADD_BODY_CONTENT(array, arg)   snprintf(array + strlen(array), sizeof(array) - strlen(array) - 1, "%s", arg)

int hscInitAddr(hscAddr *addr);
int hscCreateSocket(hscAddr *addr);
void hscSetSocketOption(hscAddr *addr);
int hscBind(hscAddr *addr);
int hscListen(hscAddr *addr);
int hscAccept(hscAddr *addr);
ssize_t hscReceiveRequest(hscAddr *addr);
int hscSendResponse(hscAddr *addr, hscRoomInfo *room_info);
char *hscGetContentByKey(const char *source, const char *key);
char *hscGetbodyContent(const char *source);
int hscIsReceiveDataFinish(char *receive);
hscRequestMethod hscGetRequestMethodType(char *header);
char *hscGetRetrieveContent(char *header);
char *hscJsonContentParse(char *json_string, char *key);
int hscAddResponseJsonContent(hscRoomInfo *room);
char *base64_encode(unsigned char *bindata, char *base64, int binlength);
int base64_decode(char *base64, unsigned char *bindata);
int hscCheckAuthorization(hscRoomInfo *room);

#endif

