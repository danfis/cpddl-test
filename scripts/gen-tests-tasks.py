#!/usr/bin/env python3
# Generates a combined C source file (tests_tasks.in.c) for the test runner
# by scanning the given C source files for TEST/TEST_COND/TEST_ONCE macros,
# reading pddl/config.h for feature flags, and reading config.toml for the
# task configuration.
#
# Usage:
#   gen-tests-tasks.py [--config-header PATH] config.toml src/file1.c ... \
#       > src/tests_tasks.in.c
#

import sys
import re
import argparse
import tomllib
from pprint import pprint

# ---------------------------------------------------------------------------
# Test-registry state
# ---------------------------------------------------------------------------

Test = {}
GlobalTearDown = False

pat_test = re.compile(
    r'^\s*TEST\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+)\).*$')
pat_test_cond = re.compile(
    r'^\s*TEST_COND\(([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+)'
    r', *([a-zA-Z0-9_ ]+)\).*$')
pat_test_tear_down = re.compile(
    r'^\s*TEST_TEAR_DOWN\(([a-zA-Z0-9_]+) *\).*$')
pat_test_explicit = re.compile(
    r'^\s*TEST_ONCE\(([a-zA-Z0-9_]+) *\).*$')
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
        Test[name] = {
            'dep': None,
            'tear-down': False,
            'explicit': False
        }
    return True


def parseFile(filename):
    global Test, GlobalTearDown
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

            m = pat_test.match(line)
            if m is not None:
                name = m.group(1)
                if addTest(name, []):
                    Test[name]['dep'] = m.group(2)

            m = pat_test_cond.match(line)
            if m is not None:
                name = m.group(1)
                tags = [x for x in m.group(3).split() if x]
                if addTest(name, tags):
                    Test[name]['dep'] = m.group(2)

            m = pat_test_tear_down.match(line)
            if m is not None:
                name = m.group(1)
                if name in Test:
                    Test[name]['tear-down'] = True

            m = pat_test_explicit.match(line)
            if m is not None:
                name = m.group(1)
                if addTest(name, []):
                    Test[name]['explicit'] = True

            m = pat_global_tear_down.match(line)
            if m is not None:
                GlobalTearDown = True


def removeUnreachable():
    change = True
    while change:
        change = False
        for k in list(Test.keys()):
            dep = Test[k]['dep']
            if dep is not None and dep != '_' and dep not in Test:
                change = True
                del Test[k]
                break


def constructTree():
    keys = sorted(Test.keys())
    for idx, key in enumerate(keys):
        Test[key]['id'] = idx
        Test[key]['child'] = []
        Test[key]['is_root'] = False
    for key in keys:
        dep = Test[key]['dep']
        if dep is None or dep == '_':
            Test[key]['is_root'] = True
        else:
            Test[dep]['child'].append(Test[key]['id'])


# ---------------------------------------------------------------------------
# Task-registry state
# ---------------------------------------------------------------------------

Task = {}


def parseTasks(cfg):
    """Populate Task dict from the parsed TOML config."""
    global Task

    if 'annotations' in cfg:
        raise ValueError(
            '[annotations] is no longer supported; rename it to [test-sets].')

    test_sets = cfg.get('test-sets', {})

    disable_map = cfg.get('disable', {})
    if disable_map:
        raise ValueError(
            'Top-level [disable] table is no longer supported; '
            'move disable lists into each [[tasks]] entry.')

    task_set_cfg = cfg.get('task-set', {})
    base_set = set(task_set_cfg.get('base', []))
    quick_set = set(task_set_cfg.get('quick', []))

    def expand(items, context):
        """Expand test-set:name references into their constituent test names."""
        result = []
        for item in items:
            if item.startswith('test-set:'):
                set_name = item[len('test-set:'):]
                if set_name not in test_sets:
                    raise ValueError(
                        f'Unknown test-set {set_name!r} referenced in {context}')
                result += test_sets[set_name]
            else:
                result.append(item)
        return result

    # Auto-generate the special "_" task from all TEST_ONCE tests.
    once_tests = sorted(name for name, t in Test.items() if t['explicit'])
    Task['_'] = {
        'disabled': [],
        'enabled': once_tests,
        'is-base': True,
        'is-quick': True,
    }

    for entry in cfg.get('tasks', []):
        name = entry['name']
        if name == '_':
            raise ValueError(
                'Task "_" must not be defined in config.toml; '
                'it is generated automatically from TEST_ONCE macros.')
        if name in Task:
            raise ValueError(f'Task {name!r} defined more than once in config.toml')

        raw_disable = entry.get('disable', [])
        raw_enable  = entry.get('enable',  [])

        disabled = expand(raw_disable, 'disable')
        enabled  = expand(raw_enable,  'enable')

        Task[name] = {
            'disabled': sorted(set(disabled)),
            'enabled': sorted(set(enabled)),
            'is-base': name in base_set,
            'is-quick': name in quick_set,
        }


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

