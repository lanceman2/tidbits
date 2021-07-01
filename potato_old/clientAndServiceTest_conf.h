#ifndef __PO_CLIENTANDSERVICETEST_H__
#define __PO_CLIENTANDSERVICETEST_H__

// This is included in client and server test programs in order to keep
// these parameters the same in both the test client and the test server
// (service).


#define PO_RANDSEQUENCE_MIN  10
#define PO_RANDSEQUENCE_MAX  200
#define PO_SCLIENT_BUFLEN PO_RANDSEQUENCE_MAX

#include "randSequence.h"

#endif //#ifndef __PO_CLIENTANDSERVICETEST_H__
