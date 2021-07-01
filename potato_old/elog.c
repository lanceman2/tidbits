#ifndef _GNU_SOURCE
#  define _GNU_SOURCE // stop vim from complaining
#endif
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#include "elog.h"


#ifdef PO_ELOG_NONE
#  error "PO_ELOG_NONE is defined"
#  error "this file [log.c] is not needed to compile your program"
#endif


void _poELogV(
        const char *pre, FILE *fstream,
        const char *sourceFile, int sourceLine,
        const char *func, const char *format, va_list ap)
{
    char t[42] = "";
    size_t len;

#ifdef PO_ELOG_TIMESTAMP
    poTime_get(t, sizeof(t));
    len = strlen(t);
    if(len <= sizeof(t)-2)
    {
        t[len] = ':';
        t[len+1] = '\0';
    }
#endif

    char buf[256];

    if(errno)
    {
        char error[128], *e;
        e = strerror_r(errno, error, sizeof(error));
        snprintf(buf, sizeof(buf), "%s%s%s:%d:%s():tid %ld:errno=%d:%s: ",
            t, pre, sourceFile, sourceLine, func, gettid(), errno, e);
    }
    else
        snprintf(buf, sizeof(buf), "%s%s%s:%d:%s():tid %ld::: ",
            t, pre, sourceFile, sourceLine, func, gettid());

    len = strlen(buf);
    vsnprintf(&buf[len], sizeof(buf) - len, format, ap);
    fprintf(fstream, "%s", buf);
}

void _poELog(
        const char *pre, FILE *fstream,
        const char *sourceFile, int sourceLine,
        const char *func, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    _poELogV(pre, fstream, sourceFile, sourceLine, func, format, ap);
    va_end(ap);
}
