#include <stdio.h>

#include "aSsert.h"

int main(void)
{
    DASSERT(fopen("/ debug assert ", "r"));
    ASSERT(fopen("/ not debug assert ", "r"));


    return 0;
}
