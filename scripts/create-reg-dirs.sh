#!/bin/bash

cat | cut -f1 -d' ' | sort | uniq | while read dr; do
    mkdir -p reg/$dr
    touch reg/$dr/.dir
    git add reg/$dr/.dir
done

find reg -maxdepth 1 -type f -name '*.out' | while read fn; do
    task=$(echo "$fn" | cut -f2 -d/)
    find reg/ -type d | while read dr; do
        prefix=$(echo "$dr" | sed 's,^reg/,,' | sed 's,/,-,g')
        if [ "$prefix" = "" ]; then continue; fi
        if echo "$task" | grep -q "^${prefix}\\."; then
            dstfn=${task#${prefix}.}
            echo "reg/${task}" "-->" "${dr}/${dstfn}"
            git mv "reg/${task}" "${dr}/${dstfn}"
            break
        fi
    done
done