TASK_DEF_HEADER = '''
struct task_def {
    int id;
    char *name;
    int enabled;
    int enabled_tests[test_set_size];
    int default_disabled_tests[test_set_size];
    int default_enabled_tests[test_set_size];
    int is_base;
    int is_quick;
};
typedef struct task_def task_def_t;

'''


def genTestDeclarations():
    for key in sorted(Test.keys()):
        print(f'void test_{key}(void);')
        if Test[key]['tear-down']:
            print(f'void test_tear_down_{key}(void);')


def genTestChildArrays():
    for key in sorted(Test.keys()):
        ch = Test[key]['child']
        if not ch:
            continue
        inner = ', '.join(str(x) for x in ch)
        print(f'static int _test_{key}__child[{len(ch)}] = {{{inner}}};')


def genTestDef(idx, name, test):
    parts = [str(idx), f'"{name}", test_{name}']

    if test['tear-down']:
        parts.append(f'test_tear_down_{name}')
    else:
        parts.append('NULL')

    dep = test['dep']
    if dep is not None and dep != '_':
        parts.append(str(Test[dep]['id']))
    else:
        parts.append('-1')

    ch = test['child']
    if ch:
        parts.append(f'_test_{name}__child, {len(ch)}')
    else:
        parts.append('NULL, 0')

    parts.append('1' if test['explicit'] else '0')
    parts.append('0')

    print('    {' + ', '.join(parts) + '},')


def genTestSet():
    constructTree()
    genTestChildArrays()
    keys = sorted(Test.keys())
    print('static test_def_t test_set[] = {')
    for idx, key in enumerate(keys):
        genTestDef(idx, key, Test[key])
    print('};')
    print(f'#define test_set_size {len(keys)}')
    if GlobalTearDown:
        print('#define USE_GLOBAL_TEAR_DOWN')
        print('void __test_global_tear_down(void);')


def genTaskDefs():
    print(TASK_DEF_HEADER)

    task_names = sorted(Task.keys())
    print('static task_def_t tasks[] = {')
    for idx, name in enumerate(task_names):
        is_base = 1 if Task[name]['is-base'] else 0
        is_quick = 1 if Task[name]['is-quick'] else 0
        print(f'    {{')
        print(f'        .id = {idx},')
        print(f'        .name = "{name}",')
        print(f'        .enabled = 0,')
        print(f'        .enabled_tests = {{ 0 }},')
        print(f'        .default_disabled_tests = {{ 0 }},')
        print(f'        .default_enabled_tests = {{ 0 }},')
        print(f'        .is_base = {is_base},')
        print(f'        .is_quick = {is_quick}')
        print(f'    }},')
    print('};')
    print(f'static const int tasks_size = {len(task_names)};')

    print('static void tasksInit(void)')
    print('{')
    for idx, name in enumerate(task_names):
        task = Task[name]
        for d in task['disabled']:
            if d not in Test:
                print(f'Warning: task {name!r} disables unknown test {d!r}',
                      file=sys.stderr)
                continue
            print(f'    tasks[{idx}].default_disabled_tests[{Test[d]["id"]}] = 1;')

        for d in task['enabled']:
            if d not in Test:
                print(f'Warning: task {name!r} enables unknown test {d!r}',
                      file=sys.stderr)
                continue
            print(f'    tasks[{idx}].default_enabled_tests[{Test[d]["id"]}] = 1;')
    print('}')


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Generate combined test/task registry C source.')
    parser.add_argument(
        '--config-header', default='../pddl/config.h', metavar='PATH',
        help='Path to pddl/config.h (default: ../pddl/config.h)')
    parser.add_argument(
        'config_toml', metavar='config.toml',
        help='TOML file describing tasks')
    parser.add_argument(
        'src_files', metavar='src.c', nargs='+',
        help='C source files to scan for TEST macros')
    args = parser.parse_args()

    parseConfig(args.config_header)
    for fn in args.src_files:
        parseFile(fn)
    removeUnreachable()

    with open(args.config_toml, 'rb') as f:
        cfg = tomllib.load(f)
    parseTasks(cfg)

    genTestDeclarations()
    genTestSet()
    genTaskDefs()


if __name__ == '__main__':
    main()
