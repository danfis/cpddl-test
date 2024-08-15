#!/usr/bin/env python3

import sys
import re
from pprint import pprint

LARGE_TESTS = ['h3', 'h3mgroup', 'symbolic',
               'endomorphism_tss', 'endomorphism_tss_minizinc',
               'endomorphism_tss_nocost',
               'lifted_search', 'lifted_search_unit_cost',
               'search', 'pot_conj']
VERY_LARGE_TESTS = LARGE_TESTS \
    + ['famgroup_maximal', 'fdr_app_op_search', 'fdr_app_op_search_essential',
       'homomorphism_reduce', 'homomorphism_endomorph',
       'homomorphism_endomorph_nocost',
       'endomorphism_fdr', 'endomorphism_fdr_minizinc',
       'endomorphism_fdr_nocost',
       'hpot',
      ]

NUMERIC_TESTS_ENABLED = [
    'r',
#    'pddl',
    'pddl_no_normalize',
#    'pddl_unit_cost',
#    'pddl_clone',
#    'pddl_compile_away_cond_eff',
#    'pddl_compile_away_eq_pred_no_norm',
#    'pddl_compile_away_eq_pred_lmg',
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
Disabled = {}

def parseDisabled(fin = sys.stdin):
    global Disabled
    for line in fin:
        task, test = [x for x in line.strip().split() if len(x) > 0]
        if task not in Disabled:
            Disabled[task] = {}
        Disabled[task][test] = True

def parseFile(fin = sys.stdin, is_base = False):
    global Task
    global Disabled
    for line in fin:
        s = [x for x in line.strip().split() if len(x) > 0]
        task = s[0]
        if task in Task:
            raise Exception('{0} already defined'.format(s[0]))
        disabled = []
        if task in Disabled:
            disabled += list(Disabled[task].keys())
        enabled = []
        large = False
        is_numeric = False
        for x in s[1:]:
            if x == '~L':
                disabled += LARGE_TESTS
            elif x == '~LL':
                disabled += VERY_LARGE_TESTS
            elif x == '~N':
                is_numeric = True
                enabled = NUMERIC_TESTS_ENABLED
            else:
                enabled += [x]
        d = {
            'disabled' : sorted(list(set(disabled))),
            'enabled' : sorted(list(set(enabled))),
            'is-base' : is_base,
            'is-numeric' : is_numeric,
        }
        Task[task] = d



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
            print(f'''    {{
        int test_id = testIdFromName("{d}");
        if (test_id >= 0){{
            tasks[{id}].default_disabled_tests[test_id] = 1;
        }}
    }}''')
        for d in task['enabled']:
            print(f'''    {{
        int test_id = testIdFromName("{d}");
        if (test_id >= 0){{
            tasks[{id}].default_enabled_tests[test_id] = 1;
        }}
    }}''')
        if (task['is-numeric']):
            print(f'''    {{
        for (int test_id = 0; test_id < test_set_size; ++test_id){{
            if (!tasks[{id}].default_enabled_tests[test_id])
                tasks[{id}].default_disabled_tests[test_id] = 1;
        }}
    }}''')
    print('}')
    pass

def main():
    if len(sys.argv) == 4:
        with open(sys.argv[1], 'r') as fin:
            parseDisabled(fin)
        with open(sys.argv[2], 'r') as fin:
            parseFile(fin, True)
        with open(sys.argv[3], 'r') as fin:
            parseFile(fin, False)
        genDefs()
        sys.exit(0)
    else:
        print('Usage: {0} tasks-disable.txt tasks-base.txt tasks-all.txt >tasks.in.c')
        sys.exit(-1)

if __name__ == '__main__':
    main()

