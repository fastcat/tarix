#!/bin/bash

# test script for t_tws.c

set -xe

rm -f bin/test/data.gz
bin/test/t_tws
[ -f bin/test/data.gz ] || exit 1
hexdump -C bin/test/data.gz
zcat bin/test/data.gz | hexdump -C
