#ifndef __PO_TIME_H__
#define __PO_TIME_H__

#include <time.h>
#include <string.h>

#include "configure.h"

// This module depends on assert
#include "aSsert.h"


#if 1

// Low res uses less system resources
#  define PO_CLOCK       CLOCK_REALTIME_COARSE

// fraction of seconds to show in poTime_get()
#  define PO_PRECISION  0.01

#else

# error "don't do this"
// High res uses more system resources
// DO NOT USE THIS CODE, just for testing
#  define PO_CLOCK       CLOCK_REALTIME

// fraction of seconds to show in poTime_get()
#  define PO_PRECISION  0.00000001

#endif


static inline
char *poTime_get(char *buf, size_t bufLen)
{
    DASSERT(bufLen > 0);
    struct timespec t;
    // As of today, Jun 24 2015, the resolution looks to be around 1/100
    // of a second.
    if(ASSERT(clock_gettime(PO_CLOCK, &t) == 0))
    {
        strncpy(buf, "no time", bufLen);
        buf[bufLen-1] = '\0';
        return buf;
    }
    DASSERT(t.tv_nsec < 1.0e+9);

    double nsec;
    // save just some digits // stop funny rounding
    nsec = (t.tv_nsec + 0.5)*(1.0e-9/PO_PRECISION);
    t.tv_nsec = nsec;

    snprintf(buf, bufLen, "%ld.%2.2ld", t.tv_sec, t.tv_nsec);
    return buf;
}

static inline
double poTime_getDouble()
{
    struct timespec t;
    if(ASSERT(clock_gettime(PO_CLOCK, &t) == 0))
        return 0.0;
    DASSERT(t.tv_nsec < 1.0e+9);

    return t.tv_sec + (t.tv_nsec + 0.5) * 1.0e-9;
}

#undef PO_CLOCK
#undef PO_PRECISION

// Don't use this in the server.
static inline
double poTime_getRealDouble()
{
    struct timespec t;
    if(ASSERT(clock_gettime(CLOCK_REALTIME, &t) == 0))
        return 0.0;
    DASSERT(t.tv_nsec < 1.0e+9);

    return t.tv_sec + (t.tv_nsec + 0.5) * 1.0e-9;
}



#endif // #ifndef __PO_TIME_H__
