#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

#define JSON_SAFE_FREE(tmp) if(tmp){json_object_put(tmp);tmp=NULL;}

int main()
{
    struct json_object* msg_json = NULL;
    char tmp[1024] = "[{\"first\": \"aaa\",\"last\": \"111\"},{\"first\": \"bbb\",\"last\": \"222\"},{\"first\": \"ccc\",\"last\": \"333\"}]";
    printf("json object array: %s\n", tmp);
    printf("======================================================\n");

    msg_json = json_tokener_parse(tmp);
    if (msg_json)
    {
        int i = 0;
        struct json_object *sub = NULL;
        struct array_list *pri_list = json_object_get_array(msg_json);

        do
        {
            sub = array_list_get_idx(pri_list, i++);
            if (sub != NULL)
            {
                printf("object: %s\n",json_object_get_string(sub));
                json_object_object_foreach(sub, key, value)
                {
                    printf("key: %s, value: %s \n", key, json_object_get_string(value));
                }
                printf("-----------------------------------------------------------\n");
            }
        }while(sub != NULL);
    }
    JSON_SAFE_FREE(msg_json);

    return 0;
}



/***
:~/github/example/C/module/app_json $ gcc json_array.c -o json_array -ljson-c
:~/github/example/C/module/app_json $ ./json_array                           
json object array: [{"first": "aaa","last": "111"},{"first": "bbb","last": "222"},{"first": "ccc","last": "333"}]
======================================================
object: { "first": "aaa", "last": "111" }
key: first, value: aaa 
key: last, value: 111 
-----------------------------------------------------------
object: { "first": "bbb", "last": "222" }
key: first, value: bbb 
key: last, value: 222 
-----------------------------------------------------------
object: { "first": "ccc", "last": "333" }
key: first, value: ccc 
key: last, value: 333 
-----------------------------------------------------------
***/
