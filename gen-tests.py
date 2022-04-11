#!/usr/bin/env python3

import sys
import re
from pprint import pprint

Test = {}
GlobalTearDown = False

pat_test = re.compile(r'^.*TEST\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+)\).*$')
pat_test_cond = re.compile(r'^.*TEST_COND\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+), *([a-zA-Z0-9_ ]+)\).*$')
pat_test_tear_down = re.compile(r'^.*TEST_TEAR_DOWN\(([a-zA-Z0-9_]+) *\).*$')
pat_test_explicit = re.compile(r'^.*TEST_EXPLICIT\(([a-zA-Z0-9_]+) *\).*$')
pat_global_tear_down = re.compile(r'^.*TEST_GLOBAL_TEAR_DOWN.*$')
pat_define = re.compile(r'^ *# *define +PDDL_([A-Z_0-9]+).*$')

CONFIG = []
def parseConfig(fn):
    global CONFIG
    with open(fn, 'r') as fin:
        for line in fin:
            m = pat_define.match(line)
            if m is not None:
                CONFIG += [m.group(1)]
    CONFIG = sorted(CONFIG)

def addTest(name, tags):
    global CONFIG
    for tag in tags:
        if tag not in CONFIG:
            return False

    global Test
    if name not in Test:
        Test[name] = { 'dep' : None,
                       'tear-down' : False,
                       'explicit' : False }
    return True

def parseFile(filename):
    global Test
    with open(filename, 'r') as fin:
        for line in fin:
            match = pat_test.match(line)
            if match is not None:
                name = match.group(1)
                if addTest(name, []):
                    Test[name]['dep'] = match.group(2)

            match = pat_test_cond.match(line)
            if match is not None:
                name = match.group(1)
                tags = [x for x in match.group(3).split() if len(x) > 0]
                if addTest(name, tags):
                    Test[name]['dep'] = match.group(2)

            match = pat_test_tear_down.match(line)
            if match is not None:
                name = match.group(1)
                if name in Test:
                    Test[name]['tear-down'] = True

            match = pat_test_explicit.match(line)
            if match is not None:
                name = match.group(1)
                if addTest(name, []):
                    Test[name]['explicit'] = True

            match = pat_global_tear_down.match(line)
            if match is not None:
                global GlobalTearDown
                GlobalTearDown = True



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

        if Test[key]['explicit']:
            print(', 1', end = '')
        else:
            print(', 0', end = '')
        print('},')
    print('};')
    print('size_t test_set_size = sizeof(test_set) / sizeof(test_def_t);')

    if GlobalTearDown:
        print('#define USE_GLOBAL_TEAR_DOWN')
        print('void __test_global_tear_down(void);')

def main():
    parseConfig('../pddl/config.h')
    for filename in sys.argv[1:]:
        parseFile(filename)
    #pprint(Test)
    genDeclarations()
    genDefs()

if __name__ == '__main__':
    main()
