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
  tar -c -f - -C bin/test/ -V 'test backup of data, data.d/data*' \
    --listed-incremental=bin/test/data.list $sources \
    | bin/tarix -zf bin/test/data.incr.tarix >bin/test/data.incr.tgz
  [ -f bin/test/data.list ]
  [ -f bin/test/data.incr.tarix ]
  [ -f bin/test/data.incr.tgz ]
  cat bin/test/data.incr.tarix
  #zcat bin/test/data.incr.tgz | hexdump -C
  tar -tzv -f bin/test/data.incr.tgz
fi
