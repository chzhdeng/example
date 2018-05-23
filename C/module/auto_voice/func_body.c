/*** MODULEINFO
    <depend>sqlite3</depend>
    <depend>msc</depend>
    <depend>rt</depend>
    <support_level>core</support_level>
 ***/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <regex.h>
#include <ctype.h>
#include <regex.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 328209 $")

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/localtime.h"
#include "asterisk/test.h"
#include "asterisk/file.h"
#include "asterisk/dsp.h"
#include "asterisk/pinyin.h"


/*** DOCUMENTATION
    <function name="BODY" language="en_US">
        <synopsis>
            Read file.
        </synopsis>
        <syntax>
            <parameter name="string" required="true"/>
        </syntax>
        <description>
            <para>Example: ${BODY(/opt/sound/body.out)} returns strings</para>
        </description>
    </function>
 ***/

#define     MAXSIZE     65535
#define     MAX_SIZE    65535
#define     MIN_SIZE    1024
#define     MALLOC      40

#define     SQLITE3_PATH    "/cfg/etc/ucm_config.db"

#include <msc/qtts.h>
#include <msc/msp_cmn.h>
#include <msc/msp_errors.h>

typedef int SR_DWORD;
typedef short int SR_WORD ;

typedef struct Readname
{
    char *extensions;
    char *readfilename;
    float similar;
    struct Readname *next;
}Readname;

typedef struct message
{
    char *extensions;
    char *fullname;
    float similar;
    struct message *next;
}Msg;

/*
    *********************************************************************
*/
typedef struct store_message
{
    char *extensions;
    char *fullname;
    float similar;
    int determin;
    struct store_message *next;
}Store;

struct wave_pcm_hdr
{
    char            riff[4];                        // = "RIFF"
    SR_DWORD        size_8;                         // = FileSize - 8
    char            wave[4];                        // = "WAVE"
    char            fmt[4];                         // = "fmt "
    SR_DWORD        dwFmtSize;                      // =  The size of the next structure : 16

    SR_WORD         format_tag;              // = PCM : 1
    SR_WORD         channels;                // = channel : 1
    SR_DWORD        samples_per_sec;        // = Sampling Rate : 8000 | 6000 | 11025 | 16000
    SR_DWORD        avg_bytes_per_sec;      // = The number of bytes per second : dwSamplesPerSec * wBitsPerSample / 8
    SR_WORD         block_align;            // = The number of bytes per sampling point : wBitsPerSample / 8
    SR_WORD         bits_per_sample;         // = Quantization bit number: 8 | 16

    char            data[4];                // = "data";
    SR_DWORD        data_size;              // = Pure Data Length : FileSize - 44 
} ;

/*
    *********************************************************************
*/

enum
{
    FALSE       =      -1,
    ZERO        =       0,
    SUCCESS     =       1,
    NUMBER      =       2,
    STRING      =       3,
    LEN_1       =       6,
    LEN_2       =       9,
    LEN_3       =       12
};

#define HZ2PY_UTF8_CHECK_LENGTH 20
#define HZ2PY_INPUT_BUF_ARRAY_SIZE 1024
#define HZ2PY_OUTPUT_BUF_ARRAY_SIZE 5120
#define HZ2PY_STR_COPY(to, from, count) \
    ok = 1;\
    i = 0;\
    _tmp = from;\
    while(i < count)\
    {\
        if (*_tmp == '\0')\
        {\
            ok = 0;\
            break;\
        }\
        _tmp ++;\
        i ++;\
    }\
    if (ok)\
    {\
        i = 0;\
        while(i < count)\
        {\
            *to = *from;\
            to ++;\
            from ++;\
            i ++;\
        }\
    }\
    else\
    {\
        if (overage_buff != NULL)\
        {\
            while(*from != '\0')\
            {\
                *overage_buff = *from;\
                from ++;\
            }\
        }\
        break;\
    }

/*Function declaration*/
static char *Get_Store_fullname_From_SQL(char *extensions);
static Store *Insert_Store(Store *head_store, char *extensions, float Similar);
static int Find_1_Similar(Store *head_store);
static void Destroy_Store(Store **head_store);
static void Find_equal_1(struct ast_channel *chan, Store *head_store, int res, int *extensions, float similar);
static void Find_Greater_than_1(struct ast_channel *chan, Store *head_store, int res, int *extensions);
static void tts_speech(struct ast_channel *chan, Store *head_store, float similar, char *Extensions, int *extensions);
static int is_utf8_string(char *utf);
static void utf8_to_pinyin(char *in, char *out, int first_letter_only, int polyphone_support, int add_blank, int convert_double_char, char *overage_buff);
static int Minnum(int num01, int num02, int num03);
static int *Getaddr(int *addr, int row, int col, int size);
static int Getnum(int *addr, int row, int col, int size);
static void Putnum(int *addr, int row, int col, int size, int num);
static int LD(const char *s, const char *d);
static char *keep_first_blank(char *buf);
static char *skin_blank(char *s);
static int parse_string(char *name);
static char *char_trans(char *read_buff);
static Readname *Insertfilename(Readname *readhead, char *name, int flag);
static void Destroyreadfilename(Readname **readhead);
static char *Keep_First_Last_String(char *buf);
static int parse_file_string(Readname *readhead, char *buf, int res);
static Readname *parse_file(Readname *readhead, char *buf, int res);
static void AtoI(const char *ptr, int *num);
static Msg *Create(void);
static char *change(char *name);
static Msg *Insert(Msg *head, char *extensions, char *fullname, int res);
static Msg *Find_max(struct ast_channel *chan, Msg *head, Store *head_store, int *extensions, float *similar, int res);
static void GetReadfilenameMaxSimilar(Readname *readname, float *similar);
static void CalculateFileSimilar(Readname *readname, char *msgfullname, float *Similar, int res);
static Msg *Show_fullname(Msg *head, Readname *readname, int res);
static void Destroy(Msg **head);
static int find_extensions(Readname *readhead);
static int find_fullname(Readname *readhead);
static int sqlite3_find_num(struct ast_channel *chan, Readname *readhead, Msg *head, Store *head_store, int *extensions, float *similar, int res);
static int Get_Lang_From_DB(char **language);
static char *Replacement_First_and_Last_Name(char *fullname);

struct wave_pcm_hdr default_pcmwavhdr = 
{
    { 'R', 'I', 'F', 'F' },
    0,
    {'W', 'A', 'V', 'E'},
    {'f', 'm', 't', ' '},
    16,
    1,
    1,
    8000,
    16000,
    2,
    16,
    {'d', 'a', 't', 'a'},
    0  
};

