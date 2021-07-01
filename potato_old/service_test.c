/*

   You can run this server and then run ./client which will
   connect to it many times and hammer it hard,
   
   ./client may run to fast and you get the connect() error
   errno=99:Cannot assign requested address.  I'd guess that the kernel
   runs out of available socket ports (or addresses) to connect with.
   There seems to be some time needed for the kernel to reset some
   socket resources; you think it would just block the connect() call
   until the resource was available, but no, it does not do it that way.
   I added more code to 'client' that keeps it from failing to connect.
   
   or you can use the below:


   Connect to TCP port with:

nc localhost 4444


   Connect to TLS over TCP port with:

gnutls-cli --insecure localhost -p 5555

   or:

openssl s_client -connect localhost:5555


TO hammer it try:

run='nc localhost 4444 -q1' ; \
j=1; k=1; while [ $j != 60000000 ]; \
do i=0; while [ $i != 10 ] ; do \
spew "hello ${k}" -t0.5 | $run & \
let i=$i+1; let k=$k+1;  done; let j=$j+1 ; \
echo -ne "\n\n       <<<<$j>>>>\n\n"; \
sleep 0.03; done

OR:

run='gnutls-cli --insecure localhost -p 5555' ; \
j=1; k=1; while [ $j != 60000000 ]; \
do i=0; while [ $i != 10 ] ; do let k=$k+1; \
spew "hello ${k}" -t0.5 | $run & \
let i=$i+1;  done; let j=$j+1 ; \
echo -ne "\n\n       <<<<$j>>>>\n\n"; \
sleep 0.1; done

*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "service.h"
#include "potato.h"
#include "randSequence.h"

static __thread char rbuf[PO_RANDSEQUENCE_MAX + 1];

static int usage(const char *argv0)
{
    printf("\n"
            "  Usage: %s [--copy PID] | [--replace PID] | ... & [--fork NUM]\n"
            "\n"
            "    Run a test server (%s).\n"
            "\n"
            "       OPTIONS\n"
            "\n"
            "  --copy PID     get a copy of listening sockets from server with PID\n"
            "\n"
            "  --fork NUM     fork NUM additional potato servers\n"
            "\n"
            "  --replace PID  get a copy of listening sockets from server with PID\n"
            "                 and tell server with PID to exit\n"
            "\n"
            "  The --copy and --replace options may be used any number of times.\n"
            "\n", argv0, argv0);
    return 1;
}

static enum POServiceReturn
Read(struct POService_session *s, void *buffer, size_t bytes, void *data)
{
    char *buf;
    buf = buffer;
    buf[bytes] = '\0';

#ifdef SERVER_SPEW
    static int count = 0; // TODO: count may be garbage in threads.
    printf("%d READ(%zu bytes): %s\n", ++count, bytes, buf);
#endif

    poService_sendF(s, "%s", poRandSequence_string(buf, rbuf, NULL));

    return POSERVICE_CONTINUE;
}

#define MAX_MESSAGE_LENGTH  PO_RANDSEQUENCE_MAX

int main(int argc, char **argv)
{
    if(poTato_doOptions((const char **) argv + 1,
            PO_OPTION_COPY_LISTENERS|PO_OPTION_REPLACE_LISTENERS|PO_OPTION_FORK,
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
                "4444", "0.0.0.0",
                MAX_MESSAGE_LENGTH,
                Read, NULL)) goto fail2;

    if(poService_create(&service[1], &server,
                &tls/* with TLS*/,
                "5555", "0.0.0.0",
                MAX_MESSAGE_LENGTH,
                Read, NULL)) goto fail2;

    if(poService_create(&service[2], &server,
                &tls/* with TLS*/,
                "5556", "0.0.0.0",
                MAX_MESSAGE_LENGTH,
                Read, NULL)) goto fail2;

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

    int retStatus = 0;

    retStatus = poTato_removeChildren();

    ILOG("normal exiting with status %d\n", retStatus);

    return retStatus; // returns number of failed children

fail2:
    poServer_destroy(&server);
fail1:
    poTls_destroy(&tls);

    ILOG("normal exiting with status 1\n");
    return 1; // one bad child
}
