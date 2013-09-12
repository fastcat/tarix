#!/usr/bin/env bash

set -xe

[ -f bin/test/data.tgz ]
[ -f bin/test/data.tarix ]
[ -f bin/test/data.x.tar ]

bin/tarix -zxf bin/test/data.tarix -t bin/test/data.tgz -g 'data.?/data[1]' >bin/test/data.glob.tar
#hexdump -C bin/test/data.glob.tar
tar -tvf bin/test/data.glob.tar
cmp bin/test/data.x.tar bin/test/data.glob.tar

bin/tarix -zxf bin/test/data.tarix -t bin/test/data.tgz data.d >bin/test/data.x2.tar
bin/tarix -zxf bin/test/data.tarix -t bin/test/data.tgz -G 'data.?' >bin/test/data.glob2.tar
#hexdump -C bin/test/data.glob2.tar
tar -tvf bin/test/data.glob2.tar
cmp bin/test/data.x2.tar bin/test/data.glob2.tar
