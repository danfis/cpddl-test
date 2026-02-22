#!/usr/bin/env python3

import sys
import re
from pprint import pprint

Test = {}
GlobalTearDown = False

pat_test = re.compile(r'^\s*TEST\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+)\).*$')
pat_test_cond = re.compile(r'^\s*TEST_COND\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+), *([a-zA-Z0-9_ ]+)\).*$')
pat_test_tear_down = re.compile(r'^\s*TEST_TEAR_DOWN\(([a-zA-Z0-9_]+) *\).*$')
pat_test_explicit = re.compile(r'^\s*TEST_ONCE\(([a-zA-Z0-9_]+) *\).*$')
pat_global_tear_down = re.compile(r'^\s*TEST_GLOBAL_TEAR_DOWN.*$')
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
    disabled = False
    with open(filename, 'r') as fin:
        for line in fin:
            if disabled and line.startswith('#endif'):
                disabled = False
                continue
            if disabled:
                continue
            if line.startswith('#if 0'):
                disabled = True
                continue

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

def removeUnreachable():
    change = True
    while change:
        change = False
        for k in Test.keys():
            dep = Test[k]['dep']
            if dep is not None and dep != '_' and dep not in Test:
                change = True
                del Test[k]
                break



def genDeclarations():
    keys = sorted(Test.keys())
    for key in keys:
        print('void test_{0}(void);'.format(key))
        if Test[key]['tear-down']:
            print('void test_tear_down_{0}(void);'.format(key))

def constructTree():
    keys = sorted(Test.keys())
    for idx, key in enumerate(keys):
        Test[key]['id'] = idx
        Test[key]['child'] = []
        Test[key]['is_root'] = False

    for idx, key in enumerate(keys):
        if Test[key]['dep'] is None or Test[key]['dep'] == '_':
            Test[key]['is_root'] = True
        else:
            Test[Test[key]['dep']]['child'] += [idx]

def genChildArrays():
    keys = sorted(Test.keys())
    for idx, key in enumerate(keys):
        if len(Test[key]['child']) == 0:
            continue
        ch = Test[key]['child']
        print('static int _test_{0}__child[{1}] = {{{2}}};' \
                .format(key, len(ch), ', '.join([str(x) for x in ch])))

def genTestDef(idx, name, test):
    print('    {', end = '')

    # .id
    print('{0}'.format(idx), end = '')

    # .name and .fn
    print(', "{0}", test_{0}'.format(name), end = '')

    # .fn_tear_down
    if test['tear-down']:
        print(', test_tear_down_{0}'.format(name), end = '')
    else:
        print(', NULL', end = '')

    # .parent
    if test['dep'] is not None and test['dep'] != '_':
        print(', {0}'.format(Test[test['dep']]['id']), end = '')
    else:
        print(', -1', end = '')

    # .children and .children_size
    if len(test['child']) > 0:
        print(', _test_{0}__child, {1}'.format(name, len(test['child'])), end = '')
    else:
        print(', NULL, 0', end = '')

    # .is_explicit
    if test['explicit']:
        print(', 1', end = '')
    else:
        print(', 0', end = '')

    # .enabled
    print(', 0', end = '');
    print('},')

def genDefs():
    constructTree()
    genChildArrays()

    keys = sorted(Test.keys())
    print('static test_def_t test_set[] = {')
    for idx, key in enumerate(keys):
        genTestDef(idx, key, Test[key])
    print('};')
    print('#define test_set_size {0}'.format(len(keys)))

    if GlobalTearDown:
        print('#define USE_GLOBAL_TEAR_DOWN')
        print('void __test_global_tear_down(void);')

def main():
    parseConfig('../pddl/config.h')
    for filename in sys.argv[1:]:
        parseFile(filename)
    removeUnreachable()
    #pprint(Test)
    genDeclarations()
    genDefs()

if __name__ == '__main__':
    main()
