#!/usr/bin/env python3

import sys
import re
from pprint import pprint

Test = {}

pat_test = re.compile(r'^.*TEST\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+)\).*$')
pat_test_tear_down = re.compile(r'^.*TEST_TEAR_DOWN\(([a-zA-Z0-9_]+) *\).*$')
pat_test_simple = re.compile(r'^.*TEST_SIMPLE\(([a-zA-Z0-9_]+) *\).*$')
def parseFile(filename):
    global Test
    with open(filename, 'r') as fin:
        for line in fin:
            match = pat_test.match(line)
            if match is not None:
                name = match.group(1)
                if name not in Test:
                    Test[name] = { 'dep' : None,
                                   'tear-down' : False,
                                   'simple' : False }
                Test[name]['dep'] = match.group(2)

            match = pat_test_tear_down.match(line)
            if match is not None:
                name = match.group(1)
                if name not in Test:
                    Test[name] = { 'dep' : None,
                                   'tear-down' : False,
                                   'simple' : False }
                Test[name]['tear-down'] = True

            match = pat_test_simple.match(line)
            if match is not None:
                name = match.group(1)
                if name not in Test:
                    Test[name] = { 'dep' : None,
                                   'tear-down' : False,
                                   'simple' : False }
                Test[name]['simple'] = True



def genDeclarations():
    keys = sorted(Test.keys())
    for key in keys:
        print('void test_{0}(void);'.format(key))
        if Test[key]['tear-down']:
            print('void test_tear_down_{0}(void);'.format(key))

def genDefs():
    keys = sorted(Test.keys())
    print('static test_def_t test_set[] = {')
    for key in keys:
        print('    {{"{0}", test_{0}'.format(key), end = '')

        if Test[key]['tear-down']:
            print(', test_tear_down_{0}'.format(key), end = '')
        else:
            print(', NULL', end = '')

        if Test[key]['dep'] is not None:
            print(', "{0}"'.format(Test[key]['dep']), end = '')
        else:
            print(', "_"', end = '')

        if Test[key]['simple']:
            print(', 1', end = '')
        else:
            print(', 0', end = '')
        print('},')
    print('};')
    print('size_t test_set_size = sizeof(test_set) / sizeof(test_def_t);')

def main():
    for filename in sys.argv[1:]:
        parseFile(filename)
    #pprint(Test)
    genDeclarations()
    genDefs()

if __name__ == '__main__':
    main()
