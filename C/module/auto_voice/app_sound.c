/*** MODULEINFO
    <depend>curl</depend>
    <depend>sqlite3</depend>
    <support_level>core</support_level>
 ***/
/*
{
    "status":0,
    "id":"",
    "hypotheses":
    [
        {
            "utterance":"你好",
            "confidence":0.72307199
        }
    ]
}
*/
#include "asterisk.h"

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 328401 $")

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"

#define     SIZE        256
#define     CHINESE       0
#define     ENGLISH       1

char path_name1[SIZE];
char path_name2[SIZE];
char pathfilename[SIZE];
char filename[SIZE];
char headfile[SIZE];
char bodyfile[256];
static unsigned long get_file_size(const char *path);
static char *app = "Sound";
static int select_lang = 0 ;
enum
{
    OPTION_APPEND = (1 << 0),
    OPTION_NOANSWER = (1 << 1),
    OPTION_QUIET = (1 << 2),
    OPTION_SKIP = (1 << 3),
    OPTION_STAR_TERMINATE = (1 << 4),
    OPTION_IGNORE_TERMINATE = (1 << 5),
    OPTION_KEEP = (1 << 6),
    FLAG_HAS_PERCENT = (1 << 7),
    OPTION_ANY_TERMINATE = (1 << 8),
};

AST_APP_OPTIONS
(
    app_opts,
    {
        AST_APP_OPTION('a', OPTION_APPEND),
        AST_APP_OPTION('k', OPTION_KEEP),
        AST_APP_OPTION('n', OPTION_NOANSWER),
        AST_APP_OPTION('q', OPTION_QUIET),
        AST_APP_OPTION('s', OPTION_SKIP),
        AST_APP_OPTION('t', OPTION_STAR_TERMINATE),
        AST_APP_OPTION('y', OPTION_ANY_TERMINATE),
        AST_APP_OPTION('x', OPTION_IGNORE_TERMINATE),
    }
);

enum Curl_Name {
    Upload_Filename,
    Upload_File_Path,
    receive_headerfilename,
    receive_bodyfilename,
    Remote_URL_Of_zh,
    Remote_URL_Of_en,
    Content_Type,
    Sqlite_Pathname,
    Sqlite_Language
};
const char *g_str[9] = {
    NULL,
    NULL,
    NULL,
    NULL,
    "https://www.google.com/speech-api/v2/recognize?lang=zh-CN&maxresults=12&key=AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw",
    "https://www.google.com/speech-api/v2/recognize?lang=en-US&maxresults=12&key=AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw",
    "Content-Type: audio/L16; rate=8000",
    "/cfg/etc/ucm_config.db",
    "select * from language_settings;"
};

