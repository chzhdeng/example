/*
 ********* for exmample ********
 chzhdeng@chzhdeng:~/gitHub/example/C/module/app_simulation_mysql$ ./simulation_mysql 
 Enter Password:
 Welcome to the MySQL monitor.  Commands end with ; 
 mysql>aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
 mysql>bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
 bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
 mysql>
 mysql>
 mysql>
 mysql>quit
 
 exit process !
 chzhdeng@chzhdeng:~/gitHub/example/C/module/app_simulation_mysql$ 
 *
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_option(char *opt)
{
    if(opt && strlen(opt) > 0 && strcmp(opt, "") != 0)
    {
        printf("%s\n",opt);
    }
}

int main(int argc, char * argv[])
{
    char str[256] = {0};
    char store[256] = "a123";
    char *pass = NULL;


    pass = getpass("Enter Password:");
    if (pass)
    {
        snprintf(str,sizeof(str)-1, "%s", pass);
        //printf("intput pass: %s\n", str);
    }
    else
    {
        printf("input error!!!!!!!!!\n");
    }

    
    if (strcmp(str, store) == 0)
    {
        printf("Welcome to the MySQL monitor.  Commands end with ; \n");
    }
    else
    {
        printf("ERROR 1045 (28000): Access denied for user 'root'@'localhost' (using password: YES)\n");
        return -1;
    }

    char opt[256] = {0};
    char tmp[256] = {0};
    int g_run = 1;
    int len_input=0;
    while(g_run)
    {
        memset(tmp, 0, sizeof(tmp));
        memset(opt, 0, sizeof(opt));
        printf("mysql>");
        fgets(tmp, sizeof(tmp), stdin);
        strncpy(opt,tmp, strlen(tmp)-1);

        if (strcasecmp(opt, "quit") == 0)
        {
            g_run = 0;
            break;
        }
        handle_option(opt);
    }
    printf("\nexit process !\n ");

    return 0;
}
