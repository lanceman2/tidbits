/* run:
 
   make && ./time_test | quickplot -P --silent

*/

#include <stdio.h>
#include <unistd.h>

#include "tIme.h"

int main(void)
{
    double offset;
    offset = poTime_getDouble();
    int i;
    for(i=0;i<160;++i)
    {
        printf("%d %.20lg\n", i, poTime_getDouble() - offset);
        usleep(100); // micro seconds
        // 1,000,000 micro second = 1 second
    }

    fprintf(stderr, "using CLOCK_REALTIME_COARSE\n"
            "notice how time is measured in "
            "0.004 second increments\n");

    return 0;
}
