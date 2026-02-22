#!/bin/bash
# Non-interactively updates golden regression baselines by copying each
# non-empty .out.tmp to its corresponding .out file.  Files larger than
# MAX_SIZE_BYTES (default 5 MB) are skipped and reported.

# Configurable size limit (5MB by default, can be overridden with MAX_SIZE_BYTES env var)
MAX_SIZE_BYTES=${MAX_SIZE_BYTES:-$((5 * 1024 * 1024))}

# Array to track skipped files
skipped_files=()

for tmp in $(find reg/ -name '*.out.tmp' -size +0); do
    echo $tmp
    f=${tmp%%.tmp}
    
    # Get file size in bytes (works on both Linux and macOS)
    tmp_size=$(stat -f%z "$tmp" 2>/dev/null || stat -c%s "$tmp" 2>/dev/null)
    
    # Create new baseline if doesn't exist
    if [ ! -f $f ]; then
        # Skip files that exceed size limit
        if [ "$tmp_size" -gt "$MAX_SIZE_BYTES" ]; then
            skipped_files+=("$tmp")
            continue
        fi

        echo Copy: $tmp $f
        cp $tmp $f
    fi
    
    # Update baseline if contents differ
    if ! diff -q $tmp $f >/dev/null 2>&1; then
        # Skip files that exceed size limit
        if [ "$tmp_size" -gt "$MAX_SIZE_BYTES" ]; then
            skipped_files+=("$tmp")
            continue
        fi

        echo Fix: $tmp $f
        cp $tmp $f
    fi
done

# Print summary of skipped files
if [ ${#skipped_files[@]} -gt 0 ]; then
    echo ""
    echo "Skipped files (size > $((MAX_SIZE_BYTES / 1024 / 1024))MB):"
    for f in "${skipped_files[@]}"; do
        echo "  $f"
    done
    exit 1
fi

exit 0
