#!/bin/bash

for tmp in $(find reg/ -name *.out.tmp); do
    f=${tmp%%.tmp}
    if [ "$(stat -c%s $tmp)" = "0" ] && [ ! -f $f ]; then
        continue
    fi

    if [ ! -f $f ]; then
        echo 'Missing' $f
        echo "TODO"
        exit -1

    elif ! diff -q $tmp $f >/dev/null 2>&1; then
        echo 'Diff' $f $tmp
        while true; do
            echo -n "(S)hort diff, (l)ess diff, (v)imdiff, (f)ix: "
            read answ
            if [ "$answ" = "S" ]; then
                diff -y -W150 $f $tmp | head -30
            elif [ "$answ" = "l" ]; then
                diff -y -W150 $f $tmp | less
            elif [ "$answ" = "v" ]; then
                vimdiff $f $tmp
            elif [ "$answ" = "f" ]; then
                cp $tmp $f
                if [ "$(wc -l $f | cut -f1 -d' ')" = "0" ]; then
                    rm $f
                fi
                break
            else
                break
            fi
        done
    fi
done
