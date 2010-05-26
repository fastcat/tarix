#!/usr/bin/env bash

set -xe

[ -f bin/test/data.tgz ]
[ -f bin/test/data.tarix ]
[ -f bin/test/data.x.tar ]

cat bin/test/data.tarix | while read line ; do
  echo "$line"
  echo "# comment: $line"
done >bin/test/data.tarix.comment

bin/tarix -zxf bin/test/data.tarix.comment -t bin/test/data.tgz data.d/data1 >bin/test/data.xc.tar
#hexdump -C bin/test/data.x.tar
tar -tvf bin/test/data.xc.tar
cmp bin/test/data.x.tar bin/test/data.xc.tar
