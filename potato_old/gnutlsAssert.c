#include <gnutls/gnutls.h>

#include "aSsert.h"
#include "elog.h"


bool _poGNUTLSAssert(const char *file, int line,
    const char *func, int val, const char *arg)
{
    if(val == GNUTLS_E_SUCCESS) return false;

    return _poAssert(file, line, func, false,
        arg, "gerr=%d %s\n",
        val, gnutls_strerror(val));
}
