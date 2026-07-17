#!/bin/bash
# Collects every <test>.*.tmp bundle whose <test>.out.tmp differs from its
# golden baseline <test>.out and streams them as a gzipped tar to stdout.
# All diagnostics go to stderr so stdout carries only the archive.
#
# Run from the tests/ directory (like fix-reg.sh).  Archive paths are stored
# as reg/<task>/<test>.*.tmp so extraction rebuilds the tree in place.
#
# Intended to be piped into `tar xzf -`, typically over ssh -- see
# fetch-reg-diffs.sh.

set -euo pipefail

files=()
n_tests=0

while IFS= read -r tmp; do
    f=${tmp%.tmp}   # <test>.out.tmp -> <test>.out

    # An empty .out.tmp with no baseline means the test simply produced no
    # output and there is nothing to compare against -- not a difference.
    size=$(stat -c%s "$tmp")
    if [ "$size" -eq 0 ] && [ ! -f "$f" ]; then
        continue
    fi

    # Difference: baseline missing (new output), or contents differ.
    if [ ! -f "$f" ] || ! diff -q "$tmp" "$f" >/dev/null 2>&1; then
        n_tests=$((n_tests + 1))
        prefix=${tmp%.out.tmp}
        for t in "$prefix".*.tmp; do
            [ -e "$t" ] && files+=("$t")
        done
    fi
done < <(find reg/ -name '*.out.tmp')

# Include the run log alongside the diffs if it is present.
[ -f check.log ] && files+=(check.log)

if [ ${#files[@]} -eq 0 ]; then
    echo "pack-reg-diffs: no differing outputs found." >&2
    # Emit a valid (empty) archive so the receiving `tar xzf -` does not
    # choke on an unexpected EOF.
    tar czf - -T /dev/null
    exit 0
fi

echo "pack-reg-diffs: packing ${#files[@]} files from ${n_tests} differing tests." >&2
tar czf - "${files[@]}"
