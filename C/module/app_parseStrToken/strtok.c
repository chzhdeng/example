#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char content[]="extension=100&permission=1&first=chzh&last=deng";
    char *save = NULL;
    char *tmp = NULL;
    int len = 0;
    char *seperator = NULL;
    char paramName[64] = {0};
    char paramValue[64] = {0};

    printf("before strtok: \n");
    printf("%s\n",content);
    printf("----------begin----------\n");

    tmp = strtok_r(content, "&", &save);
    while(tmp != NULL)
    {
        if ((seperator = strchr(tmp, '=')) != NULL)
        {
            snprintf(paramName, (seperator - tmp + 1) * sizeof(char), "%s", tmp);
            snprintf(paramValue, sizeof(paramValue)-1, "%s", seperator+1);
            printf("Key: {%s}, Value: {%s}\n", paramName, paramValue);
        }
        tmp = strtok_r(NULL, "&", &save);
    }
    printf("----------end----------\n");

    return 0;
}

/***
before strtok: 
extension=100&permission=1&first=chzh&last=deng
----------begin----------
Key: {extension}, Value: {100}
Key: {permission}, Value: {1}
Key: {first}, Value: {chzh}
Key: {last}, Value: {deng}
----------end----------
***/
