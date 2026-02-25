#!/usr/bin/env python3
"""Interactive ncurses browser for failed tests.

Navigation:
  UP/DOWN  - move selection
  SPACE    - toggle side-by-side diff view below selected item
  ENTER    - open vimdiff for selected item
  q / ESC  - quit
"""

import curses
import difflib
import os
import re
import subprocess
import sys
import argparse


# Maximum diff lines shown when expanded
MAX_DIFF_LINES = 20


def find_failures(reg_dir):
    """Return sorted list of (task, test, dirpath) for every .fail.tmp file."""
    failures = []
    for dirpath, _dirs, filenames in os.walk(reg_dir):
        for fname in filenames:
            if fname.endswith('.fail.tmp'):
                test = fname[:-len('.fail.tmp')]
                task = os.path.relpath(dirpath, reg_dir)
                failures.append((task, test, dirpath))
    failures.sort(key=lambda x: (x[0], x[1]))
    return failures


def read_lines(path):
    try:
        with open(path) as f:
            return f.readlines()
    except OSError:
        return []


def side_by_side_diff(left_lines, right_lines, half_width):
    """Yield (tag, left_str, right_str) for each diff row.

    tag is one of ' ' (equal), '-' (deleted), '+' (inserted), '~' (replaced).
    """
    matcher = difflib.SequenceMatcher(None, left_lines, right_lines, autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == 'equal':
            for l, r in zip(left_lines[i1:i2], right_lines[j1:j2]):
                yield (' ', l.rstrip(), r.rstrip())
        elif tag == 'replace':
            ls, rs = left_lines[i1:i2], right_lines[j1:j2]
            for k in range(max(len(ls), len(rs))):
                l = ls[k].rstrip() if k < len(ls) else ''
                r = rs[k].rstrip() if k < len(rs) else ''
                yield ('~', l, r)
        elif tag == 'delete':
            for l in left_lines[i1:i2]:
                yield ('-', l.rstrip(), '')
        elif tag == 'insert':
            for r in right_lines[j1:j2]:
                yield ('+', '', r.rstrip())


def clip(s, width):
    if len(s) > width:
        return s[:width - 1] + '>'
    return s


def init_colors():
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)   # selected row
    curses.init_pair(2, curses.COLOR_RED,    -1)                  # deleted
    curses.init_pair(3, curses.COLOR_GREEN,  -1)                  # inserted
    curses.init_pair(4, curses.COLOR_YELLOW, -1)                  # replaced
    curses.init_pair(5, curses.COLOR_CYAN,   -1)                  # diff header


