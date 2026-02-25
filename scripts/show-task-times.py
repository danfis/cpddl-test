#!/usr/bin/env python3
"""Print a table of total test time per task, reading from reg/<task>/*.time.tmp files."""

import os
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    reg_dir = os.path.join(script_dir, '..', 'reg')
    reg_dir = os.path.normpath(reg_dir)

    if not os.path.isdir(reg_dir):
        print(f"Error: reg directory not found: {reg_dir}", file=sys.stderr)
        sys.exit(1)

    task_times = {}
    task_fails = {}
    task_ok = {}
    for dirpath, dirnames, filenames in os.walk(reg_dir):
        time_files = [f for f in filenames if f.endswith('.time.tmp')]
        if not time_files:
            continue
        task = os.path.relpath(dirpath, reg_dir)
        total = 0.0
        for fname in time_files:
            fpath = os.path.join(dirpath, fname)
            try:
                with open(fpath) as f:
                    total += float(f.read().strip())
            except (ValueError, OSError):
                pass
        task_times[task] = total
        task_fails[task] = sum(1 for f in filenames if f.endswith('.fail.tmp'))
        task_ok[task] = len(time_files) - task_fails[task]

    if not task_times:
        print("No .time.tmp files found.")
        return

    sorted_tasks = sorted(task_times.items(),
                          key=lambda x: (x[0] != '_', x[0]))
    max_name_len = max(len(t) for t, _ in sorted_tasks)
    col_width = max(max_name_len, len('Task'))

    header = f"{'Task':<{col_width}}  {'Time (s)':>10}  {'OK':>6}  {'Failed':>6}"
    sep = '-' * (col_width + 2 + 10 + 2 + 6 + 2 + 6)
    print(header)
    print(sep)
    for task, total in sorted_tasks:
        fails = task_fails[task]
        fail_str = str(fails) if fails > 0 else '-'
        print(f"{task:<{col_width}}  {total:>10.3f}  {task_ok[task]:>6}  {fail_str:>6}")
    print(sep)
    grand_total = sum(t for _, t in sorted_tasks)
    grand_ok = sum(task_ok.values())
    grand_fails = sum(task_fails.values())
    grand_fail_str = str(grand_fails) if grand_fails > 0 else '-'
    print(f"{'TOTAL':<{col_width}}  {grand_total:>10.3f}  {grand_ok:>6}  {grand_fail_str:>6}")

if __name__ == '__main__':
    main()
