#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/if_tun.h>

#include "debug.h"
#include "tun_alloc.hpp"
#include "getTunViaUSocket.hpp"


static int usage(const char *argv0)
{
    fprintf(SPEW_FILE,
        "\n"
        "  Usage: %s X.X.X.X/N ADDRESS PORT\n"
        "\n"
        "     Create a TUN device that is connected to the local X.X.X.X/N sub net and listen for\n"
        "  UDP/IP packets on address IP4 and port PORT; and then relay data from X.X.X.X/N to\n"
        "  ADDRESS PORT and vise versa.  For example:\n"
        "\n"
        "            %s 10.0.0.0/31 thor.com 5544\n"
        "\n",
        argv0, argv0);
    return 1;
}


static void badSigCatcher(int sig)
{
    ASSERT(0, "caught signal %d", sig);
}

// These are set in the main thread and then never change after they are
// set.
static int tunFd, udpFd;


// return true when we are done due to error or 0 write.
static bool writen(int fd, const void *buf, size_t n)
{
    errno = 0;
    ssize_t ret;

    DASSERT(n, "");

    while(n)
    {
        ret = write(fd, buf, n);
        if(ret < 1) break;
        n -= ret;
        buf = (void *)( ((unsigned char *)buf) + ret);
    }

    if(ret < 1)
    {
        if(ret < 0)
            ERROR("write(fd=%d,,) failed", fd);
        else
            INFO("write(fd=%d,,) finished", fd);
        return true;
    }
    return false;
}


static inline void *readWrite(int rfd, int wfd)
{
    ssize_t rret;
    const size_t len = 256;
    char buf[len];

    while((rret = read(rfd, buf, len)) > 0)
    {
        write(1, buf, rret);
        if(writen(wfd, buf, rret)) break;
    }

    if(rret < 0)
        ERROR("read(fd=%d,,) failed", rfd);
    else if(rret == 0)
        INFO("read(fd=%d,,) finished", rfd);

    return 0;
}


static void *inThreadCallback(void *ptr)
{
    return readWrite(udpFd, tunFd);
}


static void *outThreadCallback(void *ptr)
{
    return readWrite(tunFd, udpFd);
}



int main(int argc, const char **argv)
{
    // This will hang the process or thread if we catch the following
    // signals, so we can debug it and see what was wrong if we're
    // lucky.
    ASSERT(signal(SIGSEGV, badSigCatcher) == 0, "");
    ASSERT(signal(SIGABRT, badSigCatcher) == 0, "");
    ASSERT(signal(SIGFPE,  badSigCatcher) == 0, "");

    // argv[1] = X.X.X.X/N  argv[2] = ADDRESS  argv[3] = PORT

    if(argc != 4 || argv[1][0] == '-')
        // -h --help or not valid anyway
        return usage(argv[0]);

    tunFd = getTunViaUSocket(argv[1]);
    if(tunFd < 0)
        return 1; // FAIL

     errno = 0;
    udpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpFd < 0)
    {
        ERROR("socket(AF_INET, SOCK_DGRAM, 0)=%d failed", udpFd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    long port = strtol(argv[3], 0, 10);
    if(errno || port < 0 || port > 0xFFFE)
    {
        ERROR("Bad port number: %s", argv[3]);
        return 1;
    }
    DSPEW("Got port=%hu", (unsigned short) port);
    addr.sin_port = htons((unsigned short) port);
    inet_aton(argv[2], &addr.sin_addr);

    if(bind(udpFd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        ERROR("bind() to address \"%s:%s\" failed", argv[2], argv[3]);
        return 1;
    }
    INFO("Server bound to address \"%s:%s\"", argv[2], argv[3]);

    fprintf(SPEW_FILE, "Terminating this program will terminate the TUNNEL interface.\n");


    pthread_t inThread, outThread;

    ASSERT((errno = pthread_create(&inThread, 0, inThreadCallback, 0)) == 0, "");
    ASSERT((errno = pthread_create(&outThread, 0, outThreadCallback, 0)) == 0, "");

    // Wait for threads to finish.

    ASSERT((errno = pthread_join(inThread, 0)) == 0, "");
    ASSERT((errno = pthread_join(outThread, 0)) == 0, "");

    NOTICE("Main thread exiting");

    return 0;
}
