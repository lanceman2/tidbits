#ifndef __PO_MURMURHASH_H__
#define __PO_MURMURHASH_H__

#include <inttypes.h>

#include "configure.h"

extern // see murmurHash.c for details.
uint32_t poMurmurHash(const void *key, uint32_t len, uint32_t seed);

#endif // #ifndef __PO_MURMURHASH_H__
