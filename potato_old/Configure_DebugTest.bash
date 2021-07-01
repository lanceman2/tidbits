#!/bin/bash

set -e
src_prefix="$(dirname ${BASH_SOURCE[0]})"

set -x
quickbuild --src-prefix "$src_prefix" --test true
