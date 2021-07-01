#ifndef __PO_ELOG_H__
#define __PO_ELOG_H__

// The ELOG family of macro functions are used so often that we do not add
// a PO_ prefix in the name, so for example what would be
// PO_ELOG_WARNING() is WLOG(), and so on.

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "configure.h"


#ifndef PO_ELOG_NO_TIMESTAMP
// PO_ELOG_TIMESTAMP is on by default
#  ifndef PO_ELOG_TIMESTAMP
#    define PO_ELOG_TIMESTAMP
#  endif
#else
#  undef PO_ELOG_NO_TIMESTAMP
#endif


// This module conditionally depends on the time module
#ifdef PO_ELOG_TIMESTAMP
#  include "tIme.h"
#endif


#ifdef PO_ELOG_DEBUG
#  ifndef PO_ELOG_INFO
#    define PO_ELOG_INFO
#  endif
#endif
#ifdef PO_ELOG_INFO
#  ifndef PO_ELOG_NOTICE
#    define PO_ELOG_NOTICE
#  endif
#endif
#ifdef PO_ELOG_NOTICE
#  ifndef PO_ELOG_WARNING
#    define PO_ELOG_WARNING
#  endif
#endif
#ifdef PO_ELOG_WARNING
#  ifndef PO_ELOG_ERROR
#    define PO_ELOG_ERROR
#    undef PO_ELOG_NONE
#  endif
#endif
#ifdef PO_ELOG_NONE
#  ifdef PO_ELOG_ERROR
     // PO_ELOG_NONE is overridden by all others spew levels
#    undef PO_ELOG_NONE
#  endif
#endif


// The default if no PO_ELOG_(*spew level*) was defined
#ifndef PO_ELOG_ERROR
#  ifndef PO_ELOG_NONE
#    define PO_ELOG_WARNING
#    define PO_ELOG_ERROR
#  endif
#endif


// ASSERT and family do the fatal logging
// so it's: debug, info, notice, warning, error, and assertions


#ifdef PO_ELOG_ERROR
#  include "_elog.h"
#endif

// What's faster than any code?  No code, like CPP empty macro.
// With logging off there is no logging function call.


#ifdef PO_ELOG_DEBUG
#  define  DLOG(fmt, ...)                                    \
                            _poELog(                     \
                                    "DEBUG:", PO_ELOG_FILE,     \
                                    __BASE_FILE__, __LINE__,      \
                                    __func__, fmt, ##__VA_ARGS__)
#else
#  define DLOG(fmt, ...)   /*empty macro*/
#endif


#ifdef PO_ELOG_INFO
#  define  ILOG(fmt, ...)                                    \
                            _poELog(                     \
                                    "INFO:", PO_ELOG_FILE,    \
                                    __BASE_FILE__, __LINE__,      \
                                    __func__, fmt, ##__VA_ARGS__)
#else
#  define ILOG(fmt, ...)   /*empty macro*/
#endif


#ifdef PO_ELOG_NOTICE
#  define  NLOG(fmt, ...)                                    \
                            _poELog(                     \
                                    "NOTICE:", PO_ELOG_FILE,    \
                                    __BASE_FILE__, __LINE__,      \
                                    __func__, fmt, ##__VA_ARGS__)
#else
#  define NLOG(fmt, ...)   /*empty macro*/
#endif


#ifdef PO_ELOG_WARNING
#  define  WLOG(fmt, ...)                                    \
                            _poELog(                     \
                                    "WARNING:", PO_ELOG_FILE,   \
                                    __BASE_FILE__, __LINE__,      \
                                    __func__, fmt, ##__VA_ARGS__)
#else
#  define WLOG(fmt, ...)   /*empty macro*/
#endif


#ifdef PO_ELOG_ERROR
#  define  ELOG(fmt, ...)                                    \
                            _poELog(                     \
                                    "ERROR:", PO_ELOG_FILE,     \
                                    __BASE_FILE__, __LINE__,      \
                                    __func__, fmt, ##__VA_ARGS__)
#else
#  define ELOG(fmt, ...)   /*empty macro*/
#endif

static inline
long gettid(void)
{
    return syscall(SYS_gettid);
}


#endif // #ifndef __PO_ELOG_H__
