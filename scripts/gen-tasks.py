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


HEADER = '''

struct task_def {
    int id;
    char *name;
    int enabled;
    int enabled_tests[test_set_size];
    int default_disabled_tests[test_set_size];
    int default_enabled_tests[test_set_size];
    int is_base;
};
typedef struct task_def task_def_t;

'''


Task = {}

def parseFile(fin = sys.stdin, is_base = False):
    global Task
    for line in fin:
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
            'is-base' : is_base,
        }
        Task[s[0]] = d



def genDeclarations():
    keys = sorted(Test.keys())
    for key in keys:
        print('void test_{0}(void);'.format(key))
        if Test[key]['tear-down']:
            print('void test_tear_down_{0}(void);'.format(key))

def genDefs():
    print(HEADER)

    task_names = sorted(Task.keys())
    print('static task_def_t tasks[] = {')
    for id, name in enumerate(task_names):
        is_base = 0
        if Task[name]['is-base']:
            is_base = 1
        s = f'''    {{
        .id = {id},
        .name = "{name}",
        .enabled = 0,
        .enabled_tests = {{ 0 }},
        .default_disabled_tests = {{ 0 }},
        .default_enabled_tests = {{ 0 }},
        .is_base = {is_base}
    }},'''
        print(s)
    print('};')

    print('static const int tasks_size = {0};'.format(len(task_names)))

    print('static void tasksInit(void)')
    print('{')
    for id, name in enumerate(task_names):
        task = Task[name]
        for d in task['disabled']:
            print(f'    tasks[{id}].default_disabled_tests[testIdFromName("{d}")] = 1;')
        for d in task['enabled']:
            print(f'    tasks[{id}].default_enabled_tests[testIdFromName("{d}")] = 1;')
    print('}')
    pass

def main():
    if len(sys.argv) == 3:
        with open(sys.argv[1], 'r') as fin:
            parseFile(fin, True)
        with open(sys.argv[2], 'r') as fin:
            parseFile(fin, False)
        genDefs()
        sys.exit(0)
    else:
        print('Usage: {0} tasks-base.txt tasks-all.txt >tasks.in.c')
        sys.exit(-1)

if __name__ == '__main__':
    main()

