#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

enum Curl_Name {
    Upload_Filename,
    Upload_File_Path,
    Remote_URL,
    User_Agent,
    Content_Type,
    receive_headerfilename,
    receive_bodyfilename
};
const char * g_str[] = {
    "nihao.wav",
    "/home/dengchaozhen/ReadFile/nihao.wav",
    "https://www.google.com/speech-api/v2/recognize?lang=zh-CN&maxresults=12&key=AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw",
    "user-agent:Mozilla/5.0",
    "Content-Type: audio/L16; rate=8000",
    "/home/dengzhaozhen/WriteFile/head.out",
    "/home/dengzhaozhen/WriteFile/body.out"
};

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

int main()
{
    CURL *curl = NULL;
    CURLcode res;

    FILE * Ptr_headerfile = NULL;
    FILE * Ptr_bodyfile = NULL;

    struct curl_httppost * formpost = NULL;
    struct curl_httppost * lastptr = NULL;
    struct curl_slist * http_header = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    /* Fill in the file upload field */
    curl_formadd( &formpost,
                &lastptr,
                CURLFORM_COPYNAME, g_str[Upload_Filename],
                CURLFORM_FILE, g_str[Upload_File_Path],
                CURLFORM_END);

    curl = curl_easy_init();
    http_header = curl_slist_append(http_header, g_str[User_Agent]);
    http_header = curl_slist_append(http_header, g_str[Content_Type]);
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
        curl_easy_setopt(curl,CURLOPT_URL, g_str[Remote_URL]);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        Ptr_headerfile = fopen(g_str[receive_headerfilename],"wb");
        if (Ptr_headerfile == NULL)
        {
            curl_easy_cleanup(curl);
            return -1;
        }
        Ptr_bodyfile = fopen(g_str[receive_bodyfilename],"wb");
        if (Ptr_bodyfile == NULL)
        {
            curl_easy_cleanup(curl);
            return -1;
        }

        curl_easy_setopt(curl,   CURLOPT_WRITEHEADER, Ptr_headerfile);
        curl_easy_setopt(curl,   CURLOPT_WRITEDATA, Ptr_bodyfile);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));


        fclose(Ptr_headerfile);
        fclose(Ptr_bodyfile);
        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all (http_header);
        curl_global_cleanup();
    }
    return 0;
}
