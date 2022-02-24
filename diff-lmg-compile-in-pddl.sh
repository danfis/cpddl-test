#!/bin/bash

for f in reg/tmp.*.lmg_compile_in.out; do
    if [ "$(wc -l $f | awk '{print $1}')" -le 4 ]; then
        continue
    fi
    g=${f##reg/tmp.}
    g=${g%%.*}
    g=reg/${g}.pddl.out
    echo "diff $f $g"
    continue
done
