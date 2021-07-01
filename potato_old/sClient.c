#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // IPPROTO_TCP
#include <arpa/inet.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sClient.h"
#include "aSsert.h"
#include "gnutlsAssert.h"
#include "elog.h"
#include "util.h"

#include "pthreadWrap.h" // mutexInit() mutexLock() etc...


static int verify_certificate_callback(gnutls_session_t session)
{
    unsigned int status;
    int ret, type;
    gnutls_datum_t out;

    /* This verification function uses the trusted CAs in the credentials
     * structure. So you must have installed one or more CA certificates.
     */

     /* The following demonstrate two different verification functions,
      * the more flexible gnutls_certificate_verify_peers(), as well
      * as the old gnutls_certificate_verify_peers3(). */
    {
        gnutls_typed_vdata_st data[2];

        memset(data, 0, sizeof(data));

        data[0].type = GNUTLS_DT_DNS_HOSTNAME;
        data[0].data = (void*) "localhost";

        data[1].type = GNUTLS_DT_KEY_PURPOSE_OID;
        data[1].data = (void*)GNUTLS_KP_TLS_WWW_SERVER;

        ret = gnutls_certificate_verify_peers(session, data, 2,
					      &status);
    }
    if(ret < 0) {
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    type = gnutls_certificate_type_get(session);

    ret =
        gnutls_certificate_verification_status_print(status, type,
                                             &out, 0);
    if (ret < 0) {
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    //printf("%s", out.data);

    gnutls_free(out.data);

    if(status != 0)        /* Certificate is not trusted */
            return GNUTLS_E_CERTIFICATE_ERROR;

    /* notify gnutls to continue handshake normally */
    return 0; // success.
}

struct POTlsC *poTlsC_create(struct POTlsC *tls,
        const char *certAuthFile)
{
    DASSERT(tls);
    memset(tls, 0, sizeof(*tls));
    if(GNUTLS_ASSERT(gnutls_global_init()))
        return NULL;
    if(GNUTLS_ASSERT(
        gnutls_certificate_allocate_credentials(&tls->xcred)))
        return NULL;
    if(ASSERT(gnutls_certificate_set_x509_trust_file(tls->xcred,
                     certAuthFile, GNUTLS_X509_FMT_PEM) > 0))
         goto fail;

    gnutls_certificate_set_verify_function(tls->xcred,
                        verify_certificate_callback);

    return tls;

fail:

    WLOG("poTlsC_create() failed\n");

    gnutls_certificate_free_credentials(tls->xcred);
    return NULL;
}

bool poTlsC_destroy(struct POTlsC *tls)
{
    DASSERT(tls);
    DASSERT(tls->xcred);
    gnutls_certificate_free_credentials(tls->xcred);
    return false; // success
}

struct POSClient *poSClient_create(struct POSClient *c,
    const struct sockaddr *addr, struct POTlsC *tls)
{
    DASSERT(c);
    memset(c, 0, sizeof(*c));

    if(ASSERT((c->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) >= 0))
        return NULL;

    int val = 1;
#if 1
    if(ASSERT(setsockopt(c->fd, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &val, sizeof(int)) == 0))
        goto fail;
    val = 1;
#endif
#if 0
    if(ASSERT(setsockopt(c->fd, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &val, sizeof(int)) == 0))
        goto fail;
#endif

    // Overcoming port socket TIME_WAIT state:
    // see: `netstat' after running ./client
    // and connect() errno=99:Cannot assign requested address
    //
    // We use SO_REUSEADDR and an explicit bind, which will allow Linux to
    // reuse socket ports, but it's slow to run after there are a lot of
    // ports used, so WTF is bind() doing.  In any case this seemingly
    // pointless shit lets the program make and close an unlimited number
    // of connections.  Remember this is just test client code.
    // From:
    // http://stackoverflow.com/questions/3886506/why-would-connect-give-eaddrnotavail

    struct sockaddr bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));

    switch(addr->sa_family)
    {
        case AF_INET:
        {
            struct sockaddr_in *a;
            a = (struct sockaddr_in *) &bindAddr;
            a->sin_family = AF_INET;
            a->sin_port = 0;
            a->sin_addr.s_addr = INADDR_ANY;
        }
        break;
        case AF_INET6:
        {
            struct sockaddr_in6 *a;
            a = (struct sockaddr_in6 *) &bindAddr;
            a->sin6_family = AF_INET6;
            a->sin6_port = 0;
            a->sin6_addr = in6addr_any;
        }
        break;
        default:
            ASSERT(0);
    }

#if PO_THREADPOOL_MAX_NUM_THREADS > 0
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    mutexLock(&mutex); // make bind() and connect() atomic
#endif

    if(ASSERT(0 == bind(c->fd, &bindAddr, sizeof(bindAddr))))
        goto fail;

     if(ASSERT(0 == connect(c->fd, addr, sizeof(*addr))))
        goto fail;

#if PO_THREADPOOL_MAX_NUM_THREADS > 0
    mutexUnlock(&mutex);
#endif

    if(tls)
    {
        int ret;

        if(GNUTLS_ASSERT(
            gnutls_init(&c->tlsSession, GNUTLS_CLIENT)))
            goto fail;

        if(GNUTLS_ASSERT(
                gnutls_credentials_set(c->tlsSession,
                    GNUTLS_CRD_CERTIFICATE, tls->xcred)))
            goto fail;

        gnutls_transport_set_int(c->tlsSession, c->fd);
        gnutls_handshake_set_timeout(c->tlsSession,
                GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

        gnutls_priority_t priorities_cache;

        if(GNUTLS_ASSERT(gnutls_priority_init(&priorities_cache, 
                "NORMAL", NULL)))
            goto fail;
        /* Use default priorities */
        if(GNUTLS_ASSERT(gnutls_priority_set(c->tlsSession, priorities_cache)))
            goto fail;

        /* Perform the TLS handshake */
        do {
                ret = gnutls_handshake(c->tlsSession);
        }
        while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

        if(ret < 0)
        {
            WLOG("TLS handshake failed- %s\n",
                    gnutls_strerror(ret));
            goto fail;
        }
#ifdef PO_ELOG_NOTICE
        else
        {
            char *desc;
            desc = gnutls_session_get_desc(c->tlsSession);
            DLOG("TLS Session info- %s\n", desc);
            gnutls_free(desc);
        }
#endif
    }

    return c;

fail:

    if(c->tlsSession)
        gnutls_deinit(c->tlsSession);

    if(c->fd >= 0)
        close(c->fd);

    return NULL;
}

bool poSClient_destroy(struct POSClient *c)
{
    DASSERT(c);
    DASSERT(c->fd >= 0);
    if(c->tlsSession)
    {
        gnutls_bye(c->tlsSession, GNUTLS_SHUT_RDWR);
        gnutls_deinit(c->tlsSession);
    }

    bool ret = false;

    shutdown(c->fd, SHUT_RDWR); // we tried...

#if 1
    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;
    if(ASSERT(0 == setsockopt(c->fd, SOL_SOCKET, SO_LINGER,
           &linger, sizeof(linger))))
        ret = true;
#endif

    if(ASSERT(close(c->fd) == 0))
        ret = true;
#ifdef PO_DEBUG
    memset(c, 0, sizeof(*c));
#endif
    return ret;
}

ssize_t poClient_sendF(struct POSClient *c, const char *fmt, ...)
{
    DASSERT(c);
    DASSERT(c->fd >= 0);

    va_list ap;
    int rt;
    ssize_t ret;
    va_start(ap, fmt);
    rt = vsnprintf(c->obuf, sizeof(c->obuf), fmt, ap);
    va_end(ap);
    if(VASSERT(rt < sizeof(c->obuf),
                "requested size %d > buffer size %zu\n",
                rt, sizeof(c->obuf)))
        return true; // error

    if(!c->tlsSession)
    {
        if(writen(c->fd, c->obuf, rt))
            return -1; // error
        return rt; // success
    }

    // This is a TLS session.
    size_t n;
    n = ret = rt;
    char *buf;
    buf = c->obuf;

    while(n)
    {
        ssize_t r;
        r = gnutls_record_send(c->tlsSession, buf, n);

        if(r > 0)
        {
            n -= r;
            buf += r;
        }
        else if(r < 0)
        {
            DLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                    r,  gnutls_strerror(r));

            switch(r)
            {
                case GNUTLS_E_INTERRUPTED: //interrupted by a signal
                    // Try again.
                    continue;
                default:
                    WLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                            r,  gnutls_strerror(r));
                    // TODO: Unexpected error, write more code here.
                    return -1; // error
            }
        }
        else // r == 0
        {
            DLOG("gnutls_record_send(,,) sent 0\n");
            return 0; // closing time.
        }
    }

    return ret; // success
}

