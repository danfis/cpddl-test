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
from pprint import pprint

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

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
pat_test_once = re.compile(
    r'^\s*TEST_ONCE\(([a-zA-Z0-9_]+) *\).*$')
pat_test_panic_once = re.compile(
    r'^\s*TEST_PANIC_ONCE\(([a-zA-Z0-9_]+) *\).*$')
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


def addTest(name, tags, panic=False):
    global CONFIG
    for tag in tags:
        if tag not in CONFIG:
            return False
    global Test
    if name not in Test:
        Test[name] = {
            'dep': None,
            'tear-down': False,
            'once': False,
            'panic': panic
        }
    elif Test[name]['panic'] != panic:
        raise ValueError(
            f'Test {name!r} is declared both with TEST_PANIC_ONCE and with'
            ' another TEST* macro; panic tests must have a unique name.')
    return True


def testFnName(name):
    """Name of the C function implementing the test NAME."""
    if Test[name]['panic']:
        return f'test_panic_{name}'
    return f'test_{name}'


def checkPanicTests():
    """TEST_PANIC_ONCE tests must not have children nor a tear-down.
    Must be called after constructTestTree(), which fills in 'child'."""
    tests_by_id = {t['id']: t for t in Test.values()}
    for name in sorted(Test.keys()):
        if not Test[name]['panic']:
            continue
        if Test[name]['tear-down']:
            raise ValueError(
                f'TEST_PANIC_ONCE {name!r} must not have a tear-down: its'
                ' body terminates the process with PANIC.')
        if Test[name]['child']:
            child = sorted(tests_by_id[c]['name']
                           for c in Test[name]['child'])
            raise ValueError(
                f'TEST_PANIC_ONCE {name!r} must not have children, but it is'
                f' used as a parent of {child}.')


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

            m = pat_test_once.match(line)
            if m is not None:
                name = m.group(1)
                if addTest(name, []):
                    Test[name]['once'] = True

            m = pat_test_panic_once.match(line)
            if m is not None:
                name = m.group(1)
                # A panic test has no parent (dep stays None, i.e., it is a
                # root) and is run only once, i.e., in the "_" task only.
                if addTest(name, [], panic=True):
                    Test[name]['once'] = True

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


def constructTestTree():
    keys = sorted(Test.keys())
    for idx, key in enumerate(keys):
        Test[key]['name'] = key
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

    # Auto-generate the special "_" task from all TEST_ONCE tests and their
    # children.
    once_tests = sorted(name for name, t in Test.items() if t['once'])
    tests_by_id = {t['id']: t for t in Test.values()}
    def add_childrent_to_once_tests(test_name):
        for child_id in Test[test_name]['child']:
            once_tests.append(tests_by_id[child_id]['name'])
            add_childrent_to_once_tests(tests_by_id[child_id]['name'])
    for test_name in once_tests[:]:
        add_childrent_to_once_tests(test_name)
    once_tests = sorted(set(once_tests))
    Task['_'] = {
        'disabled': sorted(name for name in Test if name not in once_tests),
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
        if 'enable' in entry:
            raise ValueError(
                f'Task {name!r}: the "enable" key is no longer supported; '
                'use TEST_SKIP_CHILDREN in the test code instead.')

        raw_disable = entry.get('disable', [])

        disabled = expand(raw_disable, 'disable')
        disabled += once_tests

        Task[name] = {
            'disabled': sorted(set(disabled)),
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
    int allowed_tests[test_set_size];
    int is_base;
    int is_quick;
};
typedef struct task_def task_def_t;

'''


def genTestDeclarations():
    for key in sorted(Test.keys()):
        print(f'void {testFnName(key)}(void);')
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
    tear_down = 'NULL'
    if test['tear-down']:
        tear_down = f'test_tear_down_{name}'

    parent = -1
    if test['dep'] is not None and test['dep'] != '_':
        parent = Test[test['dep']]['id']

    once = '0'
    if test['once']:
        once = '1'

    panic = '0'
    if test['panic']:
        panic = '1'

    print(f'    {{')
    print(f'        .id = {idx},')
    print(f'        .name = "{name}",')
    print(f'        .fn = {testFnName(name)},')
    print(f'        .fn_tear_down = {tear_down},')
    print(f'        .parent = {parent},')
    ch = test['child']
    if ch:
        print(f'        .children = _test_{name}__child,')
        print(f'        .children_size = {len(ch)},')
    else:
        print(f'        .children = NULL,')
        print(f'        .children_size = 0,')

    print(f'        .is_once = {once},')
    print(f'        .is_panic = {panic},')
    print(f'        .enabled = 0,')
    print(f'    }},')


def genTestSet():
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

    tests_by_id = {t['id']: t for t in Test.values()}
    assert(len(tests_by_id.keys()) == max(tests_by_id.keys()) + 1)
    assert(0 in tests_by_id)

    task_names = sorted(Task.keys())
    print('static task_def_t tasks[] = {')
    for idx, name in enumerate(task_names):
        is_base = 1 if Task[name]['is-base'] else 0
        is_quick = 1 if Task[name]['is-quick'] else 0

        enabled_tests = [1] * len(Test.keys())

        def disableChildren(test_id):
            enabled_tests[test_id] = 0
            for child_id in tests_by_id[test_id]['child']:
                disableChildren(child_id)

        for d in Task[name]['disabled']:
            if d in Test:
                enabled_tests[Test[d]['id']] = 0

        for idx, enabled in enumerate(enabled_tests):
            if not enabled:
                disableChildren(idx)

        enabled_list = ', '.join(str(x) for x in enabled_tests)
        print(f'    {{')
        print(f'        .id = {idx},')
        print(f'        .name = "{name}",')
        print(f'        .enabled = 0,')
        print(f'        .enabled_tests = {{ 0 }},')
        print(f'        .allowed_tests = {{ {enabled_list} }},')
        print(f'        .is_base = {is_base},')
        print(f'        .is_quick = {is_quick}')
        print(f'    }},')
    print('};')
    print(f'static const int tasks_size = {len(task_names)};')


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
    constructTestTree()
    checkPanicTests()

    with open(args.config_toml, 'rb') as f:
        cfg = tomllib.load(f)
    parseTasks(cfg)

    genTestDeclarations()
    genTestSet()
    genTaskDefs()


if __name__ == '__main__':
    main()
