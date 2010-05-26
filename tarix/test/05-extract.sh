#!/usr/bin/env bash

set -xe

[ -f bin/test/data.tgz ]
[ -f bin/test/data.tarix ]

bin/tarix -zxf bin/test/data.tarix -t bin/test/data.tgz data.d/data1 >bin/test/data.x.tar
#hexdump -C bin/test/data.x.tar
tar -tvf bin/test/data.x.tar

# bsd tar doesn't support incremental or volume label
if tar --version | grep GNU.tar ; then
  [ -f bin/test/data.incr.tgz ]
  [ -f bin/test/data.incr.tarix ]
  bin/tarix -zxf bin/test/data.incr.tarix -t bin/test/data.incr.tgz data.d/data1 >bin/test/data.incr.x.tar
  #hexdump -C bin/test/data.incr.x.tar
  tar -tvf bin/test/data.incr.x.tar
fi
