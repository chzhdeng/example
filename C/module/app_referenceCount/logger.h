#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>

//#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <regex.h>

#define        MAX_SIZE    65535
#define logger_free        free
#define logger_alloc    malloc

enum {
    VERB        =    1 << 0,
    DEBUG       =    1 << 1,
    NOTICE      =    1 << 2,
    WARN        =    1 << 3,
    ERROR       =    1 << 4
};

struct logger_name {
    const char *name;
} logger_str[5] = {
    { .name = "\033[32mVERB\33[0m" },
    { .name = "\033[32;1mDEBUG\33[0m" },
    { .name = "\033[33;1mNOTICE\33[0m" },
    { .name = "\033[31;1mWARN\33[0m" },
    { .name = "\033[31;1mERROR\33[0m"}
};

#define RAII_VAR(vartype, varname, initval, dtor)                                  \
    auto void _dtor_ ## varname (vartype *v);                                      \
    void _dtor_ ## varname (vartype *v) {                                          \
        if (*v != NULL) {                                                          \
            dtor(*v);                                                              \
            (*v) = NULL;                                                           \
        }                                                                          \
    }                                                                              \
    vartype varname    __attribute__((cleanup(_dtor_ ## varname))) = (initval);    \

static struct timeval tv_now(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);

    return t;
}

static void system_time(char *sys_tv)
{
    time_t tt;
    tt = time(NULL);
    struct tm *t = localtime(&tt);
    struct timeval tv = tv_now();

    snprintf(sys_tv, MAX_SIZE, "[%4d-%02d-%02d %02d:%02d:%02d:%03ld]", t->tm_year+1900,
        t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);
}

static int get_tid(void)
{
    return syscall(SYS_gettid);;
}

static int regex_match_file_name(const char *file, char **result)
{
    regex_t reg;
    int nmatch = 1;
    regmatch_t pmatch[1] = {0};

    RAII_VAR(char *, err_buf, logger_alloc(sizeof(char) * 1024), logger_free);
    RAII_VAR(char *, match, logger_alloc(sizeof(char) * 1024), logger_free);

    int err = 0;

    err = regcomp(&reg, "([a-z]|[A-Z]|[0-9]|[_])+\\.[c|(cpp)|h]+$", REG_EXTENDED);
    if (err < 0) {
        memset(err_buf, 0, sizeof(char) * 1024);
        regerror(err, &reg, err_buf, sizeof(err_buf) - 1);
        printf("Compile regex pattern failed because of '%s'!", err_buf);
        return -1;
    }

    err = regexec(&reg, file, nmatch, pmatch, 0);
    if (err == REG_NOMATCH) {
        printf("Not match");
        regfree(&reg);
        return -1;
    } else if (err) {
        regerror(err, &reg, err_buf, sizeof(err_buf) - 1);
        printf("Match regex pattern failed because of '%s'!", err_buf);
        regfree(&reg);
        return -1;
    }

    int len = pmatch[0].rm_eo - pmatch[0].rm_so;
    if(len) {
        memset(match, 0, sizeof(char) * 1024);
        memcpy(match, file + pmatch[0].rm_so, len);
        *result = strdup(match);
    }
    regfree(&reg);

    return 0;
}

static void __logger(int level, const char *file, const char *function, int line, const char *format, ...)
{
    RAII_VAR(char *, log_buffer, logger_alloc(sizeof(char) * MAX_SIZE), logger_free);
    RAII_VAR(char *, sys_tv, logger_alloc(sizeof(char) * 256), logger_free);

    int logger_interger = 0;
    switch (level) {
        case VERB:
            logger_interger = 1;
            break;
        case DEBUG:
            logger_interger = 2;
            break;
        case NOTICE:
            logger_interger = 3;
            break;
        case WARN:
            logger_interger = 4;
            break;
        case ERROR:
            logger_interger = 5;
            break;
        default:
            logger_interger = 2;
            break;
    }

    va_list ap;
    va_start(ap, format);
    vsnprintf(log_buffer, MAX_SIZE - 1, format, ap);
    va_end(ap);

    system_time(sys_tv);

    RAII_VAR(char *, find_file, NULL, logger_free);
    regex_match_file_name(file, &find_file);

    printf("%s %s [%05d]   -- %s:%d  %s\n",
        sys_tv, logger_str[logger_interger - 1].name, get_tid(), find_file ? find_file : file, line, log_buffer);
}

#define    logger(level, ...)                                                    \
    do {                                                                         \
        if (level) {                                                             \
            __logger(level, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);      \
        }                                                                        \
    } while (0)

#endif
