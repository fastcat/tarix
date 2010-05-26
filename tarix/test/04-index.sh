#!/usr/bin/env bash

set -xe

rm -f bin/test/data.{tarix,tgz,list,incr.tarix,incr.tgz}

mkdir -p bin/test/data.d
cp bin/test/data bin/test/data.d/data1
cp bin/test/data bin/test/data.d/data2

sources="data data.d"

tar -c -f - -C bin/test/ $sources | bin/tarix -zf bin/test/data.tarix >bin/test/data.tgz
[ -f bin/test/data.tarix ]
[ -f bin/test/data.tgz ]
cat bin/test/data.tarix
#zcat bin/test/data.tgz | hexdump -C
tar -tzv -f bin/test/data.tgz

# bsd tar doesn't support incremental or volume label
if tar --version | grep GNU.tar ; then
  # old versions (1.22.x at least) of tar have a bug handling -C properly
  # so instead we do the cd ourselves to avoid trying to detect that
  ( cd bin/test && tar -c -f - -V 'test backup of data, data.d/data*' \
    --listed-incremental=data.list $sources ) \
    | bin/tarix -zf bin/test/data.incr.tarix >bin/test/data.incr.tgz
  [ -f bin/test/data.list ]
  [ -f bin/test/data.incr.tarix ]
  [ -f bin/test/data.incr.tgz ]
  cat bin/test/data.incr.tarix
  #zcat bin/test/data.incr.tgz | hexdump -C
  tar -tzv -f bin/test/data.incr.tgz
fi
