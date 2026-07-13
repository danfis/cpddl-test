#!/bin/bash

# List (task, test) pairs that failed in the last run.
# Scans reg/ for *.fail.tmp files written by the test runner.
# Exit code: 0 if no failures, 1 if at least one failure found.
#
# Usage: list-failures.sh [-c|-m|-x]
#   -c   Print ./test command to re-run each failed pair
#   -m   Print make check command to re-run each failed pair
#   -t   Print only those where the .fail.tmp file contains "Alarm clock" (i.e.,
#        those that timed-out)

mode=plain
regex=
while getopts "cmt" opt; do
    case $opt in
        c) mode=cmd ;;
        m) mode=make ;;
        t) regex="Alarm clock" ;;
        *) echo "Usage: $0 [-c|-m|-t]" >&2; exit 2 ;;
    esac
done

found=0
while IFS= read -r f; do
    if [ "$regex" != "" ]; then
        if ! grep -q "$regex" "${f}"; then
            continue
        fi
    fi
    rel="${f#reg/}"
    tname="${rel##*/}"
    tname="${tname%.fail.tmp}"
    task="${rel%/*}"
    case $mode in
        cmd)   echo "./test -T $task -t $tname" ;;
        make)  echo "make check T='-T $task -t $tname'" ;;
        *)     echo "$task $tname" ;;
    esac
    found=1
done < <(find reg/ -name '*.fail.tmp' | sort)

exit $found
