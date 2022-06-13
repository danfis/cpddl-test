#!/usr/bin/env python3

import sys
import re
from pprint import pprint

LARGE_TESTS = ['h3', 'h3mgroup', 'symbolic',
               'endomorphism_tss', 'endomorphism_tss_minizinc',
               'endomorphism_tss_nocost']
VERY_LARGE_TESTS = LARGE_TESTS \
    + ['famgroup_maximal', 'fdr_app_op_search', 'fdr_app_op_search_essential',
       'homomorphism_reduce', 'homomorphism_endomorph',
       'homomorphism_endomorph_nocost',
       'endomorphism_fdr', 'endomorphism_fdr_minizinc',
       'endomorphism_fdr_nocost'
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
            if x == '~LL':
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
    print('static tasks_t tasks_{0} = {{0}};'.format(dname))
    print('static void addTasks_{0}(void)'.format(dname))
    print('{')
    for name in sorted(Task.keys()):
        task = Task[name]
        if len(task['disabled']) > 0 or len(task['enabled']) > 0:
            print('    {');
            print('    task_t *task = addTask(&tasks_{0}, "{1}");' \
                            .format(dname, name))
            for d in task['disabled']:
                print('    disableTaskTest(task, "{0}");'.format(d))
            for d in task['enabled']:
                print('    enableTaskTest(task, "{0}");'.format(d))
            print('    }');
        else:
            print('    addTask(&tasks_{0}, "{1}");'.format(dname, name))
    print('}')

def main():
    parseFile()
    #genDeclarations()
    genDefs(sys.argv[1])

if __name__ == '__main__':
    main()

