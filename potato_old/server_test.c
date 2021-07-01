// Tests potato server without the fancy service buffering.
#include <stdio.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "server.h"
#include "potato.h"


int main(int argc, char **argv)
{
    if(argc == 2)
        // Get listener sockets from server at pid = argv[1]
        if(poTato_signalHandOff(argv[1], true))
            return 1; // error

    struct POServer s;

    if(poServer_create(&s) == NULL) return 1;

    if(poServer_listenerSetup(&s, "4444", "0.0.0.0",
                poServer_echoRequest, NULL))
        return 1;
    if(poServer_listenerSetup(&s, "5555", "0.0.0.0",
                poServer_echoRequest, NULL))
        return 1;

    if(getuid() == 0)
    {
        if(ASSERT(setuid(SERVER_TEST_UID) == 0
                    && getuid() == SERVER_TEST_UID)) return 1;

        DLOG("switched to running as uid %d\n",
                SERVER_TEST_UID);
    }

    while(poServer_isRunning(&s))
        poServer_doNextEvents(&s);

    poServer_shutdown(&s);
    poServer_destroy(&s);

    DLOG("%s -Server Exiting Now\n", argv[0]);

    return 0;
}
