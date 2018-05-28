#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*  \"#$%&+,/:;<=>?[]^`{|}
    %5C%22%23%24%25%26%2B%2C%2F%3A%3B%3C%3D%3E%3F%5B%5D%5E%60%7B%7C%7D
*/
/* Escape the string to the URL format, return the newly malloc string
 *********************************************
 * Note: the returned string needs to be freed
 *********************************************
 */
char* urlEscape(const char *src)
{
    const char *ptr = NULL;
    int bit1 = 0, bit2 = 0;
    char *dst = NULL, *tmp = NULL;

    if (src == NULL)
    {
        return NULL;
    }
    dst = (char *)malloc(strlen(src) * 3 + 1);
    if (dst == NULL)
    {
        return NULL;
    }

    memset(dst, 0, strlen(src) * 3 + 1);
    ptr = src;
    tmp = dst;
    while (*ptr != '\0')
    {
        switch (*ptr)
        {
            case '\\':
            case '"':
            case '#':
            case '$':
            case '&':
            case '+':
            case ',':
            case '/':
            case ':':
            case ';':
            case '<':
            case '=':
            case '>':
            case '?':
            case '[':
            case ']':
            case '^':
            case '`':
            case '{':
            case '|':
            case '}':
            case '%':
                *tmp++ = '%';
                bit1 = *ptr / 16;
                bit2 = *ptr % 16;
                if (bit1 >= 10)
                {
                    *tmp++ = 'A' + bit1 - 10;
                }
                else
                {
                    *tmp++ = '0' + bit1;
                }
                if (bit2 >= 10)
                {
                    *tmp++ = 'A' + bit2 - 10;
                }
                else
                {
                    *tmp++ = '0' + bit2;
                }
                break;
            default:
                *tmp++ = *ptr;
                break;
        }
        ++ptr;
    }

    return dst;
}

/* need to free */
/* if a = 0, create the password all by numbers */
/* if a = 1, create the password by numbers, letters and special characters*/
char* createRandomPassword(int a)
{
    char *text[3] = { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ~!@#$%^*", "1234567890" };
    int len;
    char pw[32] = { 0 };
    char *p = NULL;
    int strpos, i;
    struct timeval time;
    int now;

    gettimeofday(&time, NULL);
    now = (int)time.tv_usec;
    srand(now);
    len = (a == 0) ? (rand() % 3 + 4) : (rand() % 9 + 8);

    for (i = 0; i < len; i++)
    {
        now = rand();
        srand(now);

        if (a == 0)
        {
            strpos = 2;
        }
        else if (i < 3)
        {
            strpos = i;
        }
        else
        {
            strpos = rand() % 3;
        }
        p = text[strpos];

        pw[i] = p[rand() % (strlen(text[strpos]))];
    }

    return urlEscape(pw);
}

int main()
{
    char *user_pwd = createRandomPassword(1);
    printf("gen rand password is: %s \n", user_pwd);
    free(user_pwd);
    
    return 0;
}
