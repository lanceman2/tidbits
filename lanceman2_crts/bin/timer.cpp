#include <time.h>
#include <stdio.h>

#include "debug.h"
#include "timer.hpp"


CRTSTimer::CRTSTimer(void)
{
    ASSERT(clock_gettime(CLOCK_MONOTONIC_RAW, &_t) == 0, "");
}
