#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

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

int main()
{
    struct json_object* msg = NULL;
    char tmp[1024] = {0};

    msg = json_object_new_object();
    if (msg)
    {
        json_object_object_add(msg, "firstName", json_object_new_string("chzh"));
        json_object_object_add(msg, "lastName", json_object_new_string("deng"));
    }

    // json object convert to string.
    snprintf(tmp,sizeof(tmp)-1, "%s", json_object_get_string(msg));
    json_object_put(msg);

    printf("json object string: %s\n", tmp);

    // string convert to json object.
    char *first = hscJsonContentParse(tmp, "firstName");
    printf("firstName: %s\n", first);
    free(first);

    return 0;
}

/***
:~/github/example/C/module/app_json $ ./json_str 
json object string: { "firstName": "chzh", "lastName": "deng" }
firstName: chzh
***/
