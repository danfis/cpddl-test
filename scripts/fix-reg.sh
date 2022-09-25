#!/bin/bash

for tmp in $(find reg/ -name '*.out.tmp' -size +0); do
    f=${tmp%%.tmp}
    if [ ! -f $f ]; then
        echo Copy: $tmp $f
        cp $tmp $f;
    fi
    if ! diff -q $tmp $f >/dev/null 2>&1; then
        echo Fix: $tmp $f
        cp $tmp $f;
    fi
done
