#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "app_hsc.h"

using namespace pmsHscBaseNamespace;

const char *hsc_head_key[] = {
    "Connection",
    "Content-Type",
    "Content-Length",
    "Host",
    "Accept",
    "User-Agent",
    "Authorization"
};

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int hscInitAddr(hscAddr *addr)
{
    struct sockaddr_in *local_addr = NULL;

    memset(addr,0,sizeof(*addr));
    addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_domain = AF_INET;
    addr->sin_type = SOCK_STREAM;
    addr->sin_protocol = 0;

    local_addr = &(addr->local);
    local_addr->sin_family=AF_INET;
    local_addr->sin_addr.s_addr=INADDR_ANY;

    return 0;
}

int hscCreateSocket(hscAddr *addr)
{
    return socket(addr->sin_domain,addr->sin_type,addr->sin_protocol);
}

void hscSetSocketOption(hscAddr *addr)
{
    int optval=1;
    //struct timeval timeout = {0, 10};

    setsockopt(addr->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    //setsockopt(addr->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    //setsockopt(addr->sockfd, SOL_SOCKET, SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
}

int hscBind(hscAddr *addr)
{
    int len = sizeof(struct sockaddr);
    return bind(addr->sockfd, (struct sockaddr *)&addr->local, len);
}

int hscListen(hscAddr *addr)
{
    return listen(addr->sockfd,PMS_HSC_LISTEN_LEN);
}

int hscAccept(hscAddr *addr)
{
    return accept(addr->sockfd, (struct sockaddr *)&addr->remote, &(addr->sin_len));
}

ssize_t hscReceiveRequest(hscAddr *addr)
{
    return recv(addr->remote_sockfd, addr->buf, sizeof(addr->buf), 0);
}

int hscSendResponse(hscAddr *addr, hscRoomInfo *room_info)
{
    return send(addr->remote_sockfd, room_info->res_buf, strlen(room_info->res_buf), 0);
}

/***
 Function: get request header the value of key
 Notes: free return pointer
***/
char *hscGetContentByKey(const char *source, const char *key)
{
    int len = 0 ;
    int http_end_flag = 0;
    char buf[HSC_HEAD_BUF_LEN] = {0};
    const char *ptr_start = NULL;
    char *tmp = NULL;
    char *next =NULL;

    if (source == NULL || key == NULL)
    {
        return NULL;
    }

    if ((ptr_start=strstr(source, key)) == NULL)
    {
        return NULL;
    }
    ptr_start = ptr_start + strlen(key) + HSC_KEY_SPLIT_MARK_LEN;

    tmp = (char *)ptr_start;
    while (tmp && tmp != source && len < HSC_HEAD_BUF_LEN)
    {
        next = tmp +1;
        if (*tmp == HSC_HTTP_END_FLAG_CR && *next == HSC_HTTP_END_FLAG_LF)
        {
            http_end_flag = 1;
            break;
        }
        buf[len++] = *tmp;
        tmp++;
    }

    if (!http_end_flag)
    {
        return NULL;
    }
    //cgilog(SYSLOG_DEBUG, "key: %s, value: %s ", key, buf);
    tmp = buf;

    return strdup(tmp);
}

/***free return pointer***/
char *hscGetbodyContent(const char *source)
{
    int len = 0 ;
    char buf[HSC_HEAD_BUF_LEN] = {0};
    const char *ptr_start = NULL;
    char *tmp = NULL;

    if (source == NULL)
    {
        return NULL;
    }

    if ((ptr_start=strstr(source, HSC_HTTP_BLANK_LINE)) == NULL)
    {
        return NULL;
    }
    ptr_start = ptr_start + strlen(HSC_HTTP_BLANK_LINE);

    tmp = (char *)ptr_start;
    while (tmp && *tmp != '\0' && len < HSC_HEAD_BUF_LEN)
    {
        buf[len++] = *tmp;
        tmp++;
    }
    //cgilog(SYSLOG_DEBUG, "%s\n", buf);
    tmp = buf;

    return strdup(tmp);
}

/***free return pointer***/
char *hscGetRetrieveContent(char *header)
{
    int len = 0 ;
    char buf[HSC_HEAD_BUF_LEN] = {0};
    char *ptr_start = NULL;
    char *tmp = NULL;
    char retrieve_start = '?';
    char retrieve_end = ' ';

    if (header == NULL)
    {
        return NULL;
    }
    if ((ptr_start=strchr(header, retrieve_start)) == NULL)
    {
        cgilog(SYSLOG_DEBUG, "receive retrieve content error !");
        return NULL;
    }
    ptr_start = ptr_start + 1;

    tmp = ptr_start;
    while (tmp && *tmp != retrieve_end && len < HSC_HEAD_BUF_LEN)
    {
        buf[len++] = *tmp;
        tmp++;
    }
    cgilog(SYSLOG_DEBUG, "retrieve content: %s!", buf);
    tmp = buf;

    return strdup(tmp);
}

int hscIsReceiveDataFinish(char *receive)
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

    tmp_body = hscGetbodyContent(receive);
    tmp = hscGetContentByKey(receive, hsc_head_key[HSC_HEADER_CONTENT_LEN]);
    if (tmp && tmp_body)
    {
        body_len = strlen(tmp_body);
        content_len = atoi(tmp);
        cgilog(SYSLOG_INFO, "body content: %s \n", tmp_body);
        cgilog(SYSLOG_INFO, "client request content length: %d, received body length: %d  !\n", content_len, body_len);
        if (content_len == body_len)
        {
            receive_finish = 1;
        }
    }
    HSC_SAFE_FREE(tmp_body);
    HSC_SAFE_FREE(tmp);

    return receive_finish;
}

