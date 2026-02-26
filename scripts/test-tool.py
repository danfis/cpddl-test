#!/usr/bin/env python3
"""Regression test runner for bin/pddl-tool.

Run from the tests/ directory:
    python3 scripts/test-tool.py [options]
"""

import argparse
import os
import re
import subprocess
import sys
import time
import tomllib
from concurrent.futures import ThreadPoolExecutor, as_completed

# ANSI color codes
RED   = "\033[31m"
GREEN = "\033[32m"
RESET = "\033[0m"


def parse_args():
    p = argparse.ArgumentParser(
        description="Regression test runner for bin/pddl-tool")
    p.add_argument(
        "--config", default="config-tool.toml",
        help="Path to the TOML config file (default: config-tool.toml)")
    p.add_argument(
        "-p", "--parallel", type=int, default=1,
        help="Number of parallel workers (default: 1)")
    p.add_argument(
        "--valgrind", action="store_true",
        help="Run under valgrind")
    p.add_argument(
        "--valgrind-full", action="store_true",
        help="Run under valgrind --leak-check=full --show-reachable=yes")
    p.add_argument(
        "--gdb", action="store_true",
        help="Run under gdb --ex 'set follow-fork-mode child' --ex run --args")
    p.add_argument(
        "--bwrap", action="store_true",
        help="Sandbox each test with bwrap (bubblewrap)")
    p.add_argument(
        "-t", "--test", action="append", dest="tests",
        metavar="REGEX",
        help="Run only tests whose name matches REGEX (may be repeated)")
    p.add_argument(
        "-T", "--task", action="append", dest="tasks",
        metavar="REGEX",
        help="Run only tasks whose name matches REGEX (may be repeated)")
    p.add_argument(
        "--gen-golden", action="store_true",
        help="Write outputs directly to golden baseline files instead of .tmp")
    return p.parse_args()


def matches_any(name, patterns):
    """Return True if name matches at least one regex pattern."""
    return any(re.search(pat, name) for pat in patterns)


def expand_out(tokens, task, test_name, gen_golden=False):
    """Replace <OUT>xyz with reg/<task>/tool-<test-name>.output.xyz[.tmp].
    Returns (expanded_tokens, list_of_expanded_paths)."""
    prefix = os.path.join("reg", task, f"tool-{test_name}.output.")
    result = []
    out_paths = []
    for t in tokens:
        if "<OUT>" in t:
            expanded = t.replace("<OUT>", prefix)
            if not gen_golden:
                expanded += ".tmp"
            result.append(expanded)
            out_paths.append(expanded)
        else:
            result.append(t)
    return result, out_paths


def build_tool_prefix(args):
    """Build the valgrind/gdb wrapper prefix (without bwrap)."""
    if args.gdb:
        return ["gdb", "--batch",
                "--ex", "set follow-fork-mode child",
                "--ex", "run",
                "--ex", "bt",
                "--ex", "quit",
                "--args"]
    if args.valgrind_full:
        return ["valgrind", "--leak-check=full", "--show-reachable=yes",
                "--suppressions=test.supp"]
    if args.valgrind:
        return ["valgrind", "--suppressions=test.supp"]
    return []


def build_bwrap_prefix(bwrap_base_opts, writable_files):
    """Build the bwrap prefix with --bind for each writable file."""
    bind_opts = []
    for f in writable_files:
        abs_f = os.path.abspath(f)
        bind_opts += ["--bind", abs_f, abs_f]
    return ["bwrap"] + list(bwrap_base_opts) + bind_opts + ["--"]


def check_valgrind_errors(err_tmp, full_leak_check):
    """Parse valgrind stderr and return list of failure reasons."""
    failures = []
    try:
        with open(err_tmp) as fh:
            content = fh.read()
    except OSError:
        return failures

    m = re.search(r'==[0-9]+== ERROR SUMMARY: (\d+) errors? from', content)
    if m and int(m.group(1)) > 0:
        failures.append(f"valgrind: {m.group(1)} error(s) detected")

    for label in ("definitely lost", "indirectly lost"):
        m = re.search(rf'==[0-9]+==.*{label}: ([\d,]+) bytes', content)
        if m:
            n = int(m.group(1).replace(",", ""))
            if n > 0:
                failures.append(f"valgrind: {n} bytes {label}")

    return failures


