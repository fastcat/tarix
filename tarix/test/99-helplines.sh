#!/usr/bin/env bash

set -xe

# simple help output should fit on an 80x25 terminal

help_lines=`bin/tarix -h | wc -l`
[ $help_lines -le 25 ]

bin/tarix -h | while read line ; do
  line_chars=`echo "$line" | wc -c`
  [ $line_chars -lt 80 ]
done
