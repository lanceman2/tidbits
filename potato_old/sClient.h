#ifndef __PO_SCLIENT_H__
#define __PO_SCLIENT_H__

// Simple blocking read and write fixed buffer with '\0' terminated
// strings. Does TCP or TLS over TCP.

#include <sys/types.h>
#include <inttypes.h>
#include <stdbool.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>


#include "configure.h"

#ifndef PO_SCLIENT_BUFLEN
#  define PO_SCLIENT_BUFLEN  1024
#endif

struct POTlsC
{
    // TLS stuff shared between client sessions.
    gnutls_certificate_credentials_t xcred;
    uint32_t numClients;
};

struct POSClient
{
    int fd;
    char ibuf[PO_SCLIENT_BUFLEN];
    char obuf[PO_SCLIENT_BUFLEN];
    gnutls_session_t tlsSession;
};

extern
struct POTlsC *poTlsC_create(struct POTlsC *tls,
        const char *certAuthFile);

extern
bool poTlsC_destroy(struct POTlsC *tls);

extern
struct POSClient *poSClient_create(struct POSClient *client,
        const struct sockaddr *addr,
        struct POTlsC *tls);

extern
bool poSClient_destroy(struct POSClient *client);

extern
ssize_t poClient_sendF(struct POSClient *client,
        const char *fmt, ...) __attribute__((format(printf,2,3)));

extern
char *poSClient_recv(struct POSClient *client, size_t len);

#endif //#ifndef __PO_SCLIENT_H__
