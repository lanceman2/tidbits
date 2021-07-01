#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "threadPool.h"
#include "random.h"
#include "tIme.h"


static double tOffset;
static __thread bool running = true;

static void sigHandler(int sig)
{
    errno = 0;
    WLOG("Server caught signal %d\n", sig);
    if(running == false)
        exit(1);
    running = false;
}

struct ThreadData
{
    size_t count;
    uint32_t numThreads;
};

void* callback(size_t count)
{
    //printf("%.20lg %zu\n", poTime_getRealDouble() - tOffset, count);
    usleep(200);
    printf("%.20lg %zu\n", poTime_getRealDouble() - tOffset, count);
    return NULL;
}


int main(int argc, char **argv)
{
    //struct PORandom r;
    //poRandom_init(&r, 0);
    
    tOffset = poTime_getRealDouble();

    size_t runCount = 0;
    int i;
    ASSERT(signal(SIGINT, sigHandler) == 0);

    struct POThreadPool p;
    poThreadPool_create(&p);

    for(i=0;i<200000;++i)
    {
        poThreadPool_runTask(&p, (void *(*)(void *data)) callback,
                (void *) runCount++);
    }


    poThreadPool_destroy(&p);

    WLOG("%s Exiting\n", argv[0]);
    return 0;
}
