#!/usr/bin/env bash

set -xe

bin/test/02-trs
[ -f bin/test/data ] || exit 1
hexdump -C bin/test/data
