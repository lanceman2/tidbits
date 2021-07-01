#include "elog.h"


int main(int argc, char **argv)
{
    WLOG("%s starting\n", argv[0]);
    DLOG("%s running\n", argv[0]);
    WLOG("%s exiting\n", argv[0]);

    return 0;
}