static int text_to_speech(const char *src_text , const char *des_path, const char *params)
{
    struct wave_pcm_hdr pcmwavhdr = default_pcmwavhdr;
    const char* sess_id = NULL;
    int ret = 0;
    unsigned int text_len = 0;
    unsigned int audio_len = 0;
    int synth_status = 1;
    FILE* fp = NULL;

    ast_log(LOG_NOTICE, "begin to synth...\n");
    if (NULL == src_text || NULL == des_path)
    {
        ast_log(LOG_ERROR, "params is null!\n");
        return -1;
    }
    text_len = (unsigned int)strlen(src_text);
    fp = fopen(des_path,"wb");
    if (NULL == fp)
    {
        ast_log(LOG_ERROR, "open file %s error\n", des_path);
        return -1;
    }
    sess_id = QTTSSessionBegin(params, &ret);
    if ( ret != MSP_SUCCESS )
    {
        ast_log(LOG_ERROR, "QTTSSessionBegin: qtts begin session failed Error code %d.\n", ret);
        return ret;
    }

    ret = QTTSTextPut(sess_id, src_text, text_len, NULL );
    if ( ret != MSP_SUCCESS )
    {
        ast_log(LOG_ERROR, "QTTSTextPut: qtts put text failed Error code %d.\n", ret);
        QTTSSessionEnd(sess_id, "TextPutError");
        return ret;
    }
    fwrite(&pcmwavhdr, sizeof(pcmwavhdr) ,1, fp);
    while (1) 
    {
        const void *data = QTTSAudioGet(sess_id, &audio_len, &synth_status, &ret);
        if (NULL != data)
        {
           fwrite(data, audio_len, 1, fp);
           pcmwavhdr.data_size += audio_len;//pcm data size correction
        }
        if (synth_status == 2 || ret != 0) 
        break;
    }

    //The size of the data file header fix pcm
    pcmwavhdr.size_8 += pcmwavhdr.data_size + 36;

    //The corrected data is written back to the file header
    fseek(fp, 4, 0);
    fwrite(&pcmwavhdr.size_8,sizeof(pcmwavhdr.size_8), 1, fp);
    fseek(fp, 40, 0);
    fwrite(&pcmwavhdr.data_size,sizeof(pcmwavhdr.data_size), 1, fp);
    fclose(fp);

    ret = QTTSSessionEnd(sess_id, NULL);
    if ( ret != MSP_SUCCESS )
    {
        ast_log(LOG_ERROR, "QTTSSessionEnd: qtts end failed Error code %d.\n", ret);
    }
    return ret;
}

static int tts_tts_speech(char *fullname, char *extensions, char *Count)
{
    int res = 0;

    ast_log(LOG_NOTICE, "fullname = %s, extensions = %s\n", fullname, extensions);
    if ((NULL == fullname) || (extensions == NULL))
    {
        ast_log(LOG_WARNING, "Param is NULL!\n");
        return -1;
    }

    ast_log(LOG_NOTICE, "fullname = %s, extensions = %s\n", fullname, extensions);
    if (Count != NULL)
    {
        AtoI(Count, &res);
    }
    ast_log(LOG_NOTICE, "line = %d, res = %d\n", __LINE__, res);
    ///APPID do not tamper with
    const char *login_configs = " appid = 53a14932, work_dir =   .  ";
    char text[1024];
    memset(text, 0, sizeof(text));
    if (res == 0)
    {
        sprintf(text, "正在拨号%s电话%s", fullname, extensions);
        ast_log(LOG_NOTICE, "text = %s\n", text);
    }
    else
    {
        sprintf(text, "找到联系人%s电话%s，按%d选择", fullname, extensions, res);
    }

    char filename[256];
    memset(filename, 0, sizeof(filename));
    sprintf(filename, "/data/%s.wav", extensions);
    const char* param = "aue = speex, vcn=xiaoyan,  spd = 50, vol = 50, tte = utf8, auf=audio/L16;rate=8000";
    int ret = 0;

    //User Login
    ret = MSPLogin(NULL, NULL, login_configs);
    if ( ret != MSP_SUCCESS )
    {
        ast_log(LOG_WARNING, "MSPLogin failed , Error code %d.\n", ret);
    }

    //Audio synthesis
    ret = text_to_speech(text, filename, param);
    if ( ret != MSP_SUCCESS )
    {
        ast_log(LOG_WARNING, "text_to_speech: failed , Error code %d.\n", ret);
    }
    //Sign In
    MSPLogout();
    return 0;
}
/*
    ***********************************************************************
    ***********************************************************************
    ***********************************************************************
*/

static Store *Create_Store()
{
    Store *head_store = NULL;
    head_store = (Store *)malloc(sizeof(Store));
    if (head_store == NULL)
    {
        return NULL;
    }
    head_store->next = NULL;
    return head_store;
}

static char *Replacement_First_and_Last_Name(char *fullname)
{
    ast_log(LOG_NOTICE, "line = %d\n", __LINE__);
    if (fullname == NULL)
    {
        return NULL;
    }
    char buf[256];
    memset(buf, 0, sizeof(buf));
    char *last_name = NULL;
    char *first_name = NULL;

    last_name = strchr(fullname, ' ');
    if (last_name != NULL)
    {
        *(fullname + (last_name - fullname)) = '\0';
        last_name++;
        first_name = fullname;
        ast_log(LOG_NOTICE, "last_name = %s, first_name = %s\n", last_name, first_name);
        sprintf(buf, "%s%s", last_name, first_name);
        ast_log(LOG_NOTICE, "buf = %s\n",  buf);
        sprintf(fullname, "%s", buf);
    }
    else
    {
        fullname = fullname;
    }
    ast_log(LOG_NOTICE, "line = %d, last_name = %s, fullname = %s\n", __LINE__, last_name, fullname);
    return fullname;
}

static char *Get_Store_fullname_From_SQL(char *extensions)
{
    int fd = 0;
    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int result = 0;
    char *fullname = NULL;

    if ((fd = sqlite3_open(SQLITE3_PATH, &db)))
    {
        sqlite3_close(db);
        return NULL;
    }

    char **array = NULL;
    char sql[256];
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select fullname from sip_accounts where extension = %s;", extensions);
    ast_log(LOG_NOTICE, "line = %d, sql = %s\n", __LINE__, sql);
    result = sqlite3_get_table(db, sql, &array, NULL, NULL, &zErrMsg);
    if (result != 0)
    {
        ast_log(LOG_WARNING, "Get fullname is failed from sip_accounts!\n");
        sqlite3_free_table(array);
        sqlite3_close(db);
        return NULL;
    }

    fullname = (char *)malloc(MALLOC * sizeof(char));
    if (fullname == NULL)
    {
        ast_log(LOG_WARNING, "Malloc store fullname is failed!\n");
        sqlite3_free_table(array);
        sqlite3_close(db);
        return NULL;
    }
    ast_log(LOG_NOTICE, "line = %d, *(array + 1) = %s\n", __LINE__, *(array + 1));
    if (*(array + 1) != NULL)
    {
        strcpy(fullname, *(array + 1));
        sqlite3_free_table(array);
        sqlite3_close(db);
        return fullname = Replacement_First_and_Last_Name(fullname);
    }
    else
    {
        sqlite3_free_table(array);
        sqlite3_close(db);
        return NULL;
    }
}

static Store *Insert_Store(Store *head_store, char *extensions, float Similar)
{
    if (extensions == NULL)
    {
        ast_log(LOG_WARNING, "Not find extensions like this!\n");
        return head_store;
    }

    Store *current = (Store *)malloc(sizeof(Store));
    if (current == NULL)
    {
        ast_log(LOG_WARNING, "Malloc store memory false!\n");
        return head_store;
    }

    int store_len = strlen(extensions);
    current->extensions = (char *)malloc(sizeof(char) * (store_len + 1));
    if (current->extensions == NULL)
    {
        ast_log(LOG_WARNING, "Malloc store memory false!\n");
        return NULL;
    }
    strcpy(current->extensions, extensions);
    current->similar = Similar;
    current->fullname = Get_Store_fullname_From_SQL(current->extensions);
    ast_log(LOG_NOTICE, "line = %d, similar = %f, fullname = %s, extensions = %s\n", __LINE__, current->similar, current->fullname, current->extensions);
    current->next = head_store->next;
    head_store->next = current;
    return head_store;
}

