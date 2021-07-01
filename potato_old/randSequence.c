#include "randSequence.h"

// Some error checks
#if PO_RANDSEQUENCE_MIN < 8
#  error PO_RANDSEQUENCE_MIN < 8
#endif

#if PO_RANDSEQUENCE_RANGE < 1
#  error PO_RANDSEQUENCE_RANGE < 1
#endif

static inline
char int2char(int i)
{
    if(i < 10)
        return (char) (i + 48);
    return (char) (i + 55);
}

// rlen = length of string returned without '/0' terminator.
// TODO: This is inefficient.  There must be a way to do this with less
// variables and less computation.
//
// This returns a string that is just a function of the what the input buf
// string is.
char *poRandSequence_string(const char *ibuf, char *buf, size_t *rlen)
{
    uint32_t seed[2];
    unsigned short int xsubi[3];

    seed[0] = poMurmurHash(ibuf, 4, 0x324A5234);
    seed[1] = poMurmurHash(&ibuf[4], 4, 0xF3245234);
    memcpy(xsubi, seed, sizeof(xsubi));

    int len;
    char *ret;
    ret = buf;
    len = PO_RANDSEQUENCE_MIN + nrand48(xsubi) % PO_RANDSEQUENCE_RANGE;
    if(rlen)
        *rlen = len;
    while(len)
    {
        int j = 0;
        uint64_t val;
        val = nrand48(xsubi);
        //val = val << 16;
        unsigned char *ptr;
        ptr = (unsigned char *) &val;
        while(len && j < 6)
        {
            *buf = int2char((ptr[j/2] >> (4 * (j%2))) & 0x0F);
            ++buf;
            --len;
            ++j;
        }
    }
    *buf = '\0';
    return ret;
}
