#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "http.h"
#include "potato.h"



static int usage(const char *argv0)
{
    printf("\n"
            "  Usage: %s ", argv0);
    poTato_printOptionUsage(stdout);
    printf(
            "\n"
            "\n"
            "    Run the %s test server.\n"
            "\n"
            "       OPTIONS\n",
            argv0);
    poTato_printOptionHelp(stdout);
    return 1;
}

#define MAX_MESSAGE_LENGTH  1024

int main(int argc, char **argv)
{
    if(poTato_doOptions((const char **) argv + 1,
                PO_OPTION_COPY_LISTENERS|
                PO_OPTION_REPLACE_LISTENERS|
                PO_OPTION_FORK,
                NULL))
        return usage(argv[0]);

    // Declare some important structures on the stack.
    struct POServer server;
    struct POTls tls;
    struct POService service[PO_SERVER_NUM_LISTENERS];

    if(!poTls_create(&tls, "key.pem", "cert.pem")) return 1;

    if(!poServer_create(&server)) goto fail1;

    if(poService_create(&service[0], &server,
                NULL /* no TLS*/, 
                HTTP_PORT, "0.0.0.0",
                MAX_MESSAGE_LENGTH,
                poHttp_redirectToHttps,
                NULL)) goto fail2;

    if(poService_create(&service[1], &server,
                &tls/* with TLS*/,
                HTTPS_PORT, "0.0.0.0",
                MAX_MESSAGE_LENGTH,
                poHttp_read, NULL)) goto fail2;

    if(poTato_doOptionsFork())
        goto fail2;

    while(poServer_isRunning(&server))
        poServer_doNextEvents(&server);

    poServer_shutdown(&server);
    poServer_destroy(&server);

    // All TLS sessions should be cleaned up before calling
    // poTls_destroy().  poServer_destroy() should do that
    // through the connection cleanup callbacks.
    poTls_destroy(&tls);

    int retStatus;

    retStatus = poTato_removeChildren();

    ILOG("normal exiting with status %d\n", retStatus);

    return retStatus; // returns number of failed children

fail2:
    poServer_destroy(&server);
fail1:
    poTls_destroy(&tls);

    ILOG("normal exiting with status 1\n");
    return 1; // one bad child to add to exit status
}
