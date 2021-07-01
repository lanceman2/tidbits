/*

   Connect to TCP port with:

nc localhost 4444


   Connect to TLS over TCP port with:

gnutls-cli --insecure localhost -p 5555

   or:

openssl s_client -connect localhost:5555


See service_test.c for more complex bash scripts.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "server.h"
#include "tls.h"
#include "potato.h"


// NOTE: This is not good code for a production server.  It's test code.


// returns -1 on error and close
// returns  1 on no error and close
// returns  0 on no error and keep

#define MAX_BUF    1023

static int readCount = 0;

int echoRequest(struct POServer_connection *conn, gnutls_session_t s)
{
    DASSERT(s);
    DASSERT(conn);
    DLOG("TLS read\n");

    size_t n = 0;
    char buf[MAX_BUF + 1];
    int r, ret = 0;


    while(n < MAX_BUF)
    {
        r = gnutls_record_recv(s, &buf[n], MAX_BUF - n);

        if(r > 0)
        {
            n += r;
        }
        else if(r == GNUTLS_E_AGAIN)
        {
            // this will end this non-blocking read session
            break;
        }
        else if(r == GNUTLS_E_INTERRUPTED)
        {
            DLOG("** TSL warning- %s\n", gnutls_strerror(r));
            continue;
        }
        else if(r == 0)
        {
            ret = 1; // Peer has closed the GnuTLS connection
            break;
        }
        else if(r < 0 &&  gnutls_error_is_fatal(r) == 0)
        {
            WLOG("** TSL warning- %s\n", gnutls_strerror(r));
            // So WTF do we do here?  Keep going.
        }
        else if(r < 0)
        {
            // fatal error
            NLOG("** TSL session corrupted (%d)- %s\n", r, gnutls_strerror(r));
            return -1;                    
        }
    }

    buf[n] = '\0';
    printf("%d READ(fd=%d,%zu): %s\n", readCount++, conn->fd, n, buf);

    size_t i = 0;
    while(i < n)
    {
        r = gnutls_record_send(s, &buf[i], n - i);

        if(r > 0)
        {
            i += r;
        }
        else if(r == GNUTLS_E_AGAIN)
        {
            // write is would block
            usleep(10); // WTF let's wait a little
            // Stupid test program.
            break;
        }
        else if(r == GNUTLS_E_INTERRUPTED)
        {
            DLOG("** TSL warning- %s\n", gnutls_strerror(r));
            continue;
        }
        else if(r == 0)
        {
            ret = 1; // Peer has closed the GnuTLS connection
            break;
        }
        else if(r < 0 &&  gnutls_error_is_fatal(r) == 0)
        {
            WLOG("** TSL warning- %s\n", gnutls_strerror(r));
            // So WTF do we do here?  Keep going.
        }
        else if(r < 0)
        {
            // fatal error
            NLOG("** TSL session corrupted- %s\n", gnutls_strerror(r));
            return -1;                    
        }
    }

    return ret;
}

int destroySession(struct POServer_connection *conn,
        gnutls_session_t session)
{
    DASSERT(session);
    poTlsSession_destroy(session);
    DLOG("TLS cleaned up session\n");
    return 0;
}

int createSession(struct POServer_connection *conn,
        struct POTls *tls)
{
    gnutls_session_t session;
    session = poTlsSession_create(tls, conn->fd);
    if(!session) return -1; // error

    DLOG("TLS session was set up successfully\n");

    poServer_setCleanupCallback(conn,
            (POServer_callback_t) destroySession,
            session);

    DASSERT(conn->readCallback == (POServer_callback_t) createSession);
    
    poServer_setReadCallback(conn,
            (POServer_callback_t) echoRequest,
            session);
    return echoRequest(conn, session);
}


int main(int argc, char **argv)
{    
    if(argc == 2)
        // Get listener sockets from server at pid = argv[1]
        if(poTato_signalHandOff(argv[1], true))
            return 1; // error

    struct POServer s;
    struct POTls tls;
    poTls_create(&tls, "key.pem", "cert.pem");


    if(poServer_create(&s) == NULL) return 1;

    if(poServer_listenerSetup(&s, "4444", "0.0.0.0",
                poServer_echoRequest, NULL))
        return 1;
    if(poServer_listenerSetup(&s, "5555", "0.0.0.0",
                (POServer_callback_t) createSession,
                &tls)) return 1;
    NLOG("port 5555 using TLS\n");

    while(poServer_isRunning(&s))
        poServer_doNextEvents(&s);

    poServer_shutdown(&s);
    // poServer_destroy() will call cleanup callbacks which will
    // cleanup any existing TLS sessions.
    poServer_destroy(&s);
    
    // All TLS sessions should be cleaned up before calling
    // poTls_destroy().
    poTls_destroy(&tls);

    DLOG("%s -Server Exiting Now\n", argv[0]);

    return 0;
}
