#!/usr/bin/env bash

# test script for 01-tws.c

set -xe

rm -f bin/test/data.gz
bin/test/01-tws
[ -f bin/test/data.gz ] || exit 1
hexdump -C bin/test/data.gz
zcat bin/test/data.gz | hexdump -C
