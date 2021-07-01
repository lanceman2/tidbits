#ifndef __PO_RANDOM_H__
#define __PO_RANDOM_H__
 
// We love the murmur hash so much that we use it to generate random
// numbers.  This is not for cryptography.  murmur hash is not for
// cryptography.

#include <string.h>

#include "configure.h"

#include "murmurHash.h"



struct PORandom
{
    uint32_t seed;
    uint32_t counter;
};

static inline
struct PORandom *poRandom_init(struct PORandom *r, uint32_t seed)
{
    memset(r, 0, sizeof(*r));
    r->seed = seed;
    return r;
}

static inline
uint32_t poRandom_get(struct PORandom *r)
{
    ++r->counter;
    return poMurmurHash(&r->counter, sizeof(uint32_t), r->seed);
}

#endif // #ifndef __PO_RANDOM_H__
