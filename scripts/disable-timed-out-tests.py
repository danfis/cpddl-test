#!/usr/bin/env python3
"""Disable tests that timed out (Alarm clock signal) in config.toml.

Walks reg/ for .fail.tmp files whose first line contains "Alarm clock",
then adds the corresponding test names to the disable list of the matching
[[tasks]] entry in config.toml.
"""

import os
import re
import sys
from collections import defaultdict

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

REG_DIR = "reg"
CONFIG_FILE = "config.toml"


def find_timed_out_tests(reg_dir):
    """Return dict mapping task path -> set of timed-out test names."""
    timed_out = defaultdict(set)
    for dirpath, _dirs, filenames in os.walk(reg_dir):
        for fname in filenames:
            if not fname.endswith('.fail.tmp'):
                continue
            fpath = os.path.join(dirpath, fname)
            try:
                with open(fpath) as f:
                    first_line = f.readline()
            except OSError:
                continue
            if 'Alarm clock' not in first_line:
                continue
            test = fname[:-len('.fail.tmp')]
            task = os.path.relpath(dirpath, reg_dir)
            timed_out[task].add(test)
    return timed_out


def update_config(config_file, timed_out):
    """Add timed-out tests to disable lists in config_file. Returns number of tasks changed."""
    with open(config_file) as f:
        lines = f.readlines()

    task_starts = [i for i, l in enumerate(lines) if l.strip() == '[[tasks]]']

    # Collect info about each [[tasks]] block
    task_info = []
    for idx, start in enumerate(task_starts):
        end = task_starts[idx + 1] if idx + 1 < len(task_starts) else len(lines)

        name = None
        name_line = None
        disable_open = None
        disable_close = None

        for i in range(start, end):
            stripped = lines[i].strip()
            m = re.match(r'^name\s*=\s*"(.+)"', stripped)
            if m:
                name = m.group(1)
                name_line = i
                continue
            if re.match(r'^disable\s*=\s*\[', stripped):
                disable_open = i
                open_bracket = lines[i].index('[')
                tail = lines[i][open_bracket:]
                if tail.count('[') == tail.count(']'):
                    # single-line disable list
                    disable_close = i
                else:
                    for j in range(i + 1, end):
                        if lines[j].strip().startswith(']'):
                            disable_close = j
                            break

        task_info.append({
            'name': name,
            'name_line': name_line,
            'disable_open': disable_open,
            'disable_close': disable_close,
        })

    # Process in reverse line order so earlier insertions don't shift later indices
    task_info.sort(key=lambda x: x['name_line'] if x['name_line'] is not None else -1,
                   reverse=True)

    changed = 0
    warned = set()
    for ti in task_info:
        name = ti['name']
        if not name or name not in timed_out:
            continue
        new_tests = sorted(timed_out[name])

        if ti['disable_open'] is not None and ti['disable_close'] is not None:
            # Collect already-disabled tests to avoid duplicates
            existing = set()
            for i in range(ti['disable_open'], ti['disable_close'] + 1):
                m = re.match(r'^\s*"(.+)"', lines[i])
                if m:
                    existing.add(m.group(1))
            to_add = sorted(t for t in new_tests if t not in existing)
            if not to_add:
                continue
            insert_at = ti['disable_close']
            lines[insert_at:insert_at] = [f'    "{t}",\n' for t in to_add]
            print(f"  {name}: added to disable: {', '.join(to_add)}")
        else:
            if ti['name_line'] is None:
                warned.add(name)
                continue
            insert_at = ti['name_line'] + 1
            new_block = (['disable = [\n']
                         + [f'    "{t}",\n' for t in new_tests]
                         + [']\n'])
            lines[insert_at:insert_at] = new_block
            print(f"  {name}: created disable list: {', '.join(new_tests)}")

        changed += 1

    for task in sorted(timed_out):
        if task not in {ti['name'] for ti in task_info}:
            print(f"  WARNING: task '{task}' not found in {config_file}", file=sys.stderr)

    if changed:
        with open(config_file, 'w') as f:
            f.writelines(lines)
        print(f"\nUpdated {changed} task(s) in {config_file}.")
    else:
        print("No changes needed.")

    return changed


def main():
    timed_out = find_timed_out_tests(REG_DIR)
    if not timed_out:
        print("No timed-out tests found.")
        return

    print(f"Found timed-out tests for {len(timed_out)} task(s):")
    for task in sorted(timed_out):
        print(f"  {task}: {', '.join(sorted(timed_out[task]))}")
    print()

    update_config(CONFIG_FILE, timed_out)


if __name__ == '__main__':
    main()
