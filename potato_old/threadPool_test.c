#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "threadPool.h"
#include "random.h"


static __thread bool running = true;

static void sigHandler(int sig)
{
    errno = 0;
    WLOG("Server caught signal %d\n", sig);
    if(running == false)
        exit(1);
    running = false;
}


void* callback(size_t count)
{
    fprintf(stderr, "  ==== %zu === thread id %ld enter user callback\n",
            count, gettid());
    if(count < 47)
        usleep(10000);
    fprintf(stderr, "  >>>>>> %zu <<<<< thread id %ld exit user callback\n",
            count, gettid());

    return NULL;
}


int main(int argc, char **argv)
{
    size_t runCount = 0;
    int i;
    ASSERT(signal(SIGINT, sigHandler) == 0);

    struct POThreadPool p;
    poThreadPool_create(&p);

    for(i=0;i<500;++i)
    {
        // usleep(2000000);
        poThreadPool_runTask(&p, (void *(*)(void *data)) callback,
                (void *) runCount++);
    }

    while(running)
    {
        usleep(100000);
        poThreadPool_runTask(&p, (void *(*)(void *data)) callback,
                (void *) runCount++);
    }

#if 1

    for(i=0;i<50;++i)
        poThreadPool_runTask(&p, (void *(*)(void *data)) callback,
                (void *) runCount++);

#endif

    poThreadPool_destroy(&p);

    WLOG("%s Exiting\n", argv[0]);
    return 0;
}
