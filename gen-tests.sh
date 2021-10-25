#!/bin/bash

cat $@ \
    | grep 'TEST(' \
    | sed 's/^.*TEST( *//' \
    | sed 's/ *).*$//' \
    | sed 's/ *, */ /g' \
    | sort -k1,1 \
    | awk '{printf("void test_%s(void);\nvoid test_tear_down_%s(void);\n", $1, $1);}'

echo "static test_def_t test_set[] = {"
cat $@ \
    | grep 'TEST(' \
    | sed 's/^.*TEST( *//' \
    | sed 's/ *).*$//' \
    | sed 's/ *, */ /g' \
    | sort -k1,1 \
    | awk '{printf("    {\"%s\", test_%s, test_tear_down_%s, \"%s\"},\n", $1, $1, $1, $2);}'
echo "};"
echo "size_t test_set_size = sizeof(test_set) / sizeof(test_def_t);"
