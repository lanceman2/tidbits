#ifndef __PO_GNUTLS_ASSERT_H__
#define __PO_GNUTLS_ASSERT_H__


extern bool _poGNUTLSAssert(const char *file, int line,
    const char *func, int val, const char *arg);

#define GNUTLS_ASSERT(x)        \
            _poGNUTLSAssert(__FILE__, __LINE__, __func__, (x), #x)


#endif // #ifndef __PO_GNUTLS_ASSERT_H__