/***get request method type***/
hscRequestMethod hscGetRequestMethodType(char *header)
{
    int len = 0 ;
    int http_end_flag = 0;
    char buf[HSC_HEAD_BUF_LEN] = {0};
    char *tmp = NULL;
    char *next =NULL;
    hscRequestMethod method = HSC_NONE_METHOD;

    if (header == NULL)
    {
        return method;
    }

    tmp = header;
    while (tmp && *tmp != '\0' && len < HSC_HEAD_BUF_LEN)
    {
        next = tmp +1;
        if (*tmp == ' ' && *next == '/')
        {
            http_end_flag = 1;
            break;
        }
        buf[len++] = *tmp;
        tmp++;
    }

    if (!http_end_flag)
    {
        return method;
    }

    cgilog(SYSLOG_DEBUG, "hsc client request method is: %s!", buf);
    if (strcasecmp(buf, HSC_RETRIEVE_ACTION) == 0)
    {
        method = HSC_RETRIEVE_METHOD;
    }
    else if (strcasecmp(buf, HSC_CHANGE_ACTION) == 0)
    {
        method = HSC_CHANGE_METHOD;
    }
    else
    {
        method = HSC_NONE_METHOD;
    }

    return method;
}

/***free return pointer***/
char *hscJsonContentParse(char *json_string, char *key)
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

int hscAddResponseJsonContent(hscRoomInfo *room)
{
    if (room == NULL)
    {
        return -1;
    }
    if (room->res_body != NULL)
    {
        json_object_put(room->res_body);
    }

    room->res_body = json_object_new_object();
    json_object_object_add(room->res_body, HSC_PARAMETER_EXTENSION, json_object_new_string(room->extension));
    if (room->param.key)
    {
        json_object_object_add(room->res_body, room->param.key, json_object_new_string(room->param.value == NULL ? "" : room->param.value));
    }

    return 0;
}

char *base64_encode(unsigned char *bindata, char *base64, int binlength)
{
    int i, j;
    unsigned char current;

    for ( i = 0, j = 0 ; i < binlength ; i += 3 )
    {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
        if ( i + 1 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
        if ( i + 2 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return base64;
}

int base64_decode(char *base64, unsigned char *bindata)
{
    int i, j;
    unsigned char k;
    unsigned char temp[4];
    for ( i = 0, j = 0; base64[i] != '\0' ; i += 4 )
    {
        memset( temp, 0xFF, sizeof(temp) );
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i] )
                temp[0]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+1] )
                temp[1]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+2] )
                temp[2]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+3] )
                temp[3]= k;
        }

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2))&0xFC)) |
                ((unsigned char)((unsigned char)(temp[1]>>4)&0x03));
        if ( base64[i+2] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4))&0xF0)) |
                ((unsigned char)((unsigned char)(temp[2]>>2)&0x0F));
        if ( base64[i+3] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6))&0xF0)) |
                ((unsigned char)(temp[3]&0x3F));
    }
    return j;
}

int hscCheckAuthorization(hscRoomInfo *room)
{
    int len = 0;
    char *tmp = NULL;
    char original[256] = {0};
    char generate[512] = {0};
    char *base64encode = NULL;
    char *tmp_generate = NULL;
    unsigned char *tmp_original = NULL;

    tmp_original = (unsigned char *)original;
    tmp_generate = (char *)generate;

    /***test generate base 64 encode***/
    snprintf(original, sizeof(original)-1, "admin:admin155%%");
    len = strlen(original);
    base64_encode(tmp_original, tmp_generate, len);
    cgilog(SYSLOG_DEBUG,"test generate base 64 encode is: %s", generate);

    if (room)
    {
        tmp = hscGetContentByKey(room->req_buf, hsc_head_key[HSC_HEADER_AUTH]);
        if (tmp && ((base64encode = strstr(tmp, "Basic")) != NULL))
        {
            /***clean test generate encode***/
            memset(generate, '\0', sizeof(generate));

            /***get username and password***/
            snprintf(generate, sizeof(generate)-1, "%s", base64encode+6);
            cgilog(SYSLOG_DEBUG, "hsc client request base 64 encode is: %s", generate);
            base64_decode(tmp_generate, tmp_original);
            cgilog(SYSLOG_DEBUG,"hsc original data is: %s", original);
        }
        HSC_SAFE_FREE(tmp);
    }

    return 0;
}
