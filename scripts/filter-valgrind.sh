#!/bin/bash
# AWK filter that extracts Valgrind memory-leak summary lines from mixed output,
# retaining only lines that report a positive number of lost bytes.

awk '/^==/{}/^==.*(definitely|possibly|indirectly) lost:/{if ($4 > 0) print}/^$/{print}/^[^=]/{print}'
