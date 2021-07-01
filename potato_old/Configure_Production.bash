#!/bin/bash

set -e
src_prefix="$(dirname ${BASH_SOURCE[0]})"

set -x
CFLAGS="-O3 -Werror -Wall" quickbuild --src-prefix "$src_prefix" --test false
