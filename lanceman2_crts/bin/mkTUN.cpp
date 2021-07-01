/*
  Running examples:

     * terminal 1
     ./mkTUN 111.0.0.0/31 | hexdump -v

     # terminal 2:  TCP will not work so 'nc -u':
     nc -u  111.0.0.1 5444  < /dev/urandom


     # terminal 3
     /sbin/ifconfig

     ls /sys/class/net

     /sbin/route

*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/if_tun.h>

#include "debug.h"
#include "tun_alloc.hpp"
#include "getTunViaUSocket.hpp"

static int usage(const char *argv0)
{
    fprintf(SPEW_FILE,
        "\n"
        "  Usage: %s IP4_SUBNET\n"
        "\n"
        "     This is just a test program that is not installed.\n"
        "\n"
        "     Create a TUN device that is is connected to the IP4 subnet IP4_SUBNET.\n"
        "  For example:\n"
        "\n"
        "            %s 10.0.0.0/24\n"
        "\n",
        argv0, argv0);
    return 1;
}



static void badSigCatcher(int sig)
{
    ASSERT(0, "caught signal %d", sig);
}



int main(int argc, const char **argv)
{
    // This will hang the process or thread if we catch the following
    // signals, so we can debug it and see what was wrong if we're
    // lucky.
    ASSERT(signal(SIGSEGV, badSigCatcher) == 0, "");
    ASSERT(signal(SIGABRT, badSigCatcher) == 0, "");
    ASSERT(signal(SIGFPE,  badSigCatcher) == 0, "");

    if(argc != 2 || argv[1][0] == '-')
        // -h --help or not valid anyway
        return usage(argv[0]);

    int fd = getTunViaUSocket(argv[1]);
    if(fd < 0)
        return 1; // FAIL

    fprintf(SPEW_FILE, "Terminating this program will terminate the interface. fd=%d\n", fd);

#if 1
    ssize_t ret;
    const size_t len = 256;
    char buf[len];

    while((ret = read(fd, buf, len)) > 0)
    {
        fprintf(stderr, "read(%zd)\n", ret);
        write(1, buf, ret);

#if 0 // This will not make sense to the kernel
        if(ret != write(fd, buf, ret))
            fprintf(stderr, "Write(%zd) to tun failed\n", ret);
#endif

    }

    ERROR("read(fd=%d,,)=%zd\n", fd, ret);
#else
    while(1) sleep(1);
#endif

    return 0;
}