static int Get_Lang_from_DB(char **language)
{
    int fd = 0;
    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int result = 0;
    char *language_settings = NULL;
    fd = sqlite3_open(g_str[Sqlite_Pathname], &db);
    if (fd)
    {
        sqlite3_close(db);
        return -1;
    }

    char **array = NULL;
    const char * sql = g_str[Sqlite_Language];
    result = sqlite3_get_table(db, sql, &array, NULL, NULL, &zErrMsg);
    if (result != 0)
    {
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
    sqlite3_free_table(array);
    sqlite3_close(db);
    *language = language_settings;
    return 0;
}

static void Get_Upload_Filename(char *path_name1, char *path_name2)
{
    char *str = strrchr(path_name1, '/');
    str++;
    sprintf(filename, "%s-%ld.wav", str, pthread_self());
    g_str[Upload_Filename] = filename;
    sprintf(pathfilename, "%s-%ld.wav", path_name1, pthread_self());
    g_str[Upload_File_Path] = pathfilename;
    sprintf(headfile, "%s/head-%ld.out", path_name2, pthread_self());
    sprintf(bodyfile, "%s/body-%ld.out", path_name2, pthread_self());
    g_str[receive_headerfilename] = headfile;
    g_str[receive_bodyfilename] = bodyfile;
    ast_log(LOG_NOTICE, "'%s'\n'%s'\n'%s'\n'%s'\n", g_str[receive_headerfilename], g_str[receive_bodyfilename], g_str[Upload_Filename], g_str[Upload_File_Path]);
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    if (written <= 0)
    {
        ast_log(LOG_NOTICE, "Write to '%s' '%d' bit\n", (char *)ptr, written);
    }
    return written;
}

static unsigned long get_file_size(const char *path)
{
    unsigned long filesize = -1;
    struct stat statbuff;
    if(stat(path, &statbuff) < 0){
        ast_log(LOG_WARNING, "Get '%s' file size is false\n", path);
        return filesize;
    }else{
        filesize = statbuff.st_size;
    }
    return filesize;
}

static int trans_vioce()
{
    Get_Upload_Filename(path_name1, path_name2);
    unsigned long filesize = get_file_size(g_str[1]);
    if (filesize >= 80000)
    {
        ast_log(LOG_WARNING, "file size is '%ld', so too big\n", filesize);
        return -1;
    }
    CURL *curl = NULL;
    CURLcode res;

    FILE * headerfile = NULL;
    FILE * bodyfile = NULL;

    struct curl_httppost * formpost = NULL;
    struct curl_httppost * lastptr = NULL;
    struct curl_slist * http_header = NULL;
    curl_global_init(CURL_GLOBAL_ALL);

    /* Fill in the file upload field */
    curl_formadd( &formpost,
                &lastptr,
                CURLFORM_COPYNAME,
                g_str[Upload_Filename],
                CURLFORM_FILE,
                g_str[Upload_File_Path],
                CURLFORM_END);

    curl = curl_easy_init();
    http_header = curl_slist_append(http_header, g_str[Content_Type]);
    if(curl)
    {
        ast_log(LOG_WARNING, "Translator Speech to google Speech API starting\n");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);

        /***********************language select Chinese or English**********************************/
        if (select_lang == ENGLISH)
            curl_easy_setopt(curl,CURLOPT_URL, g_str[Remote_URL_Of_en]);
        else if (select_lang == CHINESE)
            curl_easy_setopt(curl,CURLOPT_URL, g_str[Remote_URL_Of_zh]);
        else
            curl_easy_setopt(curl,CURLOPT_URL, g_str[Remote_URL_Of_en]);

        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        headerfile = fopen(g_str[receive_headerfilename],"wb");
        if (headerfile == NULL)
        {
            ast_log(LOG_WARNING, "open '%s' file is false\n", g_str[receive_headerfilename]);
            curl_easy_cleanup(curl);
            return -1;
        }

        bodyfile = fopen(g_str[receive_bodyfilename],"wb");
        if (bodyfile == NULL)
        {
            ast_log(LOG_WARNING, "open '%s' file is false\n", g_str[receive_bodyfilename]);
            curl_easy_cleanup(curl);
            return -1;
        }

        curl_easy_setopt(curl,   CURLOPT_WRITEHEADER, headerfile);

        curl_easy_setopt(curl,   CURLOPT_WRITEDATA, bodyfile);

        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));

        fclose(headerfile);
        fclose(bodyfile);
        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all (http_header);
        curl_global_cleanup();
    }
    return 0;
}

