#!/usr/bin/env python3

import sys
import re
from pprint import pprint

LARGE_TESTS = ['h3', 'h3mgroup', 'symbolic',
               'endomorphism_tss', 'endomorphism_tss_minizinc',
               'endomorphism_tss_nocost',
               'lifted_search', 'lifted_search_unit_cost']
VERY_LARGE_TESTS = LARGE_TESTS \
    + ['famgroup_maximal', 'fdr_app_op_search', 'fdr_app_op_search_essential',
       'homomorphism_reduce', 'homomorphism_endomorph',
       'homomorphism_endomorph_nocost',
       'endomorphism_fdr', 'endomorphism_fdr_minizinc',
       'endomorphism_fdr_nocost',
       'hpot',
      ]
Task = {}

def parseFile():
    global Task
    for line in sys.stdin:
        s = [x for x in line.strip().split() if len(x) > 0]
        task = s[0]
        if s[0] in Task:
            raise Exception('{0} already defined'.format(s[0]))
        disabled = []
        enabled = []
        large = False
        for x in s[1:]:
            if x == '~L':
                disabled += LARGE_TESTS
            elif x == '~LL':
                disabled += VERY_LARGE_TESTS
            elif x.startswith('!'):
                disabled += [x[1:]]
            else:
                enabled += [x]
        d = {
            'disabled' : sorted(list(set(disabled))),
            'enabled' : sorted(list(set(enabled))),
        }
        Task[s[0]] = d



def genDeclarations():
    keys = sorted(Test.keys())
    for key in keys:
        print('void test_{0}(void);'.format(key))
        if Test[key]['tear-down']:
            print('void test_tear_down_{0}(void);'.format(key))

def genDefs(dname):
    print('static const char *tasks_{0}[] = {{'.format(dname))
    for name in sorted(Task.keys()):
        task = Task[name]
        print('    "{0}",'.format(name))
    print(r'};')
    print('#define tasks_{0}_size {1}'.format(dname, len(Task)))

    print('static int task_test_map_{0}[tasks_{0}_size * test_set_size] = {{0}};' \
                .format(dname, len(Task)))
    print('static void setTaskTestMap_{0}(void)'.format(dname))
    print('{')
    print('    initTaskTestMap(tasks_{0}, task_test_map_{0}, tasks_{0}_size);'.format(dname))
    for idx, name in enumerate(sorted(Task.keys())):
        task = Task[name]
        for d in task['disabled']:
            print('    disableTaskTest(task_test_map_{0}, tasks_{0}_size,' \
                  ' {1}, testIdFromName("{2}"));'.format(dname, idx, d))
        for d in task['enabled']:
            print('    enableTaskTest(task_test_map_{0}, tasks_{0}_size,' \
                  ' {1}, testIdFromName("{2}"));'.format(dname, idx, d))
    print('}')

def main():
    parseFile()
    #genDeclarations()
    genDefs(sys.argv[1])

if __name__ == '__main__':
    main()

