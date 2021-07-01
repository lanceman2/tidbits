#!/bin/bash

set -xeuo pipefail


cd "$(dirname ${BASH_SOURCE[0]})"



exec gdb --args rtl_sdr -f 89100000 -s 2048000 -S -n 10259 xxx

exit

strace -f rtl_sdr -f 89100000 -s 2048000 -S -n 1025999 xxx1 2> yyy