static int sound_exec(struct ast_channel *chan, const char *data)
{
    char *language = NULL;
    if (Get_Lang_from_DB(&language) == -1)
    {
        ast_log(LOG_WARNING, "open language_settings file is false\n");
        return -1;
    }
    if (strcmp(language, "zh") == 0)
    {
        select_lang = CHINESE;
        free(language);
        language = NULL;
    }
    else
    {
        select_lang = ENGLISH;
        free(language);
        language = NULL;
    }

    int res = 0;
    int flag = 0;
    char *frext = NULL, *opts[0];
    char *parse = NULL, *dir = NULL, *file = NULL;
    char tmp[256];
    int i = 0;
    int terminator = '#';
    struct ast_filestream *s = NULL;
    struct ast_frame *f = NULL;
    struct ast_dsp *sildet = NULL;
    int totalsilence = 0;
    int dspsilence = 0;
    int silence = 0;
    int maxduration = 0;
    int ioflags = 0;
    int waitres = 0;
    struct ast_flags flags = { 0, };
    AST_DECLARE_APP_ARGS(args,
        AST_APP_ARG(filename);
        AST_APP_ARG(silence);
        AST_APP_ARG(maxduration);
        AST_APP_ARG(options);
    );

    if (ast_strlen_zero(data))
    {
        ast_log(LOG_WARNING, "Sound requires an parameter (filename) \n");
        pbx_builtin_setvar_helper(chan, "Sound_STATUS", "ERROR");
        return -1;
    }

    parse = ast_strdupa(data);
    AST_STANDARD_APP_ARGS(args, parse);
    if (args.argc == 4)
    {
        ast_app_parse_options(app_opts, &flags, opts, args.options);
    }

    if (!ast_strlen_zero(args.filename))
    {
        frext = strrchr(args.filename, '.');
        if (!frext)
        {
            frext = strchr(args.filename, ':');
        }
        if (frext)
        {
            *frext = '\0';
            frext++;
        }
    }

    if (!frext)
    {
        ast_log(LOG_WARNING, "No extension specified to filename! \n");
        pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "ERROR");
        return -1;
    }
    if (args.silence)
    {
        if ((sscanf(args.filename, "%30d", &i) == 1) && (i > -1))
        {
            silence = i * 1000;
        }
        else if (!ast_strlen_zero(args.silence))
        {
            ast_log(LOG_WARNING, "'%s' is not a valid silence duration\n", args.silence);
        }
    }
    if (args.maxduration)
    {
        if ((sscanf(args.maxduration, "%30d", &i) == 1) && (i > -1))
        {
            maxduration = i * 1000;
        }
        else if (!ast_strlen_zero(args.maxduration))
        {
            ast_log(LOG_WARNING, "'%s' is not a valid maximum duration\n", args.maxduration);
        }
    }
    if (!ast_test_flag(&flags, OPTION_QUIET))
    {
        res = ast_streamfile(chan, "beep", chan->language);
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
    ast_copy_string(tmp, args.filename, sizeof(tmp));
    dir = ast_strdupa(tmp);
    sprintf(tmp, "%s-%ld", tmp, pthread_self());
    strcpy(path_name1, dir);
    if ((file = strrchr(dir, '/')))
    {
        *file++ = '\0';
    }
    ast_mkdir(dir, 0777);
    strcpy(path_name2, dir);

    ioflags = ast_test_flag(&flags, OPTION_APPEND) ? O_CREAT | O_APPEND | O_WRONLY : O_CREAT | O_TRUNC | O_WRONLY;

    s = ast_writefile(tmp, frext, NULL, ioflags, 0, AST_FILE_MODE);
    if (!s)
    {
        ast_log(LOG_WARNING, "Could not create file %s\n", args.filename);
        pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "ERROR");
        return -1;
    }
    ast_indicate(chan, AST_CONTROL_VIDUPDATE);
    if (maxduration <= 0)
    {
        maxduration = -1;
    }

    unsigned long filesize = 0;

    while ((waitres = ast_waitfor(chan, maxduration)) > -1)
    {
        if (maxduration > 0)
        {
            if(waitres == 0)
            {
                pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "TIMEOUT");
                break;
            }
            maxduration = waitres;
        }
        f = ast_read(chan);
        if (!f)
        {
            res = -1;
            break;
        }
        if (f->frametype == AST_FRAME_VOICE)
        {
            filesize++;
            if (225 == filesize)
            {
                ast_log(LOG_WARNING, "file size is '%ld', Delay has to\n", filesize);
                flag = 1;
                break;
            }

            res = ast_writestream(s, f);

            if (res)
            {
                ast_log(LOG_WARNING, "Problem writing frame\n");
                ast_frfree(f);
                pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "ERROR");
            }

            if (silence > 0)
            {
                dspsilence = 0;
                ast_dsp_silence(sildet, f, &dspsilence);
                if (dspsilence)
                {
                    totalsilence = dspsilence;
                } else
                {
                    totalsilence = 0;
                }
                if (totalsilence > silence)
                {
                    ast_frfree(f);
                    pbx_builtin_setvar_helper(chan, "RECORD_STATUS", "SILENCE");
                    break;
                }
            }
        }
        else if ((f->frametype == AST_FRAME_DTMF) && ((f->subclass.integer == terminator) || (ast_test_flag(&flags, OPTION_ANY_TERMINATE))))
        {
            ast_frfree(f);
            pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "DTMF");
            flag = 1;
            break;
        }
        ast_frfree(f);
    }

    if (!f)
    {
        ast_debug(1, "Got hangup\n");
        res = -1;
        pbx_builtin_setvar_helper(chan, "SOUND_STATUS", "HANGUP");
    }
    ast_closestream(s);

    if (!ast_test_flag(&flags, OPTION_QUIET))
    {
        res = ast_streamfile(chan, "transbeep", chan->language);
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

    if (1 == flag)
    {
        ast_log(LOG_NOTICE, "Speech to Text starting\n");
        trans_vioce();
    }

    res = remove(g_str[1]);
    if (res == 0)
    {
        ast_log(LOG_NOTICE, "Remove '%s' file success\n", g_str[1]);
    }

    return res;
}

static int unload_module(void)
{
    return ast_unregister_application(app);
}

static int load_module(void)
{
    return ast_register_application_xml(app, sound_exec);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Trivial Sound Application");
