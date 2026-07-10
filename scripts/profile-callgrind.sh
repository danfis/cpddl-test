#!/bin/bash

OUTPUT=profile-base
if [ -f ${OUTPUT}.out ]; then
    OUTPUT=profile
fi

valgrind \
    --tool=callgrind \
    --callgrind-out-file=${OUTPUT}.cg \
    $@

callgrind_annotate --threshold=100 --inclusive=yes ${OUTPUT}.cg >${OUTPUT}.out