char *poSClient_recv(struct POSClient *c, size_t len)
{
    DASSERT(c);
    DASSERT(c->fd >= 0);
    DASSERT(len < sizeof(c->ibuf));

    if(!c->tlsSession)
    {
        if(readn(c->fd, c->ibuf, len))
            return NULL; // error
        return c->ibuf;
    }

    // This is a TLS session.
    char *buf;
    buf = c->ibuf;

    while(len)
    {
        ssize_t rt;
        rt = gnutls_record_recv(c->tlsSession, buf, len);

        if(rt > 0)
        {
            buf += rt;
            len -= rt;
        }
        else if(rt < 0)
        {
             DLOG("gnutls_record_recv(,,) failed err=%zd \"%s\"\n",
                    rt, gnutls_strerror(rt));
            switch(rt)
            {
                case GNUTLS_E_INTERRUPTED: //interrupted by a signal
                    continue; // try again
                default:
                    WLOG("gnutls_record_recv(,,) failed "
                        "%sfatal error=%zd \"%s\"\n",
                        gnutls_error_is_fatal(rt)?"":"non-", 
                        rt, gnutls_strerror(rt));
                    return NULL; // error close
            }
        }
        else // rt == 0
        {
            DLOG("gnutls_record_recv(,,)=0\n");
            return NULL;
        }
    }

    *buf = '\0'; 

    return c->ibuf;
}
