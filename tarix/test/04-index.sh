#!/usr/bin/env bash

set -xe

rm -f bin/test/data.{tarix,tgz,list,incr.tarix,incr.tgz}

mkdir -p bin/test/data.d
cp bin/test/data bin/test/data.d/data1
cp bin/test/data bin/test/data.d/data2

sources="data data.d"

tar -C bin/test/ -c $sources | bin/tarix -zf bin/test/data.tarix >bin/test/data.tgz
[ -f bin/test/data.tarix ] || exit 1
[ -f bin/test/data.tgz ] || exit 1
cat bin/test/data.tarix
#zcat bin/test/data.tgz | hexdump -C
tar -tzvf bin/test/data.tgz

tar -C bin/test/ -c -V 'test backup of data, data.d/data*' --listed-incremental=bin/test/data.list $sources | bin/tarix -zf bin/test/data.incr.tarix >bin/test/data.incr.tgz
[ -f bin/test/data.list ] || exit 1
[ -f bin/test/data.incr.tarix ] || exit 1
[ -f bin/test/data.incr.tgz ] || exit 1
cat bin/test/data.incr.tarix
#zcat bin/test/data.incr.tgz | hexdump -C
tar -tzvf bin/test/data.incr.tgz