def run_one(task, test_name, test_opts, cfg, tool_prefix, gen_golden=False,
            use_valgrind=False, full_leak_check=False,
            use_bwrap=False, bwrap_base_opts=(), use_gdb=False):
    """Run a single (task, test) combination. Returns (task, test_name, ok, msg)."""
    max_mem = cfg.get("max-mem", None)
    max_time = cfg.get("max-time", None)
    common_opts = cfg.get("common-options", [])

    reg_dir = os.path.join("reg", task)
    os.makedirs(reg_dir, exist_ok=True)

    if gen_golden:
        out_file = os.path.join(reg_dir, f"tool-{test_name}.out")
    else:
        out_file = os.path.join(reg_dir, f"tool-{test_name}.out.tmp")

    err_tmp   = os.path.join(reg_dir, f"tool-{test_name}.err.tmp")
    log_tmp   = os.path.join(reg_dir, f"tool-{test_name}.log.tmp")
    time_tmp  = os.path.join(reg_dir, f"tool-{test_name}.time.tmp")
    fail_tmp  = os.path.join(reg_dir, f"tool-{test_name}.fail.tmp")
    out_gold  = os.path.join(reg_dir, f"tool-{test_name}.out")

    # Remove stale tmp files
    stale = [err_tmp, log_tmp, time_tmp, fail_tmp]
    if not gen_golden:
        stale.append(out_file)
    for f in stale:
        try:
            os.remove(f)
        except FileNotFoundError:
            pass

    # Expand <OUT> in test options
    expanded_opts, out_paths = expand_out(test_opts, task, test_name, gen_golden)

    # When valgrind is used, redirect log to stderr so valgrind and log mix there
    log_arg = "stderr" if use_valgrind else log_tmp

    # Build the pddl-tool command
    pddl_cmd = (
        ["../bin/pddl-tool"]
        + (["--max-mem", max_mem] if max_mem else [])
        + list(common_opts)
        + ["--log-out", log_arg]
        + expanded_opts
        + [os.path.join("pddl", task)]
    )

    # Optionally wrap with bwrap (outermost sandbox)
    if use_bwrap:
        # Files the tool writes directly: log (unless valgrind) + <OUT> files
        writable = list(out_paths)
        if not use_valgrind:
            # Touch log_tmp so it exists for bind mount
            open(log_tmp, "a").close()
            writable.append(log_tmp)
        # Touch each <OUT> file so bind mount target exists
        for f in out_paths:
            os.makedirs(os.path.dirname(f), exist_ok=True)
            open(f, "a").close()
        bwrap_prefix = build_bwrap_prefix(bwrap_base_opts, writable)
    else:
        bwrap_prefix = []

    # Full command: [bwrap ...] [valgrind/gdb ...] pddl-tool ...
    cmd = bwrap_prefix + tool_prefix + pddl_cmd

    # Wrap with timeout (not when using gdb/valgrind/bwrap wrappers)
    if max_time and not tool_prefix and not bwrap_prefix:
        cmd = ["timeout", str(max_time)] + cmd

    failures = []

    t_start = time.monotonic()
    try:
        result = subprocess.run(
            cmd,
            stdout=open(out_file, "w"),
            stderr=open(err_tmp, "w"),
            timeout=None,
        )
    except Exception as exc:
        with open(fail_tmp, "w") as fh:
            fh.write(f"Exception running command: {exc}\n")
        return task, test_name, False, f"EXCEPTION: {exc}"
    elapsed = time.monotonic() - t_start

    with open(time_tmp, "w") as fh:
        fh.write(f"{elapsed:.3f}\n")

    if gen_golden:
        return task, test_name, True, f"golden written ({elapsed:.2f}s)"

    # Check exit code
    if result.returncode != 0:
        failures.append(f"exit code {result.returncode}")

    # Check stderr / valgrind errors
    if use_valgrind:
        failures += check_valgrind_errors(err_tmp, full_leak_check)
    elif use_gdb:
        with open(err_tmp) as fh:
            err_content = fh.read()
        # GDB prints "No stack." when there is no crash; ignore that
        if err_content.replace("No stack.", "").strip():
            failures.append("non-empty stderr (gdb)")
    elif os.path.getsize(err_tmp) > 0:
        failures.append("non-empty stderr")

    # Check stdout
    if os.path.exists(out_gold):
        diff = subprocess.run(
            ["diff", out_gold, out_file],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if diff.returncode != 0:
            failures.append("stdout differs from golden baseline")
    else:
        if os.path.getsize(out_file) > 0:
            failures.append("non-empty stdout but no golden baseline")

    # Check .output.<suffix>.tmp files against .output.<suffix> golden files
    for entry in os.listdir(reg_dir):
        full = os.path.join(reg_dir, entry)
        if not entry.startswith(f"tool-{test_name}.output."):
            continue
        if not entry.endswith(".tmp"):
            continue
        gold = full[:-len(".tmp")]
        suffix = entry[len(f"tool-{test_name}.output."):-len(".tmp")]
        if os.path.exists(gold):
            diff = subprocess.run(
                ["diff", gold, full],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if diff.returncode != 0:
                failures.append(f"output.{suffix} differs from golden baseline")
        else:
            if os.path.getsize(full) > 0:
                failures.append(f"non-empty output.{suffix} but no golden baseline")

    if failures:
        reason = "; ".join(failures)
        with open(fail_tmp, "w") as fh:
            fh.write(reason + "\n")
        return task, test_name, False, reason

    return task, test_name, True, f"OK ({elapsed:.2f}s)"


def main():
    args = parse_args()

    use_valgrind = args.valgrind or args.valgrind_full

    if args.gen_golden and use_valgrind:
        print("error: --gen-golden cannot be combined with --valgrind or --valgrind-full",
              file=sys.stderr)
        sys.exit(1)

    with open(args.config, "rb") as fh:
        cfg = tomllib.load(fh)

    all_tasks = cfg.get("tasks", [])
    all_tests = cfg.get("tests", {})

    # Apply regex filters
    if args.tasks:
        all_tasks = [t for t in all_tasks if matches_any(t, args.tasks)]
    if args.tests:
        all_tests = {k: v for k, v in all_tests.items()
                     if matches_any(k, args.tests)}

    if not all_tasks:
        print("No tasks to run.", file=sys.stderr)
        sys.exit(1)
    if not all_tests:
        print("No tests to run.", file=sys.stderr)
        sys.exit(1)

    tool_prefix = build_tool_prefix(args)
    bwrap_base_opts = cfg.get("bwrap_options", []) if args.bwrap else []

    work = [
        (task, test_name, test_opts)
        for task in all_tasks
        for test_name, test_opts in all_tests.items()
    ]

    total = len(work)
    passed = 0
    failed = 0

    # Pre-compute column widths for aligned output
    task_w = max((len(t) for t, _, _ in work), default=4)
    test_w = max((len(n) for _, n, _ in work), default=4)

    def job(item):
        task, test_name, test_opts = item
        return run_one(task, test_name, test_opts, cfg, tool_prefix,
                       args.gen_golden, use_valgrind, args.valgrind_full,
                       args.bwrap, bwrap_base_opts, args.gdb)

    with ThreadPoolExecutor(max_workers=args.parallel) as executor:
        futures = {executor.submit(job, item): item for item in work}
        for future in as_completed(futures):
            task, test_name, ok, msg = future.result()
            if ok:
                status = f"{GREEN}[PASS]{RESET}"
                passed += 1
            else:
                status = f"{RED}[FAIL]{RESET}"
                failed += 1
            print(f"{status}  {task:<{task_w}}  {test_name:<{test_w}}  {msg}")

    print(f"\n{total} test(s): {passed} passed, {failed} failed.")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