def draw(stdscr, failures, selected, scroll_top, expanded, err_expanded,
         task_col_w, test_col_w, max_diff_lines, max_err_lines):
    stdscr.erase()
    height, width = stdscr.getmaxyx()
    body_height = height - 1  # last line reserved for status bar

    item_rows = {}   # item_idx -> screen row
    row = 0
    idx = scroll_top

    while idx < len(failures) and row < body_height:
        task, test, dirpath = failures[idx]
        label = f" {task:<{task_col_w}}  {test:<{test_col_w}}"
        item_rows[idx] = row

        attr = curses.color_pair(1) | curses.A_BOLD if idx == selected else curses.A_NORMAL
        try:
            stdscr.addstr(row, 0, clip(label, width).ljust(min(width - 1, width)), attr)
        except curses.error:
            pass
        row += 1

        if idx in expanded and row < body_height:
            out_path     = os.path.join(dirpath, test + '.out')
            out_tmp_path = os.path.join(dirpath, test + '.out.tmp')
            left_lines  = read_lines(out_path)
            right_lines = read_lines(out_tmp_path)

            half = max(1, (width - 3) // 2)
            hdr = clip('.out', half).ljust(half) + ' | ' + clip('.out.tmp', half)
            try:
                stdscr.addstr(row, 0, hdr[:width - 1], curses.color_pair(5) | curses.A_BOLD)
            except curses.error:
                pass
            row += 1

            shown = 0
            for tag, left, right in side_by_side_diff(left_lines, right_lines, half):
                if row >= body_height or shown >= max_diff_lines:
                    break
                lstr = clip(left,  half).ljust(half)
                rstr = clip(right, half)
                line = (lstr + ' | ' + rstr)[:width - 1]
                if tag == '-':
                    attr = curses.color_pair(2)
                elif tag == '+':
                    attr = curses.color_pair(3)
                elif tag == '~':
                    attr = curses.color_pair(4)
                else:
                    attr = curses.A_NORMAL
                try:
                    stdscr.addstr(row, 0, line, attr)
                except curses.error:
                    pass
                row += 1
                shown += 1

        if idx in err_expanded and row < body_height:
            err_tmp_path = os.path.join(dirpath, test + '.err.tmp')
            err_lines = read_lines(err_tmp_path)
            hdr = f" -- {test}.err.tmp --"
            try:
                stdscr.addstr(row, 0, hdr[:width - 1], curses.color_pair(5) | curses.A_BOLD)
            except curses.error:
                pass
            row += 1
            shown = 0
            for line in err_lines:
                if row >= body_height or shown >= max_err_lines:
                    break
                try:
                    stdscr.addstr(row, 0, clip(line.rstrip(), width - 1))
                except curses.error:
                    pass
                row += 1
                shown += 1

        idx += 1

    # Status bar
    status = (f" {selected + 1}/{len(failures)}"
              f"  |  UP/DOWN: navigate"
              f"  SPACE: toggle diff  e: toggle err  E: nvim err"
              f"  ENTER: vimdiff"
              f"  q/ESC: quit")
    try:
        stdscr.addstr(height - 1, 0, status[:width - 1].ljust(width - 1), curses.A_REVERSE)
    except curses.error:
        pass

    stdscr.refresh()
    return item_rows


def main(stdscr, task_filter=None, max_diff_lines=MAX_DIFF_LINES, max_err_lines=MAX_DIFF_LINES):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    reg_dir = os.path.normpath(os.path.join(script_dir, '..', 'reg'))

    all_failures = find_failures(reg_dir)
    if task_filter is not None:
        failures = [(task, test, d) for task, test, d in all_failures
                    if task_filter(task, test)]
    else:
        failures = all_failures

    curses.curs_set(0)
    init_colors()

    if not failures:
        stdscr.addstr(0, 0, "No failures found.")
        stdscr.refresh()
        stdscr.getch()
        return

    selected   = 0
    scroll_top = 0
    expanded     = set()
    err_expanded = set()
    item_rows  = {}

    task_col_w = max(len(t) for t, _, _ in failures)
    test_col_w = max(len(t) for _, t, _ in failures)

    while True:
        item_rows = draw(stdscr, failures, selected, scroll_top, expanded, err_expanded,
                         task_col_w, test_col_w, max_diff_lines, max_err_lines)

        key = stdscr.getch()

        if key in (ord('q'), 27):   # q or ESC
            break

        elif key in (curses.KEY_DOWN, ord('j')):
            if selected < len(failures) - 1:
                selected += 1
                if selected not in item_rows:
                    scroll_top += 1

        elif key in (curses.KEY_UP, ord('k')):
            if selected > 0:
                selected -= 1
                if selected < scroll_top:
                    scroll_top = selected

        elif key == ord(' '):
            if selected in expanded:
                expanded.discard(selected)
            else:
                expanded.add(selected)

        elif key == ord('e'):
            if selected in err_expanded:
                err_expanded.discard(selected)
            else:
                err_expanded.add(selected)

        elif key == ord('E'):
            task, test, dirpath = failures[selected]
            err_tmp_path = os.path.join(dirpath, test + '.err.tmp')
            curses.def_prog_mode()
            curses.endwin()
            subprocess.call(['nvim', err_tmp_path])
            curses.reset_prog_mode()
            stdscr.refresh()

        elif key in (curses.KEY_ENTER, 10, 13):
            task, test, dirpath = failures[selected]
            out_path     = os.path.join(dirpath, test + '.out')
            out_tmp_path = os.path.join(dirpath, test + '.out.tmp')
            curses.def_prog_mode()
            curses.endwin()
            subprocess.call(['nvim', '-d', out_path, out_tmp_path])
            curses.reset_prog_mode()
            stdscr.refresh()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Interactive ncurses browser for failed tests.')
    parser.add_argument('-T', '--task', metavar='REGEX',
                        help='Show only tasks matching this regex (substring match)')
    parser.add_argument('-n', '--lines', metavar='N', type=int, default=MAX_DIFF_LINES,
                        help=f'Number of diff lines shown on SPACE (default: {MAX_DIFF_LINES})')
    parser.add_argument('-m', '--err-lines', metavar='M', type=int, default=MAX_DIFF_LINES,
                        help=f'Number of .err.tmp lines shown on e (default: {MAX_DIFF_LINES})')
    parser.add_argument('-t', '--test', metavar='REGEX',
                        help='Show only tests matching this regex (substring match)')
    args = parser.parse_args()

    try:
        task_re = re.compile(args.task) if args.task else None
        test_re = re.compile(args.test) if args.test else None
    except re.error as e:
        print(f"Invalid regex: {e}", file=sys.stderr)
        sys.exit(1)

    def _filter(task, test):
        if task_re and not task_re.search(task):
            return False
        if test_re and not test_re.search(test):
            return False
        return True

    curses.wrapper(lambda scr: main(scr, task_filter=_filter,
                                    max_diff_lines=args.lines,
                                    max_err_lines=args.err_lines))
