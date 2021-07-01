#include <errno.h>
#include "elog.h"


int main(int argc, char **argv)
{
    errno = 0;
    DLOG("%s - debug message\n", argv[0]);
    NLOG("%s - notice message\n", argv[0]);
    WLOG("%s - warning message\n", argv[0]);
    errno = 44;   
    ELOG("%s - error message\n", argv[0]);

    return 0;
}
