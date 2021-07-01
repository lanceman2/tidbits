#ifndef __PO_ASSERT_H__
#define __PO_ASSERT_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "configure.h"


#ifdef PO_ASSERT_TERMINAL
#  ifdef PO_ASSERT_SLEEP
#    error "PO_ASSERT_TERMINAL and PO_ASSERT_SLEEP are both defined"
#  endif
#endif

#ifndef PO_DEBUG
#  ifndef PO_ASSERT_SLEEP
#    define PO_ASSERT_TERMINAL
#  endif
#endif


#ifdef PO_ELOG_ERROR
#  include "_elog.h"
#endif

static inline
bool _poAssert(const char *file, int line,
    const char *func, bool bool_arg,
    const char *arg, const char *format, ...)
{
    if(bool_arg) return false; // success

    char pre[256];
#ifdef PO_ASSERT_TERMINAL
#  ifdef PO_DEBUG
    snprintf(pre, sizeof(pre),
            "ASSERT(%s)FAILED- SLEEPING loop for debugger: ",
            arg);
#  else
    snprintf(pre, sizeof(pre),
            "ASSERT(%s)FAILED- EXITING with error: ",
            arg);
#  endif
#else
    snprintf(pre, sizeof(pre),
            "ASSERT(%s)FAILED: ",
            arg);
#endif

    va_list ap;
    va_start(ap, format);
#ifdef PO_ELOG_ERROR
    _poELogV(pre, PO_ELOG_FILE, file, line, func, format, ap);
#else
    size_t len;
    len = strlen(pre);
    snprintf(&pre[len], sizeof(pre), "%s:%d:%s():pid=%d:",
            file, line, func, getpid());
    len = strlen(pre);
    vsnprintf(&pre[len], sizeof(pre), format, ap);
    fprintf(stderr, "%s", pre);
#endif
    va_end(ap);


#ifdef PO_ASSERT_TERMINAL
    exit(1);
#else
    int i = 1; // you can change i in the debugger
    while(i)
        sleep(10);
#endif

    // We never get here.
    return true; // error
}




// The ASSERT family of macro functions are used so often that we do not add
// a PO_ prefix in the name, so for example what would be
// PO_ASSERT_DEBUG_VERBOSE() is DVASSERT(), and so on.


extern
bool _poAssert(const char *file, int line,
    const char *func, bool bool_arg,
    const char *arg, const char *format, ...)
    __attribute__((format(printf,6,7)));


#define ASSERT(x)               \
        _poAssert(__FILE__, __LINE__, __func__, (x), #x, "\n")

#define VASSERT(x, fmt, ...)    \
        _poAssert(__FILE__, __LINE__, __func__, (x), #x, fmt, ##__VA_ARGS__)


#ifdef PO_DEBUG

#  define DASSERT(x)             ASSERT(x)
#  define DVASSERT(x, fmt, ...)  VASSERT((x), fmt, ##__VA_ARGS__)

#else

#  define DASSERT(x)                /*empty macro*/
#  define DVASSERT(x, fmt, ...)     /*empty macro*/

#endif


// this module conditionally depends on elog
#ifndef PO_ELOG_NONE
#  include "elog.h"
#endif


#endif // #ifndef __PO_ASSERT_H__