static int Find_1_Similar(Store *head_store)
{
    Store *Current_Store = head_store->next;
    int count = 0;
    while (Current_Store != NULL)
    {
        ast_log(LOG_NOTICE, "line = %d, similar = %f, fullname = %s, extensions = %s\n", __LINE__, Current_Store->similar, Current_Store->fullname, Current_Store->extensions);
        if ((Current_Store->similar) == (float)1)
        {
            count++;
        }
        Current_Store = Current_Store->next;
    }

    return count;
}

static void Destroy_Store(Store **head_store)
{
    Store *destroy = NULL;
    Store *cur = NULL;
    destroy = (*head_store)->next;
    while (destroy != NULL)
    {
        cur = destroy;
        destroy = destroy->next;
        if ((cur->extensions) != NULL)
        {
            free((cur->extensions));
            cur->extensions = NULL;
        }
        if ((cur->fullname) != NULL)
        {
            free((cur->fullname));
            cur->fullname = NULL;
        }
        free(cur);
        cur = NULL;
    }

    free((*head_store));
    *head_store = NULL;
}

static void Find_equal_1(struct ast_channel *chan, Store *head_store, int res, int *extensions, float similar)
{
    Store *Current_Store = head_store->next;
    char *name = NULL;
    char *number = NULL;
    struct ast_flags flags = { 0, };
    int flag = 0;
    char Audio[256];
    memset(Audio, 0, sizeof(Audio));

    while (Current_Store != NULL)
    {
        ast_log(LOG_NOTICE, "extensions = %s, fullname = %s, Similar = %f\n", Current_Store->extensions, Current_Store->fullname, Current_Store->similar);
        if ((Current_Store->similar == (float)1) || (Current_Store->similar == similar))
        {
            name = Current_Store->fullname;
            number = Current_Store->extensions;
            break;
        }
        Current_Store = Current_Store->next;
    }
    ast_log(LOG_NOTICE, "name = %s, number = %s, Current_Store->fullname = %s\n", name, number, Current_Store->fullname);
    tts_tts_speech(Current_Store->fullname, number, NULL);
    ast_log(LOG_NOTICE, "Execute tts_tts_speech is success!\n");

    sprintf(Audio, "/data/%s", number);
    ast_log(LOG_NOTICE, "Audio = %s\n", Audio);
    
    if (!ast_test_flag(&flags, (1 << 2)))
    {
        flag= ast_streamfile(chan, Audio, chan->language);
        if (!flag)
        {
            flag = ast_waitstream(chan, "");
        }
        else
        {
            ast_log(LOG_WARNING, "ast_streamfile failed on %s\n", Audio);
        }
        ast_stopstream(chan);
    }
    if (flag != 0)
    {
        ast_log(LOG_WARNING, "ast_streamfile failed on %s\n", chan->name);
    }

    ast_stopstream(chan);
    ast_log(LOG_NOTICE, "Audio = %s\n", Audio);
    AtoI(number, extensions);
}

static void Broadcast(struct ast_channel *chan, char *extension)
{
    struct ast_flags flags = {0, };
    char Audio[256];
    memset(Audio, 0, sizeof(Audio));
    sprintf(Audio, "/data/%s", extension);
    int res = 0;
    if (!(ast_test_flag(&flags, (1 << 2))))
    {
        res = ast_streamfile(chan, Audio, chan->language);
        if (!res)
        {
            res = ast_waitstream(chan, "");
        }
        else
        {
            ast_log(LOG_WARNING, "ast_streamfile failed on %s\n", chan->name);
        }
        ast_stopstream(chan);
    }
}

static void Find_Greater_than_1(struct ast_channel *chan, Store *head_store, int res, int *extensions)
{
    Store *Current_Store = head_store->next;

    struct ast_frame *f = NULL;
    struct ast_flags flags = { 0, };

    int count = 0;

    char *Extensions[10] = {0};
    char Count[10];
    memset(Count, 0, sizeof(Count));

    int waitres = 0;
    int rawtime1 = 225;

    while (Current_Store != NULL)
    {
        if ((Current_Store->similar > 0.80) && (count <= res) && (res <= 9))
        {
                count++;
                Extensions[count] = Current_Store->extensions;
                sprintf(Count, "%d", count);
                if (Current_Store->next == NULL)
                {
                    Find_equal_1(chan, head_store, 0, extensions, Current_Store->similar);
                }
                else
                {
                    tts_tts_speech(Current_Store->fullname, Current_Store->extensions, Count);
                    ast_log(LOG_NOTICE, "line = %d, Current_Store->fullname = %s, Current_Store->extensions = %s, Current_Store->determin = %d\n", __LINE__, Current_Store->fullname, Current_Store->extensions, Current_Store->determin);
                    Broadcast(chan, Current_Store->extensions);
                }
        }
        while ((waitres = ast_waitfor(chan, 0)) > -1)
        {
            rawtime1--;
            if (0 == rawtime1)
            {
                break;
            }

            f = ast_read(chan);
            if (!f)
                {
                break;
            }
            if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 1))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_1", "DTMF");
                AtoI(Extensions[1], extensions);
                ast_log(LOG_NOTICE, "Extensions[1] = %s, *extensions = %d\n", Extensions[1], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 2))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_2", "DTMF");
                AtoI(Extensions[2], extensions);
                ast_log(LOG_NOTICE, "Extensions[1] = %s, *extensions = %d\n", Extensions[2], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 3))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_3", "DTMF");
                AtoI(Extensions[3], extensions);
                ast_log(LOG_NOTICE, "Extensions[3] = %s, *extensions = %d\n", Extensions[3], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 4))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_4", "DTMF");
                AtoI(Extensions[4], extensions);
                ast_log(LOG_NOTICE, "Extensions[4] = %s, *extensions = %d\n", Extensions[4], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 5))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_5", "DTMF");
                AtoI(Extensions[5], extensions);
                ast_log(LOG_NOTICE, "Extensions[5] = %s, *extensions = %d\n", Extensions[5], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 6))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_6", "DTMF");
                AtoI(Extensions[6], extensions);
                ast_log(LOG_NOTICE, "Extensions[6] = %s, *extensions = %d\n", Extensions[6], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 7))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_7", "DTMF");
                AtoI(Extensions[7], extensions);
                ast_log(LOG_NOTICE, "Extensions[7] = %s, *extensions = %d\n", Extensions[7], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 8))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_8", "DTMF");
                AtoI(Extensions[8], extensions);
                ast_log(LOG_NOTICE, "Extensions[8] = %s, *extensions = %d\n", Extensions[8], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            else if ((f->frametype == AST_FRAME_DTMF) && ((!ast_test_flag(&flags, 9))))
            {
                pbx_builtin_setvar_helper(chan, "BUTTON_9", "DTMF");
                AtoI(Extensions[9], extensions);
                ast_log(LOG_NOTICE, "Extensions[1] = %s, *extensions = %d\n", Extensions[9], *extensions);
                ast_frfree(f);
                f = NULL;
                break;
            }
            ast_frfree(f);
            f = NULL;
        }
        ast_log(LOG_NOTICE, "Next extensions!\n");
        Current_Store = Current_Store->next;
    }
}

