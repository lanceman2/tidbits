#ifndef __PO_RANDSEQUENCE_H__
#define __PO_RANDSEQUENCE_H__

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "murmurHash.h"

#ifndef PO_RANDSEQUENCE_MIN
#  define PO_RANDSEQUENCE_MIN  10
#endif
#ifndef PO_RANDSEQUENCE_MAX
#  define PO_RANDSEQUENCE_MAX  300
#endif


#ifndef PO_RANDSEQUENCE_RANGE
#  define PO_RANDSEQUENCE_RANGE (PO_RANDSEQUENCE_MAX - PO_RANDSEQUENCE_MIN)
#endif

// rlen = length of string returned without 
// TODO: This is inefficient.  There must be a way to do this with less
// variables and less computation.
//
// This returns a string that is just a function of the what the input buf
// string is.
extern
char *poRandSequence_string(const char *ibuf, char *buf, size_t *rlen);

#endif // #ifndef __PO_RANDSEQUENCE_H__
