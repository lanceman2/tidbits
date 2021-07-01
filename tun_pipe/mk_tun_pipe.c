// modprobe tun
// lsmod |grep tun

#include <stdio.h>
#include <stdlib.h>

static void usage(const char *argv0)
{
    printf(
        "  Usage: %s\n\n", argv0);
    printf("  Create a Linux TUN device and make a IP interface to it.\n");
    exit(1);
}



int main(int argc, char **argv)
{

    usage(argv[0]);

    return 0;
}