static void tts_speech(struct ast_channel *chan, Store *head_store, float similar, char *Extensions, int *extensions)
{
    ast_log(LOG_NOTICE, "line = %d, head_store = %p, similar = %f, Extensions = %s, extensions = %p\n", __LINE__, head_store, similar, Extensions, extensions);
    Insert_Store(head_store, Extensions, similar);

    int res = Find_1_Similar(head_store);
    ast_log(LOG_NOTICE, "tts_speech func res = %d\n", res);

    if (res == 0)
    {
        Find_Greater_than_1(chan, head_store, res, extensions);
    }
    else if (res == 1)
    {
        Find_equal_1(chan, head_store, res, extensions, 0);
    }
}

/*
    ***********************************************************************
    ***********************************************************************
    ***********************************************************************
*/

static int is_utf8_string(char *utf)
{
    int length = strlen(utf);
    int check_sub = 0;
    int i = 0;

    if ( length > HZ2PY_UTF8_CHECK_LENGTH )
    {
        length = HZ2PY_UTF8_CHECK_LENGTH;
    }

    for ( ; i < length; i ++ )
    {
        if ( check_sub == 0 )
        {
            if ( (utf[i] >> 7) == 0 )
            {
                continue;
            }
            else if ( (utf[i] & 0xE0) == 0xC0 )
            {
                check_sub = 1;
            }
            else if ( (utf[i] & 0xF0) == 0xE0 )
            {
                check_sub = 2;
            }
            else if ( (utf[i] & 0xF8) == 0xF0 )
            {
                check_sub = 3;
            }
            else if ( (utf[i] & 0xFC) == 0xF8 )
            {
                check_sub = 4;
            }
            else if ( (utf[i] & 0xFE) == 0xFC )
            {
                check_sub = 5;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            if ( (utf[i] & 0xC0) != 0x80 )
            {
                return 0;
            }
            check_sub --;
        }
    }
    return SUCCESS;
}

static void utf8_to_pinyin(char *in, char *out, int first_letter_only, int polyphone_support, int add_blank, int convert_double_char, char *overage_buff)
{
    int i = 0;
    char *utf = in;
    char *_tmp = NULL;
    char *_tmp2 = NULL;
    char py_tmp[30] = "";
    char py_tmp2[30] = "";
    char *out_start_flag = out;
    int uni = 0;
    int ok = 0;

    while (*utf != '\0')
    {
        if ( (*utf >> 7) == 0 )
        {
            HZ2PY_STR_COPY(out, utf, 1);
        }
        //0xxx xxxx
        else if ( (*utf & 0xE0) == 0xC0 )
        {
            HZ2PY_STR_COPY(out, utf, 2);
        }
        //110x xxxx xxxx xxxx
        else if ( (*utf & 0xF0) == 0xE0 )   //1110 xxxx
        {
            if (*(utf + 1) != '\0' && *(utf + 2) != '\0')
            {
                uni = (((int)(*utf & 0x0F)) << 12)
                    | (((int)(*(utf + 1) & 0x3F)) << 6)
                    | (*(utf + 2) & 0x3F);

                //printf("%d\n", uni);
                /*****yi==19968*****/
                if ( uni > 19967 && uni < 40870 )
                {
                    if (add_blank == 1 && out > out_start_flag && *(out - 1) != ' ' && *(out - 1) != '\r' && *(out - 1) != '\n' && *(out - 1) != '\t')
                    {
                        *out = ' ';
                        out ++;
                    }

                    memset(py_tmp, '\0', 30);
                    memset(py_tmp2, '\0', 30);

                    strcpy(py_tmp, _pinyin_table_[uni - 19968]);

                    _tmp = py_tmp;
                    _tmp2 = py_tmp2;

                    if (first_letter_only == 1)
                    {
                        *_tmp2 = *_tmp;
                        _tmp ++;
                        _tmp2 ++;
                        while(*_tmp != '\0')
                        {
                            if (*_tmp == '|' || *(_tmp - 1) == '|')
                            {
                                *_tmp2 = *_tmp;
                                _tmp2 ++;
                            }
                            _tmp ++;
                        }
                    }
                    else
                    {
                        strcpy(py_tmp2, py_tmp);
                    }

                    _tmp2 = py_tmp2;

                    if (polyphone_support == 0)
                    {
                        while(*_tmp2 != '\0')
                        {
                            if (*_tmp2 == '|')
                            {
                                *_tmp2 = '\0';
                                break;
                            }
                            _tmp2 ++;
                        }

                        _tmp2 = py_tmp2;
                    }

                    strcpy(out, _tmp2);
                    out += strlen(_tmp2);
                    if (add_blank)
                    {
                        *out = ' ';
                        out ++;
                    }
                    utf += 3;
                }
                else if ( convert_double_char && uni > 65280 && uni < 65375)
                {
                    *out = uni - 65248;
                    out ++;
                    utf += 3;
                }
                else if ( convert_double_char && uni == 12288 )
                {
                    *out = 32;
                    out ++;
                    utf += 3;
                }
                else
                {
                    HZ2PY_STR_COPY(out, utf, 3);
                }
            }
            else
            {
                HZ2PY_STR_COPY(out, utf, 3);
            }
        }
        else if ( (*utf & 0xF8) == 0xF0 )    //1111 0xxx
        {
            HZ2PY_STR_COPY(out, utf, 4);
        }
        else if ( (*utf & 0xFC) == 0xF8 )   //1111 10xx
        {
            HZ2PY_STR_COPY(out, utf, 5);
        }
        else if ( (*utf & 0xFE) == 0xFC )   //1111 110x
        {
            HZ2PY_STR_COPY(out, utf, 6);
        }
        else
        {
            if (overage_buff != NULL)
            {
                *overage_buff = *utf;
                overage_buff ++;
            }
            else
            {
                HZ2PY_STR_COPY(out, utf, 1);
            }
            break;
        }
    }
}

static int Minnum(int num01, int num02, int num03)
{
    int min = num01;

    if (min > num02)
    {
        min = num02;
    }

    if (min > num03)
    {
        min = num03;
    }

    return min;
}

static int *Getaddr(int *addr, int row, int col, int size)
{
    if (addr == NULL)
    {
        ast_log(LOG_WARNING, "Obtaining an illegal address structural matrix\n");
        return NULL;
    }

    return (addr + col + row * (size + 1));
}

static int Getnum(int *addr, int row, int col, int size)
{
    int *paddr = Getaddr(addr, row, col, size);
    return *paddr;
}

static void Putnum(int *addr, int row, int col, int size, int num)
{
    if (addr == NULL)
    {
        return;
    }

    int *paddr = Getaddr(addr, row, col, size);
    if (addr == NULL)
    {
        return;
    }
    *paddr = num;
}

static int LD(const char *s, const char *d)
{
    int lenrow = 0;
    int lencol = 0;
    int *source = NULL;
    int *cost = NULL;
    char rowchar;
    char colchar;
    int above;
    int left;
    int midleftabove;
    int result;
    int len;
    int row, col;

    if ((s == NULL) || (d == NULL))
    {
        return -1;
    }

    lenrow = strlen(s);
    lencol = strlen(d);

    len = (lenrow + 1) * (lencol + 1) * sizeof(int);
    source = (int *)malloc(len);
    cost = (int *)malloc(len);

    if ((source == NULL) || (cost == NULL))
    {
        return -1;
    }

    for (col = 0; col <= lencol; col++)
    {
        Putnum(source, 0, col, lenrow, col);
    }
    for (row = 0; row <= lenrow; row++)
    {
        Putnum(source, row, 0, lencol, row);
    }

    for (row = 0; row <= lenrow; row++)
    {
        for (col = 0; col <= lencol; col++)
        {
            Putnum(cost, row, col, lencol, 2);
        }
    }

    for (row = 1; row <= lenrow; row++)
    {
        rowchar = s[row - 1];
        for (col = 1; col <= lencol; col++)
        {
            int temp;
            colchar = d[col - 1];
            if (rowchar == colchar)
            {
                temp = Getnum(cost, row, col, lencol);
                if (2 == temp)
                {
                    Putnum(cost, row, col, lencol, 0);
                }
            }
            else
            {
                temp = Getnum(cost, row, col, lencol);
                if (2 == temp)
                {
                    Putnum(cost, row, col, lencol, 1);
                }
            }

            above = Getnum(source, row, col - 1, lencol);
            left = Getnum(source, row - 1, col, lencol);
            midleftabove = Getnum(source, row - 1, col - 1, lencol);
            int curcost = Getnum(cost, row, col, lencol);
            int min = Minnum(above + 1, left + 1, midleftabove + curcost);
            Putnum(source, row, col, lencol, min);
        }
    }
    result = Getnum(source, lenrow, lencol, lencol);

    free(source);
    free(cost);

    return result;
}

static char *keep_first_blank(char *buf)
{
    if (buf == NULL)
    {
        return NULL;
    }

    char *tmp = strchr(buf, ' ');
    if (tmp == NULL)
    {
        ast_log(LOG_NOTICE, "When keep first blank, it isn't exit!\n");
        return buf;
    }
    *(buf + (tmp - buf)) = '\0';
    tmp++;
    if ((tmp = skin_blank(tmp)) == NULL)
    {
        ast_log(LOG_WARNING, "It is error, when skin blank!\n");
        return NULL;
    }
    sprintf(buf, "%s %s", buf, tmp);
    return buf;
}

static char *skin_blank(char *s)
{
    static char name[MAXSIZE];
    int i = 0;
    if (s == NULL)
    {
        return NULL;
    }
    while (*s != '\0')
    {
        if (*s == ' ')
        {
            s++;
        }
        else
        {
            name[i] = *s;
            i++;
            s++;
        }
    }
    name[i] = '\0';
    return name;
}

static int parse_string(char *name)
{
    if (name == NULL)
    {
        return FALSE;
    }
    int len = strlen(name);
    int Acount = 0;
    int Ncount = 0;
    char *tmp = name;
    int flag = 0;

    while (*tmp != '\0')
    {
        flag = 0;
        if (*tmp>='0' && *tmp<='9')
        {
            tmp++;
            if ((*tmp>='a' && *tmp<='z') || (*tmp >= 'A' && *tmp <= 'Z'))
            {
                flag = 1;
                break;
            }
            else
            {
                tmp--;
            }
        }

        if ((*tmp>='a' && *tmp<='z') || (*tmp >= 'A' && *tmp <= 'Z'))
        {
            tmp++;
            if (*tmp>='0' && *tmp<='9')
            {
                flag = 1;
                break;
            }
            else
            {
                tmp--;
            }
        }

        if ((*tmp>='a' && *tmp<='z') || (*tmp >= 'A' && *tmp <= 'Z'))
        {
            tmp++;
            if ((*tmp>='a' && *tmp<='z') || (*tmp >= 'A' && *tmp <= 'Z'))
            {
                tmp--;
                Acount++;
            }
        }

        if (*tmp>='0' && *tmp<='9')
        {
            tmp++;
            if (*tmp>='0' && *tmp<='9')
            {
                tmp--;
                Ncount++;
            }
        }

        if ((*tmp == '\0') || (*tmp == ' '))
        {
            break;
        }

        tmp++;
    }

    if (flag == 1)
    {
//        ast_log(LOG_WARNING, "The information is read mixed letters and numbers, the information is invalid!\n");
        return FALSE;
    }
    else if (Ncount == len - 1)
    {
//        ast_log(LOG_NOTICE, "The information is read numbers\n");
        return NUMBER;
    }
    else if (Acount == len - 1)
    {
//        ast_log(LOG_NOTICE, "The information is read letters\n");
        return STRING;
    }

    return ZERO;
}

static char *char_trans(char *read_buff)
{
    int has_check_utf8 = 0;
    char overage_buff[7];
    int i = 0;
    int j = 0;
    char Judge_digital = '\0';

    char inbuf[HZ2PY_INPUT_BUF_ARRAY_SIZE];
    char outbuf[HZ2PY_OUTPUT_BUF_ARRAY_SIZE];
    char std_buf[HZ2PY_OUTPUT_BUF_ARRAY_SIZE];

    int add_blank = 1;
    int polyphone_support = 0;
    int first_letter_only = 0;
    int convert_double_char = 0;
    
//    ast_log(LOG_NOTICE, "To read Chinese characters into pinyin start\n");
    memset(inbuf, '\0', sizeof(char) * HZ2PY_INPUT_BUF_ARRAY_SIZE);
    memset(outbuf, '\0', sizeof(char) * HZ2PY_OUTPUT_BUF_ARRAY_SIZE);
    memset(std_buf, '\0', sizeof(char) * HZ2PY_OUTPUT_BUF_ARRAY_SIZE);
    memset(overage_buff, '\0', 7);

    if (read_buff == NULL)
    {
        return NULL;
    }

    for (i = 0; read_buff[i] !='\0';i++)
    inbuf[i] = read_buff[i];

    if ( has_check_utf8 == 0 )
    {
        if ( !is_utf8_string(inbuf) )
        {
            fputs("File encoding is not utf-8!\n", stderr);
            return NULL;
        }
        has_check_utf8 = 1;
    }

    utf8_to_pinyin(inbuf,
        outbuf,
        first_letter_only,
        polyphone_support,
        add_blank,
        convert_double_char,
        overage_buff);
    char *out_buf = (char *)malloc(MALLOC * (strlen(outbuf) + 1));
    if (out_buf == NULL)
    {
        return NULL;
    }
    sprintf(out_buf, "%s", outbuf);
    for (i = 0;i < HZ2PY_OUTPUT_BUF_ARRAY_SIZE ; i++)
        {
            Judge_digital = outbuf[i];
            switch (Judge_digital)
            {
                case '0':std_buf[j] = 'l'; j++; std_buf[j] = 'i'; j++; std_buf[j] = 'n'; j++; std_buf[j] = 'g'; break;
                case '1':std_buf[j] = 'y'; j++; std_buf[j] = 'i';  break;
                case '2':std_buf[j] = 'e'; j++; std_buf[j] = 'r';  break;
                case '3':std_buf[j] = 's'; j++; std_buf[j] = 'a'; j++; std_buf[j] = 'n';  break;
                case '4':std_buf[j] = 's'; j++; std_buf[j] = 'i';  break;
                case '5':std_buf[j] = 'w'; j++; std_buf[j] = 'u';  break;
                case '6':std_buf[j] = 'l'; j++; std_buf[j] = 'i'; j++; std_buf[j] = 'u';  break;
                case '7':std_buf[j] = 'q'; j++; std_buf[j] = 'i';  break;
                case '8':std_buf[j] = 'b'; j++; std_buf[j] = 'a';  break;
                case '9':std_buf[j] = 'j'; j++; std_buf[j] = 'i'; j++; std_buf[j] = 'u';  break;
                default: std_buf[j] = outbuf[i];
            }
            j++;
        }
    sprintf(out_buf, "%s", std_buf);
    ast_log(LOG_NOTICE, "out_buf = %s\n", out_buf);
    return out_buf;
}

static Readname *Createfilename()
{
    Readname *readhead = (Readname *)malloc(sizeof(Readname));
    if (readhead == NULL)
    {
        ast_log(LOG_WARNING, "Malloc memory for Readname is failed!");
        return NULL;
    }
    readhead->next = NULL;
    return readhead;
}

static Readname *Insertfilename(Readname *readhead, char *name, int flag)
{
    if (name == NULL)
    {
        return NULL;
    }
    Readname *tmp = (Readname *)malloc(sizeof(Readname));
    if (tmp == NULL)
    {
        return NULL;
    }
    if (flag == STRING)
    {
        tmp->readfilename = name;
        tmp->extensions = NULL;
    }

    if (flag == NUMBER)
    {
        tmp->extensions = name;
        tmp->readfilename = NULL;
    }
    tmp->next = readhead->next;
    readhead->next = tmp;

    return readhead;
}

static void Destroyreadfilename(Readname **readhead)
{
    Readname *destroyfilename = NULL;
    Readname *curfilename = NULL;
    destroyfilename = (*readhead)->next;
    while (destroyfilename != NULL)
    {
        curfilename = destroyfilename;
        destroyfilename = destroyfilename->next;
        if ((curfilename->extensions) != NULL)
        {
            free((curfilename->extensions));
            (curfilename->extensions) = NULL;
        }
        if ((curfilename->readfilename) != NULL)
        {
            free(curfilename->readfilename);
            curfilename->readfilename = NULL;
        }
        free(curfilename);
        curfilename = NULL;
    }
    free((*readhead));
    (*readhead) = NULL;
}

static char *Keep_First_Last_String(char *buf)
{
    if (buf == NULL)
    {
        return NULL;
    }
    char *first_blank = NULL;
    char *last_blank = NULL;
    char tmp_buf[MIN_SIZE];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    sprintf(tmp_buf, "%s", buf);
    char *temp = (char *)malloc(sizeof(char) * MALLOC);
    if (temp == NULL)
    {
        return NULL;
    }

    if ((first_blank = strchr(buf, ' ')) != NULL)
    {
        *(buf + (first_blank - buf)) = '\0';

        if ((last_blank = strrchr(tmp_buf, ' ')) != NULL)
        {
            last_blank++;
        }
        sprintf(tmp_buf, "%s %s", buf, last_blank);
    }
    sprintf(temp, "%s", tmp_buf);
    return temp;
}

static int parse_file_string(Readname *readhead, char *buf, int res)
{
    if (buf == NULL)
    {
        return FALSE;
    }
    int flag = 0;
    if (res == false)
    {
        char *temp = buf;
        int len = strlen(buf);

        temp = char_trans(temp);
        temp = keep_first_blank(temp);

        char *skinblank = NULL;
        if ((skinblank = skin_blank(temp)) == NULL)
        {
            return FALSE;
        }
/*
        if ((flag = parse_string(skinblank)) == FALSE)
        {
            free(temp);
            temp = NULL;
        }
*/
        if (((flag = parse_string(skinblank)) == NUMBER))
        {
            sprintf(temp, "%s", skinblank);
            readhead = Insertfilename(readhead, temp, NUMBER);
            return NUMBER;
        }
        if ((((flag = parse_string(skinblank)) == STRING) || ((flag = parse_string(skinblank)) == FALSE))&& (len == LEN_1))
        {
            sprintf(temp, "%s", skinblank);
            readhead = Insertfilename(readhead, temp, STRING);
            return STRING;
        }
        if ((((flag = parse_string(skinblank)) == STRING) || ((flag = parse_string(skinblank)) == FALSE)) && (len > LEN_1))
        {
            readhead = Insertfilename(readhead, temp, STRING);
            return STRING;
        }
        else
        {
            free(temp);
            temp = NULL;
        }
    }
    if (res == true)
    {
        char *skinblank = skin_blank(buf);

        char *temp = NULL;
        int flag = 0;
        if (((flag = parse_string(skinblank)) == NUMBER))
        {
            temp = Keep_First_Last_String(skinblank);
            sprintf(temp, "%s", skinblank);
            readhead = Insertfilename(readhead, temp, NUMBER);
            return NUMBER;
        }

        if ((flag = parse_string(skinblank)) == STRING)
        {
            temp = Keep_First_Last_String(buf);
            readhead = Insertfilename(readhead, temp, STRING);
            return STRING;
        }
        else
        {
            free(temp);
            temp = NULL;
        }

    }
    return SUCCESS;
}

static Readname *parse_file(Readname *readhead, char *buf, int res)
{
    if (buf == NULL)
    {
        ast_log(LOG_WARNING, "Afferent parameters is NULL!\n");
        return NULL;
    }

    char *tmp = NULL;
    char *temp = NULL;
    int flag = 0;
    while ((tmp = strstr(buf, "\"transcript\"")) != NULL)
    {
        if ((temp = strchr(tmp, '}')) == NULL)
        {
            return NULL;
        }
        *(tmp + (temp - tmp)) = '\0';

        char *tempp = NULL;
        if ((tempp = strstr(tmp, "\":\"")) == NULL)
        {
            return NULL;
        }

        char *temp_1 = NULL;
        if ((temp_1 = strchr(tempp + 3, '\"')) == NULL)
        {
            return NULL;
        }
        *(tempp + (temp_1 - tempp)) = '\0';

        tempp = tempp + 3;
        ast_log(LOG_NOTICE, "Getting message '%s'\n", tempp);
        if ((flag = parse_file_string(readhead, tempp, res)) == FALSE)
        {
            return NULL;
        }

        buf = tmp + (temp - tmp + 1);
    }

    return readhead;
}

static void AtoI(const char *ptr, int *num)
{
    int sum = 0;
    if (ptr == NULL)
    {
        return;
    }

    while (*ptr)
    {
        sum = *ptr - '0' + 10 * sum;
        ptr++;
    }
    *num = sum;
}

static Msg *Create(void)
{
    Msg *head = NULL;
    head = (Msg *)malloc(sizeof(Msg));
    if (head == NULL)
    {
        return NULL;
    }
    head->next = NULL;
    return head;
}

static char *change(char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    char *fullname = name;
    char *lastname = NULL;
    char *fristname = NULL;
    lastname = strstr(name, "  ");
    if (lastname == NULL)
    {
        fristname = skin_blank(name);
        sprintf(fullname, "%s", fristname);
        return fullname;
    }
    *(name + (lastname - name)) = '\0';
    lastname += 2;
    name = skin_blank(name);
    sprintf(fullname, "%s%s", lastname, name);
    return fullname;
}

static Msg *Insert(Msg *head, char *extensions, char *fullname, int res)
{
    Msg *temp = NULL;
    temp = (Msg *)malloc(sizeof(Msg));
    int len = 0;
    char *skinblank = NULL;
    if (temp == NULL)
    {
        return NULL;
    }

    if (extensions == NULL)
    {
        (temp->extensions) = NULL;
    }
    else
    {
        temp->extensions = extensions;
    }

    if (fullname == NULL)
    {
        (temp->fullname) = NULL;
    }
    else if (res == false)
    {
        len = strlen(fullname);
        fullname = char_trans(fullname);
        fullname = change(fullname);
        if ((len == LEN_1) || (len == 7))
        {
            if ((skinblank = skin_blank(fullname)) == NULL)
            {
                return head;
            }
            sprintf(fullname, "%s", skinblank);
        }
        temp->fullname = fullname;
    }
    else if (res == true)
    {
        char *tmp_fullname = Keep_First_Last_String(fullname);
        temp->fullname = tmp_fullname;
    }
    temp->next = head->next;
    head->next = temp;
    return head;
}

static Msg *Find_max(struct ast_channel *chan, Msg *head, Store *head_store, int *extensions, float *similar, int res)
{
    Msg *cur = head->next;
    Msg *next = cur->next;
    char *Extensions = NULL;

    float MaxSimilar = 0;
    if (cur != NULL)
    {
        MaxSimilar = cur->similar;
        Extensions = cur->extensions;
    }

    if (false == res)
    {
        while (next != NULL)
        {
            if (((next->similar) > MaxSimilar) && ((next->similar >= (float)0.8)))
            {
                Extensions = next->extensions;
                MaxSimilar = next->similar;
                ast_log(LOG_NOTICE, "fullname = %s, extensions = %s, similar = %f\n", next->fullname, next->extensions, next->similar);
                tts_speech(chan, head_store, MaxSimilar, Extensions, extensions);
                if (extensions != NULL)
                {
                    *similar = MaxSimilar;
                }
                ast_log(LOG_NOTICE, "Find the extension MaxSimilar = '%f', Exetension = '%s', extensions = '%d'\n", MaxSimilar, Extensions, *extensions);
            }
            next = next->next;
        }
    }

    if (true == res)
    {
        while (next != NULL)
        {
            if (((next->similar) > MaxSimilar) && ((next->similar >= (float)0.8)))
            {
                Extensions = next->extensions;
                MaxSimilar = next->similar;
            }
            next = next->next;
        }
        *similar = MaxSimilar;
        AtoI(Extensions, extensions);
    }

    ast_log(LOG_NOTICE, "Find the extension number is '%d'\n", *extensions);
    return head;
}

static void GetReadfilenameMaxSimilar(Readname *readname, float *similar)
{
    Readname *tmp = readname->next;
    float MaxSimilar = tmp->similar;
    Readname *cur = tmp->next;
    while (cur != NULL)
    {
        if (MaxSimilar < (cur->similar))
        {
            MaxSimilar = cur->similar;
        }

        cur = cur->next;
    }

    if (MaxSimilar > 0.5)
    {
        *similar = MaxSimilar;
    }
    else
    {
        *similar = 0;
    }
}

static void CalculateFileSimilar(Readname *readname, char *msgfullname, float *Similar, int res)
{
    Readname *CurReadName = readname->next;
    int result = 0;
    int len_1 = 0, len_2 = 0, MaxLen = 0;

    if (msgfullname == NULL)
    {
        return;
    }

    char *blank = NULL;
    char *msgfullname_blank = NULL;

    len_1 = strlen(msgfullname);

    while (CurReadName != NULL)
    {
        if (res == false)
        {
            if ((blank = skin_blank((CurReadName->readfilename))) != NULL)
            {
                sprintf((CurReadName->readfilename), "%s", blank);
            }
        }

        if (res == true)
        {
            if ((blank = strchr((CurReadName->readfilename), ' ')) == NULL)
            {
                if ((msgfullname_blank = strchr(msgfullname, ' ')) == NULL)
                {
                    msgfullname = msgfullname;
                }
                else
                {
                    *(msgfullname + (msgfullname_blank - msgfullname)) = '\0';

                    len_1 = len_1 - 1;
                }
            }

        }
        len_2 = strlen((CurReadName->readfilename));
        MaxLen = len_1 > len_2 ? len_1 : len_2;

        result = LD(msgfullname, (CurReadName->readfilename));
        CurReadName->similar = 1 - (float)result / MaxLen;

        CurReadName = CurReadName->next;
    }
    GetReadfilenameMaxSimilar(readname, Similar);
}

static Msg *Show_fullname(Msg *head, Readname *readname, int res)
{
    Msg *msghead = head->next;
    Readname *msgreadname = readname->next;
    char *msgfullname = NULL;
    char *msgreadfilename = NULL;

    float similar = 0;
    if (res == false)
    {
        if ((msgreadfilename = strchr((msgreadname->readfilename), ' ')) == NULL)
        {
            while (msghead != NULL)
            {
                if ((msghead->fullname) != NULL)
                {
                    if ((msgfullname = strchr((msghead->fullname), ' ')) != NULL)
                    {
                        msgfullname++;

                    }
                    else if ((msgfullname = strchr((msghead->fullname), ' ')) == NULL)
                    {
                        msgfullname = (msghead->fullname);

                    }

                    CalculateFileSimilar(readname, msgfullname, &similar, res);

                    msghead->similar = similar;
                }
                else
                {
                    msghead->similar = 0;
                }
                msghead = msghead->next;
            }
        }

        if ((msgreadfilename = strchr((msgreadname->readfilename), ' ')) != NULL)
        {
            while (msghead != NULL)
            {
                if ((msghead->fullname) != NULL)
                {
                    msgfullname = msghead->fullname;
                    char *skinblank = skin_blank((msghead->fullname));
                    sprintf(msgfullname, "%s", skinblank);

                    CalculateFileSimilar(readname, msgfullname, &similar, res);
                    msghead->similar = similar;
                }
                else
                {
                    msghead->similar = 0;
                }
                msghead = msghead->next;
            }
        }
    }

    if (res == true)
    {
        while (msghead != NULL)
        {
            if ((msghead->fullname) != NULL)
            {
                CalculateFileSimilar(readname, (msghead->fullname), &similar, res);
                msghead->similar = similar;
            }
            else
            {
                msghead->similar = (float)0;
            }
            msghead = msghead->next;
        }
    }

    return head;
}

static void Destroy(Msg **head)
{
    Msg *destroy = NULL;
    Msg *cur = NULL;
    destroy = (*head)->next;
    while (destroy != NULL)
    {
        cur = destroy;
        destroy = destroy->next;

        if ((cur->fullname) != NULL)
        {
            free((cur->fullname));
            (cur->fullname) = NULL;
        }
        free(cur);
        cur = NULL;
    }
    free((*head));
    (*head) = NULL;
}

static int find_extensions(Readname *readhead)
{
    Readname *curreadhead = readhead->next;
    int count = 0;
    while (curreadhead != NULL)
    {
        if ((curreadhead->extensions) != NULL)
        {
            count++;
        }
        curreadhead = curreadhead->next;
    }
    return count;
}

static int find_fullname(Readname *readhead)
{
    Readname *curreadhead = readhead->next;
    int count = 0;
    while (curreadhead != NULL)
    {
        if ((curreadhead->readfilename) != NULL)
        {
            count++;
        }
        curreadhead = curreadhead->next;
    }
    return count;
}

static int sqlite3_find_num(struct ast_channel *chan, Readname *readhead, Msg *head, Store *head_store, int *extensions, float *similar, int res)
{
    int fd = 0;
    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int result = 0;
    int i = 0;
    char *extensions_callid = NULL;

    char sql[MAXSIZE];
    int flag = 0;
    memset(sql, 0, sizeof(sql));

    int row = 0, col = 0;

    fd = sqlite3_open(SQLITE3_PATH, &db);
    if (fd)
    {
        sqlite3_close(db);
        return FALSE;
    }

    char **array = NULL;
    if ((flag = find_extensions(readhead)) > 0)
    {
        Readname *curfilename = readhead->next;
        while (curfilename != NULL)
        {
            if ((curfilename->extensions) != NULL)
            {
                sprintf(sql, "select extension from sip_accounts where extension = %s;", curfilename->extensions);

                result = sqlite3_get_table(db, sql, &array, &row, &col, &zErrMsg);
                if (result != 0)
                {
                    sqlite3_free_table(array);
                    sqlite3_close(db);
                    ast_log(LOG_WARNING, "Open sql structdata is false!\n");
                    return FALSE;
                }

                if ((col == 1) && (row == 1))
                {
                    AtoI(*(array + 1), extensions);
                    sqlite3_free_table(array);
                    sqlite3_close(db);
                    *similar = (float)1;
                    return SUCCESS;
                }
                else
                {
                    sqlite3_free_table(array);
                }
            }
            curfilename = curfilename->next;
        }
        sqlite3_close(db);
    }

    if ((flag = find_fullname(readhead)) > 0)
    {
        strcpy(sql, "select extension, fullname from sip_accounts;");

        result = sqlite3_get_table(db, sql, &array, &row, &col, &zErrMsg);
        if (result != 0)
        {
            ast_log(LOG_WARNING, "Open sql structdata is false!\n");
            sqlite3_free_table(array);
            sqlite3_close(db);

            return FALSE;
        }

        i = col;
        for (; i < (row + 1) * col; i++)
        {
            if (i % col == 0)
            {
                extensions_callid = *(array + i);
                continue;
            }

            if (i % col == 1)
            {
                if ((*(array + i)) == NULL)
                {
                    head = Insert(head, extensions_callid, NULL, res);
                }
                else
                {

                    head = Insert(head, extensions_callid, *(array + i), res);
                }
            }
        }

        head = Show_fullname(head, readhead, res);

        head = Find_max(chan, head, head_store, extensions, similar, res);

        sqlite3_free_table(array);
        sqlite3_close(db);

        return SUCCESS;
    }

    return ZERO;
}

static int Get_Lang_From_DB(char **language)
{
    int fd = 0;
    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int result = 0;
    char *language_settings = NULL;
    fd = sqlite3_open(SQLITE3_PATH, &db);
    if (fd)
    {
        ast_log(LOG_WARNING, "Open LADP DB failed!");
        sqlite3_close(db);
        return -1;
    }

    char **array = NULL;
    char * sql = "select * from language_settings;";
    result = sqlite3_get_table(db, sql, &array, NULL, NULL, &zErrMsg);
    if (result != 0)
    {
        ast_log(LOG_WARNING, "Getting message is failed from LADP!");
        sqlite3_free_table(array);
        sqlite3_close(db);
        return -1;
    }
    language_settings = (char *)malloc(sizeof(char) * 6);
    if (language_settings == NULL)
    {
        return -1;
    }
    strcpy(language_settings, *(array + 1));
    *language = language_settings;
    sqlite3_free_table(array);
    sqlite3_close(db);
    return 0;
}

static int body_function_read(struct ast_channel *chan, const char *cmd, char *data, char *bufbuf, size_t buflen)
{
    struct ast_flags flags = { 0, };
    int fd = 0;
    int res = 0;
    int extensions = 0;
    char buf[MAXSIZE];
    char path[MAXSIZE];
    char headpath[MAXSIZE];
    float Similar = 0;
    char *language = NULL;
    memset(buf, 0, sizeof(buf));
    memset(path, 0, sizeof(path));
    memset(headpath, 0, sizeof(headpath));
    char audiobuf[256];
    sprintf(path, "%s/body-%ld.out", data, pthread_self());
    sprintf(headpath, "%s/head-%ld.out", data, pthread_self());
    if ((fd = open(path, 0, O_RDONLY)) <= ZERO)
    {
        ast_log(LOG_WARNING, "Reading message from '%s' is false!\n", path);
        return FALSE;
    }

    if ((res = read(fd, buf, sizeof(buf))) <= ZERO)
    {
        ast_log(LOG_WARNING, "Not read any message from '%s'!\n", path);
        res = remove(path);
        if (res == 0)
        {
            ast_log(LOG_NOTICE, "'%s' file removed is successed!\n", path);
        }

        res = remove(headpath);
        if (res == 0)
        {
            ast_log(LOG_NOTICE, "'%s' file removed is successed!\n", headpath);
        }
        ast_log(LOG_WARNING, "Reading message from '%s' is false!\n", path);
        return FALSE;
    }

    if (Get_Lang_From_DB(&language) == -1)
    {
        ast_log(LOG_WARNING, "Kinds of getting language from language_settings of ucm_config is false!\n");
        return FALSE;
    }
    if (strcmp(language, "zh") == 0)
    {
        res = false;
        free(language);
        language = NULL;
        ast_log(LOG_NOTICE, "Chinese is being used currently!\n");
    }
    else
    {
        res = true;
        ast_log(LOG_NOTICE, "English is being used currently!\n");
        free(language);
        language = NULL;
    }

    Readname *readhead = NULL;
    Msg *head = NULL;
    head = Create();
    readhead = Createfilename();
    Store *head_store = Create_Store();

    readhead = parse_file(readhead, buf, res);

    if ((res = sqlite3_find_num(chan, readhead, head, head_store, &extensions, &Similar, res)) <= 0)
    {
        ast_log(LOG_WARNING, "Find the extension number from a database failure\n");
        return FALSE;
    }

    if (Similar < 0.75)
    {
        extensions = 0;
        ast_log(LOG_NOTICE, "Speech recognition false, extensions = '%d', Similar = '%f'\n", extensions, Similar);
    }
    else
    {
        ast_log(LOG_NOTICE, "Speech recognition success, extensions = '%d', Similar = '%f'\n", extensions, Similar);
    }
/*
    if (0 == extensions)
    {
        if (!ast_test_flag(&flags, (1 << 2)))
        {
            res = ast_streamfile(chan, "transfailbeep", chan->language);
            if (!res)
            {
                res = ast_waitstream(chan, "");
            }
            else
            {
                ast_log(LOG_WARNING, "ast_streamfile failed on %s\n", chan->name);
            }
            ast_stopstream(chan);
        }
    }
*/
    Destroyreadfilename(&readhead);
    Destroy(&head);
    Destroy_Store(&head_store);
    close(fd);

    snprintf(bufbuf, buflen, "%d", extensions);

    res = remove(path);
    if (0 == res)
    {
        ast_log(LOG_NOTICE, "remove %s file success!\n", path);
    }

    res = remove(headpath);
    if (0 == res)
    {
        ast_log(LOG_NOTICE, "remove %s file success!\n", headpath);
    }

    sprintf(audiobuf, "/data/%d.wav", extensions);
    res = remove(audiobuf);
    if (0 == res)
    {
        ast_log(LOG_WARNING, "remove %s file is success!\n", audiobuf);
    }

    return ZERO;
}

static struct ast_custom_function body_function =
{
    .name = "BODY",
    .read = body_function_read,
};

static int unload_module(void)
{
    int res = 0;
    res |= ast_custom_function_unregister(&body_function);
    return res;
}

static int load_module(void)
{
    int res = 0;
    res |= ast_custom_function_register(&body_function);
    return 0;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Return handling dialplan functions");
