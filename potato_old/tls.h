#ifndef __PO_TLS_H__
#define __PO_TLS_H__

#include <inttypes.h>
#include <stdbool.h>
#include <gnutls/gnutls.h>

#include "configure.h"
#include "aSsert.h"



struct POTls
{
    // Stuff that is shared between TLS sessions.

    // These seem to be a pointers to something that gnutls allocates.
    gnutls_certificate_credentials_t x509_cred;
    gnutls_priority_t priority_cache;
    gnutls_dh_params_t dh_params;

    uint32_t sessionCount;
};

// Pass pointers to the memory into these constructors.
extern
struct POTls *poTls_create(struct POTls *tls, const char *key,
        const char *cert);
extern
bool poTls_destroy(struct POTls *tls);
extern
gnutls_session_t poTlsSession_create(struct POTls *tls, int fd);
// The tls objects do not destroy sessions, the user must
// call this for each session created.  This is not thread safe,
// use with care.
extern
bool poTlsSession_destroy(gnutls_session_t s);

#endif // #ifndef __PO_TLS_H__
