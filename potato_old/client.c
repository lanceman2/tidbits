#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include "sClient.h"
#include "elog.h"
#include "randSequence.h"
#include "threadPool.h"
#include "tIme.h"
#include "util.h"

#define DEFAULT_PORT "4444"

static bool useTLS = false;
static struct sockaddr addr;

static bool running = true;

static pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

static void intCatcher(int sig)
{
    WLOG("caught signal %d\n", sig);
    if(!running) exit(0);
    running = false;
}

static struct POTlsC *tls = NULL;

static
void *go(void *ptr)
{
    struct POSClient c;
    char buf[PO_RANDSEQUENCE_MAX + 1];

    snprintf(buf, sizeof(buf), "%p", ptr);

    int n;
    n =  3 + ((intptr_t) ptr) % 3;


    DLOG("%d loops- buf seed=\"%s\"\n", n, buf);


    if(!poSClient_create(&c, &addr, tls))
        goto fail1;

    while(n)
    {
        ssize_t w;
        size_t l;

        if((w = poClient_sendF(&c, "%s",
                poRandSequence_string(buf, buf, &l))) < PO_RANDSEQUENCE_MIN)
            goto fail;

//fprintf(stderr, "wrote(%zd) \"%s\"\n", w, buf);
    
        poRandSequence_string(buf, buf, &l);

        char *rbuf;

//fprintf(stderr, "waiting for (%zu) %s\n", l, buf);

        rbuf = poSClient_recv(&c, l);
        if(!rbuf) goto fail;
        rbuf[l] = '\0';

//fprintf(stderr, "%s\n", rbuf);

        if(strcmp(buf, rbuf))
        {
            ELOG("failed to get correct response\n");
            goto fail;
        }

        --n;

#if 0
        if(n)
            usleep(300);
#endif
    }

    poSClient_destroy(&c);
    return NULL;

fail:

    poSClient_destroy(&c);

fail1:

    ELOG("client failed\n");
    // Make exit() thread safe.
    pthread_mutex_lock(&exit_mutex);
    exit(1); // fail
}

static
int usage(const char *argv0)
{
    printf("Usage: %s [MAX_CONNECTIONS] [--port PORT] [-host HOST] [--tls]\n"
            "  A client that goes with service_test.\n"
            "  Edit client_conf.h to change it's behavior.\n"
            ,
            argv0);
    return 1;
}


int main(int argc, char ** argv)
{
    char *port = DEFAULT_PORT;
    char *host = "127.0.0.1";
    const char *argv0;
    uint64_t maxCount = 1000000;
    argv0 = *argv;
    while(*(++argv))
    {
        if(strcmp("--port", *argv) == 0)
        {
            if(*(++argv))
                port = *argv;
            else
                return usage(argv0);
        }
        else if(strncmp("--port=", *argv, 7) == 0)
            port = &(*argv)[7];
        else if(strcmp("--host", *argv) == 0)
        {
            if(*(++argv))
                host = *argv;
            else
                return usage(argv0);
        }
        else if(strncmp("--host=", *argv, 7) == 0)
            host = &(*argv)[7];
        else if(strcmp("--tls", *argv) == 0)
            useTLS = true;
        else
        {
            maxCount = strtoull(*argv, NULL, 10);
            if(maxCount < 1)
                return usage(argv0);
        }
    }

    if(poUtil_setAddress(&addr, port, host, __func__))
        return 1; // error

    NLOG("port=%s\n", port);
    NLOG("useTLS=%s\n", useTLS?"true":"false");

    signal(SIGINT, intCatcher);

    struct POTlsC tlsM;
    if(useTLS)
    {
        tls = &tlsM;
        if(!poTlsC_create(tls, "cert.pem"))
            return 1; // error
    }

    struct POThreadPool pool;
    if(!poThreadPool_create(&pool))
        goto fail;

    uint8_t *p = (void *) 0xFFFFFFFF; // just a large seed/counter
    uint64_t count = 0;

    double t;
    t = poTime_getRealDouble();

    while(running)
    {
        // This blocks when it has to, just the way we like it;
        // waiting for the busy threads to come back for more.
        // Thread pools are cool shit.
        poThreadPool_runTask(&pool, go, ++p);
        ++count;
        // netstat shows too many sockets in TIME_WAIT state.
        if(count >= maxCount) break;
    }

    // This will wait for the threads in the pool to finish.
    poThreadPool_destroy(&pool);

    if(useTLS)
        poTlsC_destroy(tls);

    t = poTime_getRealDouble() - t;

    fprintf(stderr, "%"PRIu64" connections %s TLS on port %s"
            " in %lg seconds => average %lg connections/second\n",
            count,
            (useTLS)?"WITH":"WITHOUT",
            port,
            t, count/t);
    
    return 0;

fail:

    if(useTLS)
        poTlsC_destroy(tls);

    return 1;
}
