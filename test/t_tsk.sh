#!/usr/bin/env bash

set -xe

bin/test/t_tsk
[ -f bin/test/data ] || exit 1
hexdump -C bin/test/data
