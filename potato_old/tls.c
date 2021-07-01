#include <string.h>
#include <stdlib.h>

#include "tls.h"

// Not thread safe, handle with care.
static uint32_t tlsObjCount = 0;


static inline
bool generate_dh_params(gnutls_dh_params_t *dh_params)
{
    unsigned int bits;
        
    bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH,
            GNUTLS_SEC_PARAM_LEGACY);

    /* Generate Diffie-Hellman parameters - for use with DHE
     * kx algorithms. When short bit length is used, it might
     * be wise to regenerate parameters often.
     */
    if(ASSERT(gnutls_dh_params_init(dh_params) == GNUTLS_E_SUCCESS) ||
        ASSERT(gnutls_dh_params_generate2(*dh_params, bits) ==
            GNUTLS_E_SUCCESS))
        return true;

    return false;
}


struct POTls *poTls_create(struct POTls *tls, const char *key,
        const char *cert)
{
    DASSERT(tls);

    if(tlsObjCount == 0)
        if(ASSERT(gnutls_global_init() == GNUTLS_E_SUCCESS))
            goto fail;
    ++tlsObjCount;


    memset(tls, 0, sizeof(*tls));

    if(ASSERT(gnutls_certificate_allocate_credentials(&tls->x509_cred) ==
                GNUTLS_E_SUCCESS))
        goto fail;

#if 0
    if(ASSERT(gnutls_certificate_set_x509_trust_file(tls->x509_cred,
                    PO_TLS_CA_FILE,
                    GNUTLS_X509_FMT_PEM) > 0
                    /*returns num of certificates processed*/))
        goto fail;

    gnutls_certificate_set_x509_crl_file(tls->x509_cred, "crl.pem",
                                             GNUTLS_X509_FMT_PEM);
#endif
    
    if(ASSERT(gnutls_certificate_set_x509_key_file(tls->x509_cred,
                    cert, key,
                    GNUTLS_X509_FMT_PEM) >= 0))
        goto fail;

    if(ASSERT(gnutls_priority_init(&tls->priority_cache,
                "PERFORMANCE:%SERVER_PRECEDENCE", NULL) ==
                GNUTLS_E_SUCCESS))
        goto fail;


    ///////////////////////////////////////////////////////////////////
    // TODO: Should this be done for each session??
    if(ASSERT(generate_dh_params(&tls->dh_params) == false))
        goto fail;

    gnutls_certificate_set_dh_params(tls->x509_cred, tls->dh_params);
    ///////////////////////////////////////////////////////////////////


    return tls;

fail:

    if(tls->x509_cred)
        gnutls_certificate_free_credentials(tls->x509_cred);

    if(tls->priority_cache)
        gnutls_priority_deinit(tls->priority_cache);


    --tlsObjCount;
    if(tlsObjCount == 0)
        gnutls_global_deinit();

    return NULL;
}

bool poTls_destroy(struct POTls *tls)
{
    DASSERT(tls);

 
    DASSERT(tls->sessionCount == 0);

    if(VASSERT(tls->sessionCount == 0,
                "there are still %"PRIu32" sessions\n",
                tls->sessionCount))
        return true; // you can try again later. ???


    DASSERT(tls->x509_cred);
    DASSERT(tls->priority_cache);

    if(tls->x509_cred)
        gnutls_certificate_free_credentials(tls->x509_cred);

    if(tls->priority_cache)
        gnutls_priority_deinit(tls->priority_cache);

#ifdef PO_DEBUG
    memset(tls, 0, sizeof(*tls));
#endif
 

    --tlsObjCount;
    if(tlsObjCount == 0)
        gnutls_global_deinit();

    return false; // success
}

gnutls_session_t poTlsSession_create(struct POTls *tls, int fd)
{
    DASSERT(tls);
    DASSERT(fd >= 0);
    
    DASSERT(tls->x509_cred);
    DASSERT(tls->priority_cache);

    gnutls_session_t s;

    if(ASSERT(gnutls_init(&s, GNUTLS_SERVER) == GNUTLS_E_SUCCESS))
        goto fail;

    if(ASSERT(gnutls_priority_set(s, tls->priority_cache) ==
                GNUTLS_E_SUCCESS))
        goto fail;

    
    if(ASSERT(gnutls_credentials_set(s, GNUTLS_CRD_CERTIFICATE,
                tls->x509_cred) == GNUTLS_E_SUCCESS))
        goto fail;

#if 1
    // We send a "do you support http2" using ALPN (application-layer
    // protocol negotiation).  The "client connection preface"
    // (https://tools.ietf.org/html/rfc7540) will let us know if the
    // client wants to use http2.
    //
    // Once TLS negotiation is complete, both the client and the server
    // MUST send a connection preface which confirms the use of http2.

    gnutls_datum_t protocols[1];
    protocols[0].data = (unsigned char *) "h2"; // for http/2
    protocols[0].size = 2;

    if(ASSERT(gnutls_alpn_set_protocols(s, protocols,
            1/*num procols*/, 0/*flags*/) == GNUTLS_E_SUCCESS))
        goto fail;
#endif

    /* We don't request any certificate from the client.  If we did we
     * would need to verify it. One way of doing that is shown in the
     * "Verifying a certificate" example. */
    gnutls_certificate_server_set_request(s, GNUTLS_CERT_IGNORE);

    gnutls_transport_set_int(s, fd);


    int ret;
    do
        ret = gnutls_handshake(s);
    while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

    if(ret < 0)
    {
        WLOG("TLS Handshake has failed (%s)\n",
                gnutls_strerror(ret));
        goto fail;
    }

    gnutls_session_set_ptr(s, tls);

    ++tls->sessionCount;
    return s;

fail:

    
    if(s)
        gnutls_deinit(s);

    return NULL;

}

bool poTlsSession_destroy(gnutls_session_t s)
{
    DASSERT(s);
    int ret;
    struct POTls *tls;
    tls = gnutls_session_get_ptr(s);
    DASSERT(tls);

    while(((ret = gnutls_bye(s, GNUTLS_SHUT_WR))
            != GNUTLS_E_SUCCESS))
    {
        if(ret != GNUTLS_E_INTERRUPTED ||
                ret != GNUTLS_E_AGAIN)
        {
            DLOG("gnutls_bye()=%d\n", ret);
            break;
        }
    }

    gnutls_deinit(s);

    --tls->sessionCount;

    return false;
}
