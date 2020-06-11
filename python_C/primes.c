#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

unsigned int *generate(unsigned int num) {

    // TODO: check for valid num >= 2.

    unsigned int *primes = malloc(sizeof(*primes)*num);
    // TODO: check for error

    primes[0] = 2; // 2 is the only even prime number.
    unsigned int nextPrime = 3;
    unsigned int *current = primes + 1;
    unsigned int *stop = primes + num;
    while(current<stop) {
        // *n is the last prime we have so far
        unsigned int *i = primes;
        for(; i<current; ++i)
            if(nextPrime % (*i) == 0) {
                nextPrime += 2;
                break;
            }
        if(i==current)
            *(current++) = nextPrime;
    }
    return primes;
}

// I gave up on making this generate() function into part of a python
// module.  Passing an array to python is a pain.


int main(void) {

#define TEST
#ifdef TEST
    const unsigned int how_many = 30;
    unsigned int *primes = generate(how_many);
    for(unsigned int i=0;i<how_many;++i)
        printf("%u\n", primes[i]);
#else
     unsigned int *primes = generate(50000);
#endif
    free(primes);
    return 0;
}
