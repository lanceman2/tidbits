#include <stdio.h>

#include "random.h"


#define LEN 10000

int main(void)
{
    uint32_t i;
    struct PORandom r;
    poRandom_init(&r, 1/*seed*/);

#if 0
    uint32_t num[LEN] = { 0 };
    for(i=0; i<6000000; ++i)
        ++num[poRandom_get(&r)%LEN];

    for(i=0; i<LEN; ++i)
        printf("%"PRIu32" %"PRIu32"\n", i, num[i]);
#else

    for(i=0; i<10000; ++i)
        printf("%"PRIu32" %"PRIu32"\n", i, poRandom_get(&r)%5000);

#endif

    return 0;
}
