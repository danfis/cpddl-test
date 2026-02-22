#!/bin/bash

# List (task, test) pairs that failed in the last run.
# Scans reg/ for *.fail.tmp files written by the test runner.
# Exit code: 0 if no failures, 1 if at least one failure found.
#
# Usage: list-failures.sh [-c|-m]
#   -c   Print ./test command to re-run each failed pair
#   -m   Print make check command to re-run each failed pair

mode=plain
while getopts "cm" opt; do
    case $opt in
        c) mode=cmd ;;
        m) mode=make ;;
        *) echo "Usage: $0 [-c|-m]" >&2; exit 2 ;;
    esac
done

found=0
while IFS= read -r f; do
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
