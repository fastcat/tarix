#!/usr/bin/env bash

set -xe

bin/test/03-tsk
[ -f bin/test/data ] || exit 1
hexdump -C bin/test/data
