#!/usr/bin/env python3
"""Performance comparison runner for bin/pddl-tool.

Runs bin/pddl-tool on a fixed list of PDDL problems (see the configuration
block below), records runtime, peak memory and the "solved" flag of every
run, caches the results under tests/<RESULTS_DIR>/<NAME>/ and prints a
summary comparing all configurations found there.

Typical use:
    1. Edit PDDL_FILES / TOOL_ARGS / MAX_TIME / MAX_MEM below.
    2. python3 tests/scripts/perf-tool.py base      # baseline
    3. Modify the source, rebuild bin/pddl-tool.
    4. python3 tests/scripts/perf-tool.py v1        # compares v1 with base
    5. ... v2, v3, ...

Tests are not re-run unless the PDDL files changed (only the affected tests
are re-run), TOOL_ARGS / limits changed, bin/pddl-tool changed, or -f is
given.

Each run is pinned to its own core (taskset); with -p N the first N (physical)
cores of the process' affinity mask are used. Two instances of this script
running at the same time would therefore share the same cores -- run them
sequentially, or restrict each one with `taskset -c ...` up front (the script
only picks cores from its own affinity mask).
"""

import argparse
import glob
import hashlib
import json
import os
import queue
import re
import shlex
import shutil
import signal
import statistics
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

###############################################################################
# Configuration -- edit this block
###############################################################################

# Base directory of the PDDL files listed in PDDL_FILES.
PDDL_DIR = "~/dev/pddl-data"

# Problems to run, relative to PDDL_DIR, without the .pddl suffix. The path is
# passed to pddl-tool as is; pddl-tool appends .pddl and finds the domain file
# itself.
PDDL_FILES = [
    "test-seq/depot/pfile1",
    "test-seq/depot/pfile2",
    "test-seq/driverlog/pfile1",
    "test-seq/driverlog/pfile2",
    "test-seq/blocksworld/probBLOCKS-10-0",
    "ipc-1998/gripper/prob03",
    "ipc-1998/gripper/prob05",
]

# Name of the plan file the tool writes; a test counts as solved if any file
# matching PLAN_FILE + "*" exists in its run directory after the run.
PLAN_FILE = "plan.out"

# Arguments passed to bin/pddl-tool (after --max-mem and --log-out log.out).
# Placeholders: {plan} -> PLAN_FILE, {problem} -> absolute problem path
# (without .pddl). {problem} is appended if missing.
TOOL_ARGS = ["gplan", "astar", "lmc", "{plan}", "{problem}"]

# Wall-clock limit per run in seconds (enforced by this script; 0 disables it).
MAX_TIME = 60

# Memory limit per run in MB (passed as --max-mem; 0 disables it).
MAX_MEM = 4096

# Directory with the results, relative to the tests/ directory:
# tests/<RESULTS_DIR>/<NAME>/<test-id>/run-<k>/
RESULTS_DIR = "perf"

###############################################################################
# End of configuration
###############################################################################

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
ROOT_DIR = os.path.abspath(os.path.join(TESTS_DIR, ".."))
TOOL_BIN = os.path.join(ROOT_DIR, "bin", "pddl-tool")

STATUS_ORDER = ["solved", "unsolved", "timeout", "memout", "error"]
LOG_PREFIX_RE = re.compile(r"^\[([0-9]+\.[0-9]+)s ([0-9]+)MB\] ?(.*)$")
LOG_PROCESSING_RE = re.compile(r"^PDDL: Processing (.+) and (.+)\.$")
MEMOUT_PATTERNS = ["Allocation of memory failed", "Memory allocation failed"]
KILL_GRACE = 5.0

RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
RESET = "\033[0m"

_print_lock = threading.Lock()


def log(msg=""):
    with _print_lock:
        print(msg, flush=True)


def warn(msg):
    log(f"{YELLOW}warning:{RESET} {msg}")


def die(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


###############################################################################
# Command line
###############################################################################

def parse_args():
    p = argparse.ArgumentParser(
        description="Performance comparison runner for bin/pddl-tool")
    p.add_argument("name", metavar="NAME",
                   help="Name of the configuration (results go to "
                        f"tests/{RESULTS_DIR}/NAME/)")
    p.add_argument("-p", "--parallel", type=int, default=1,
                   help="Number of tests run in parallel, each pinned to its "
                        "own core (default: 1)")
    p.add_argument("-n", "--runs", type=int, default=3,
                   help="Number of runs of each test (default: 3)")
    p.add_argument("-f", "--force", action="store_true",
                   help="Re-run all tests even if results exist")
    p.add_argument("-s", "--summary-only", action="store_true",
                   help="Do not run anything, only print the summary")
    p.add_argument("-c", "--compare", action="append", metavar="NAME",
                   help="Compare only with the given configuration(s) "
                        "(may be repeated; default: all)")
    p.add_argument("--no-pin", action="store_true",
                   help="Do not pin runs to cores")
    args = p.parse_args()
    if args.parallel < 1:
        p.error("--parallel must be >= 1")
    if args.runs < 1:
        p.error("--runs must be >= 1")
    if "/" in args.name or args.name in (".", ".."):
        p.error("NAME must be a plain directory name")
    return args


###############################################################################
# Tests
###############################################################################

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def resolve_tests():
    """Turn PDDL_FILES into a list of test dicts, sorted by test id."""
    pddl_dir = os.path.abspath(os.path.expanduser(PDDL_DIR))
    if not os.path.isdir(pddl_dir):
        die(f"PDDL_DIR does not exist: {pddl_dir}")

    tests = {}
    for entry in PDDL_FILES:
        test_id = os.path.normpath(entry)
        if test_id.endswith(".pddl"):
            test_id = test_id[:-5]
        if test_id.startswith("..") or os.path.isabs(test_id):
            die(f"PDDL_FILES entry must be relative to PDDL_DIR: {entry}")
        if test_id in tests:
            die(f"duplicate test: {test_id}")

        problem = os.path.join(pddl_dir, test_id)
        if not os.path.isfile(problem + ".pddl"):
            die(f"problem file not found: {problem}.pddl")

        tests[test_id] = {
            "id": test_id,
            "problem": problem,
            "problem_sha256": sha256_file(problem + ".pddl"),
        }
    return [tests[k] for k in sorted(tests)]


def domain_sha256(domain):
    """Hash of the domain file recorded from a previous run, or None."""
    if not domain:
        return None
    try:
        return sha256_file(domain)
    except OSError:
        return None


def tool_args_fingerprint():
    return {
        "tool_args": list(TOOL_ARGS),
        "plan_file": PLAN_FILE,
        "max_time": MAX_TIME,
        "max_mem": MAX_MEM,
    }


def build_cmd(test):
    cmd = [TOOL_BIN]
    if MAX_MEM > 0:
        cmd += ["--max-mem", str(MAX_MEM)]
    cmd += ["--log-out", "log.out"]
    subst = {"plan": PLAN_FILE, "problem": test["problem"]}
    args = list(TOOL_ARGS)
    if not any("{problem}" in a for a in args):
        args.append("{problem}")
    for a in args:
        for key, val in subst.items():
            a = a.replace("{" + key + "}", val)
        cmd.append(a)
    return cmd


def git_describe():
    try:
        out = subprocess.run(["git", "describe", "--always", "--dirty"],
                             cwd=ROOT_DIR, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True)
        if out.returncode == 0:
            return out.stdout.strip()
    except OSError:
        pass
    return "?"


###############################################################################
# Core pinning
###############################################################################

def physical_cores(cpus):
    """Keep only one hyper-thread sibling per physical core (best effort)."""
    seen = set()
    out = []
    for cpu in sorted(cpus):
        path = f"/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list"
        try:
            with open(path) as fh:
                key = fh.read().strip()
        except OSError:
            key = str(cpu)
        if key in seen:
            continue
        seen.add(key)
        out.append(cpu)
    return out


def select_cores(n):
    """Return a list of N cpu ids to pin runs to, or None if not possible."""
    try:
        cpus = sorted(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return None
    cores = physical_cores(cpus)
    if len(cores) < n:
        warn(f"only {len(cores)} physical cores available, using "
             "hyper-thread siblings as well")
        cores = cpus
    if len(cores) < n:
        warn(f"only {len(cores)} cpus available for {n} parallel runs")
        return None
    return cores[:n]


###############################################################################
# Running
###############################################################################

class Runner:
    def __init__(self, parallel, pin):
        self.parallel = parallel
        self.cores = queue.Queue()
        self.pin = False
        self.stop = threading.Event()
        self.active = set()
        self.active_lock = threading.Lock()

        if pin:
            cores = select_cores(parallel)
            if cores is None:
                warn("cannot determine cpu affinity, runs are not pinned")
            elif shutil.which("taskset") is None:
                warn("taskset not found, runs are not pinned")
            else:
                self.pin = True
                for c in cores:
                    self.cores.put(c)
        if not self.pin:
            for i in range(parallel):
                self.cores.put(None)

    def run(self, test, run_dir):
        """Run one test in RUN_DIR; return a dict with the measurements."""
        core = self.cores.get()
        try:
            return self._run(test, run_dir, core)
        finally:
            self.cores.put(core)

    def _run(self, test, run_dir, core):
        if os.path.exists(run_dir):
            shutil.rmtree(run_dir)
        os.makedirs(run_dir)

        cmd = build_cmd(test)
        if core is not None:
            cmd = ["taskset", "-c", str(core)] + cmd
        with open(os.path.join(run_dir, "cmd"), "w") as fh:
            fh.write(shlex.join(cmd) + "\n")

        if self.stop.is_set():
            return None

        stdout = open(os.path.join(run_dir, "stdout.log"), "w")
        stderr = open(os.path.join(run_dir, "stderr.log"), "w")
        timed_out = threading.Event()
        t_start = time.monotonic()
        try:
            proc = subprocess.Popen(cmd, cwd=run_dir, stdin=subprocess.DEVNULL,
                                    stdout=stdout, stderr=stderr)
        finally:
            stdout.close()
            stderr.close()

        with self.active_lock:
            self.active.add(proc)

        def kill():
            timed_out.set()
            self._kill(proc)

        timer = None
        if MAX_TIME > 0:
            timer = threading.Timer(MAX_TIME, kill)
            timer.daemon = True
            timer.start()
        try:
            _, status, rusage = os.wait4(proc.pid, 0)
        finally:
            if timer is not None:
                timer.cancel()
            with self.active_lock:
                self.active.discard(proc)
        wall = time.monotonic() - t_start
        exit_code = os.waitstatus_to_exitcode(status)
        proc.returncode = exit_code

        res = {
            "core": core,
            "wall_time": wall,
            "cpu_time": rusage.ru_utime + rusage.ru_stime,
            "max_rss_mb": rusage.ru_maxrss / 1024.0,
            "exit_code": exit_code if exit_code >= 0 else None,
            "signal": -exit_code if exit_code < 0 else None,
            "timed_out": timed_out.is_set(),
        }
        res.update(parse_log(os.path.join(run_dir, "log.out")))
        res.update(parse_plans(run_dir))
        res["status"] = classify(res, run_dir)
        return res

    @staticmethod
    def _kill(proc):
        try:
            proc.send_signal(signal.SIGTERM)
        except OSError:
            return
        deadline = time.monotonic() + KILL_GRACE
        while time.monotonic() < deadline:
            # wait4() in the worker thread reaps the process; once it is
            # gone, the signal fails with ESRCH (or the pid is reused, which
            # we cannot distinguish -- hence the short grace period only).
            try:
                os.kill(proc.pid, 0)
            except OSError:
                return
            if proc.returncode is not None:
                return
            time.sleep(0.1)
        try:
            proc.send_signal(signal.SIGKILL)
        except OSError:
            pass

    def shutdown(self):
        self.stop.set()
        with self.active_lock:
            procs = list(self.active)
        for proc in procs:
            try:
                proc.send_signal(signal.SIGKILL)
            except OSError:
                pass


def parse_log(path):
    """Extract the time/memory reported in the last log line prefix and the
    domain file pddl-tool reports it is processing."""
    res = {"log_time": None, "log_mem": None, "domain": None}
    try:
        with open(path, errors="replace") as fh:
            for line in fh:
                m = LOG_PREFIX_RE.match(line.rstrip("\n"))
                if m is None:
                    continue
                res["log_time"] = float(m.group(1))
                res["log_mem"] = int(m.group(2))
                if res["domain"] is None:
                    d = LOG_PROCESSING_RE.match(m.group(3))
                    if d is not None:
                        res["domain"] = d.group(1)
    except OSError:
        pass
    return res


def parse_plans(run_dir):
    files = sorted(os.path.basename(f)
                   for f in glob.glob(os.path.join(run_dir, glob.escape(PLAN_FILE) + "*")))
    cost = None
    for fn in files:
        try:
            with open(os.path.join(run_dir, fn), errors="replace") as fh:
                for line in fh:
                    if line.startswith(";; Cost:"):
                        c = int(line.split()[2])
                        cost = c if cost is None else min(cost, c)
                        break
        except (OSError, ValueError, IndexError):
            pass
    return {"plan_files": files, "plan_cost": cost}


def classify(res, run_dir):
    if res["plan_files"]:
        return "solved"
    if res["timed_out"]:
        return "timeout"
    try:
        with open(os.path.join(run_dir, "stderr.log"), errors="replace") as fh:
            err = fh.read()
    except OSError:
        err = ""
    if any(p in err for p in MEMOUT_PATTERNS):
        return "memout"
    if res["exit_code"] == 0:
        return "unsolved"
    return "error"


def aggregate(test, runs):
    """Aggregate the per-run results of one test into result.json content."""
    def stat(key):
        vals = [r[key] for r in runs if r.get(key) is not None]
        if not vals:
            return {"mean": None, "min": None, "max": None, "stdev": None}
        return {
            "mean": statistics.fmean(vals),
            "min": min(vals),
            "max": max(vals),
            "stdev": statistics.stdev(vals) if len(vals) > 1 else 0.0,
        }

    counts = {}
    for r in runs:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    # most frequent status, ties broken towards the worst one
    status = max(counts, key=lambda s: (counts[s], STATUS_ORDER.index(s)))
    solved_runs = counts.get("solved", 0)
    costs = [r["plan_cost"] for r in runs if r.get("plan_cost") is not None]
    domains = [r["domain"] for r in runs if r.get("domain")]
    domain = domains[0] if domains else None

    return {
        "id": test["id"],
        "problem": test["problem"],
        "problem_sha256": test["problem_sha256"],
        "domain": domain,
        "domain_sha256": domain_sha256(domain),
        "status": status,
        "solved": solved_runs == len(runs),
        "solved_runs": solved_runs,
        "num_runs": len(runs),
        "wall_time": stat("wall_time"),
        "cpu_time": stat("cpu_time"),
        "max_rss_mb": stat("max_rss_mb"),
        "log_time": stat("log_time"),
        "log_mem": stat("log_mem"),
        "plan_cost": min(costs) if costs else None,
        "runs": runs,
    }


###############################################################################
# Results directory, meta.json, change detection
###############################################################################

def config_dir(name):
    return os.path.join(TESTS_DIR, RESULTS_DIR, name)


def meta_path(name):
    return os.path.join(config_dir(name), "meta.json")


def load_json(path):
    try:
        with open(path) as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def save_json(path, data):
    tmp = path + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(data, fh, indent=2)
        fh.write("\n")
    os.replace(tmp, path)


def load_config(name):
    """Load meta.json and all result.json files of a configuration."""
    meta = load_json(meta_path(name))
    if meta is None:
        return None
    results = {}
    for test_id in meta.get("tests", {}):
        res = load_json(os.path.join(config_dir(name), test_id, "result.json"))
        if res is not None:
            results[test_id] = res
    return {"name": name, "meta": meta, "results": results}


def list_configs():
    base = os.path.join(TESTS_DIR, RESULTS_DIR)
    if not os.path.isdir(base):
        return []
    names = [n for n in sorted(os.listdir(base))
             if os.path.isfile(meta_path(n))]
    return names


def select_tests_to_run(name, tests, tool_sha, fingerprint, runs, force):
    """Return (tests_to_run, reason, old_meta)."""
    cdir = config_dir(name)
    old = load_json(meta_path(name))
    if force:
        return tests, "--force given", old
    if old is None:
        return tests, "no previous results", None
    if old.get("tool_sha256") != tool_sha:
        return tests, "bin/pddl-tool changed", old
    if old.get("tool_args") != fingerprint:
        return tests, "tool arguments or limits changed", old
    if old.get("runs") != runs:
        return tests, f"number of runs changed ({old.get('runs')} -> {runs})", old

    old_tests = old.get("tests", {})
    to_run = []
    for t in tests:
        o = old_tests.get(t["id"])
        if o is None:
            to_run.append(t)
            continue
        if o.get("problem_sha256") != t["problem_sha256"]:
            to_run.append(t)
            continue
        # the domain file is the one pddl-tool reported in the previous run
        if o.get("domain") and domain_sha256(o["domain"]) != o.get("domain_sha256"):
            to_run.append(t)
            continue
        res = load_json(os.path.join(cdir, t["id"], "result.json"))
        if res is None or res.get("num_runs", 0) < runs:
            to_run.append(t)
    return to_run, "PDDL files changed or results missing", old


def run_configuration(args, tests):
    name = args.name
    cdir = config_dir(name)
    if not os.path.isfile(TOOL_BIN):
        die(f"binary not found: {TOOL_BIN}")
    tool_sha = sha256_file(TOOL_BIN)
    fingerprint = tool_args_fingerprint()

    to_run, reason, old = select_tests_to_run(name, tests, tool_sha, fingerprint,
                                              args.runs, args.force)
    os.makedirs(cdir, exist_ok=True)

    ids = {t["id"] for t in tests}
    dropped = [k for k in (old or {}).get("tests", {}) if k not in ids]
    for k in dropped:
        log(f"note: {k} is no longer in PDDL_FILES; {os.path.join(cdir, k)} is left on disk")

    def make_meta():
        # The domain file (and its hash) is taken from result.json, where it
        # is recorded from the log of the run.
        entries = {}
        for t in tests:
            res = load_json(os.path.join(cdir, t["id"], "result.json")) or {}
            entries[t["id"]] = {
                "problem": t["problem"],
                "problem_sha256": t["problem_sha256"],
                "domain": res.get("domain"),
                "domain_sha256": res.get("domain_sha256"),
            }
        return {
            "name": name,
            "created": (old or {}).get("created") or time.strftime("%Y-%m-%d %H:%M:%S"),
            "updated": time.strftime("%Y-%m-%d %H:%M:%S"),
            "git": git_describe(),
            "tool_sha256": tool_sha,
            "tool_args": fingerprint,
            "runs": args.runs,
            "tests": entries,
        }

    if not to_run:
        log(f"{name}: all {len(tests)} tests are up to date, nothing to run")
        save_json(meta_path(name), make_meta())
        return True

    if len(to_run) == len(tests):
        log(f"{name}: running all {len(tests)} tests ({reason})")
    else:
        log(f"{name}: running {len(to_run)} of {len(tests)} tests ({reason})")
    log(f"  tool: {TOOL_BIN} ({git_describe()})")
    log(f"  args: {shlex.join(build_cmd(to_run[0])[1:])}")
    log(f"  runs: {args.runs}, parallel: {args.parallel}, "
        f"max-time: {MAX_TIME}s, max-mem: {MAX_MEM}MB")

    # Remove stale results of the tests that are re-run so that an
    # interrupted run never leaves a mix of old and new runs behind.
    for t in to_run:
        tdir = os.path.join(cdir, t["id"])
        if os.path.exists(tdir):
            shutil.rmtree(tdir)
        os.makedirs(tdir)

    runner = Runner(args.parallel, not args.no_pin)
    if not runner.pin and not args.no_pin:
        warn("runs are not pinned to cores")

    jobs = [(t, k) for t in to_run for k in range(1, args.runs + 1)]
    total = len(jobs)
    id_w = max(len(t["id"]) for t in to_run)
    done_runs = {t["id"]: {} for t in to_run}
    done_lock = threading.Lock()
    ok = True

    def job(item):
        t, k = item
        run_dir = os.path.join(cdir, t["id"], f"run-{k}")
        return t, k, runner.run(t, run_dir)

    executor = ThreadPoolExecutor(max_workers=args.parallel)
    try:
        futures = [executor.submit(job, item) for item in jobs]
        for i, fut in enumerate(as_completed(futures), 1):
            t, k, res = fut.result()
            if res is None:
                continue
            color = GREEN if res["status"] == "solved" else RED
            log(f"[{i:>{len(str(total))}}/{total}] {t['id']:<{id_w}}  run {k}: "
                f"{color}{res['status']:<8}{RESET} {res['wall_time']:8.3f}s "
                f"{res['max_rss_mb']:8.1f}MB")
            with done_lock:
                done_runs[t["id"]][k] = res
                if len(done_runs[t["id"]]) == args.runs:
                    runs = [done_runs[t["id"]][j] for j in sorted(done_runs[t["id"]])]
                    save_json(os.path.join(cdir, t["id"], "result.json"),
                              aggregate(t, runs))
    except KeyboardInterrupt:
        log("\ninterrupted, killing running processes...")
        runner.shutdown()
        for fut in futures:
            fut.cancel()
        ok = False
    finally:
        executor.shutdown(wait=True, cancel_futures=True)

    # Also written after an interrupt: the finished tests are kept and the
    # unfinished ones are re-run next time because their result.json is
    # missing.
    save_json(meta_path(name), make_meta())
    return ok


###############################################################################
# Summary
###############################################################################

def fmt_num(val, prec=3):
    if val is None:
        return "-"
    return f"{val:.{prec}f}"


def print_table(rows, aligns=None):
    """Print ROWS (lists of strings); None rows are separators."""
    ncol = max(len(r) for r in rows if r is not None)
    widths = [0] * ncol
    for r in rows:
        if r is None:
            continue
        for i, c in enumerate(r):
            widths[i] = max(widths[i], len(c))
    for r in rows:
        if r is None:
            log("-" * (sum(widths) + 2 * (ncol - 1)))
            continue
        cells = []
        for i, c in enumerate(r):
            a = aligns[i] if aligns and i < len(aligns) else "<"
            cells.append(f"{c:{a}{widths[i]}}")
        log("  ".join(cells).rstrip())


def print_summary(names):
    configs = [c for c in (load_config(n) for n in names) if c is not None]
    if not configs:
        log("no results found")
        return

    # header: per-configuration description
    log("")
    log("Configurations:")
    rows = [["name", "git", "created", "runs", "max-time", "max-mem", "tool args"]]
    for c in configs:
        m = c["meta"]
        fp = m.get("tool_args", {})
        rows.append([c["name"], str(m.get("git", "?")), str(m.get("created", "?")),
                     str(m.get("runs", "?")), str(fp.get("max_time", "?")),
                     str(fp.get("max_mem", "?")),
                     " ".join(fp.get("tool_args", []))])
    print_table(rows)

    fps = {json.dumps(c["meta"].get("tool_args"), sort_keys=True) for c in configs}
    runs = {c["meta"].get("runs") for c in configs}
    if len(fps) > 1:
        warn("compared configurations differ in tool arguments or limits")
    if len(runs) > 1:
        warn("compared configurations differ in the number of runs")

    base = configs[0]
    test_ids = sorted(set().union(*(c["results"].keys() for c in configs)))

    def solved(c, tid):
        r = c["results"].get(tid)
        return r is not None and r.get("solved")

    def val(c, tid, key):
        return c["results"][tid][key]["mean"]

    def cell(c, tid, key, prec):
        if not solved(c, tid):
            return "-"
        v = val(c, tid, key)
        s = fmt_num(v, prec)
        if c is not base and solved(base, tid):
            b = val(base, tid, key)
            if b is not None and v is not None:
                ratio = v / b if b > 0 else float("inf")
                s += f" ({ratio:.2f}x)"
        return s

    # per-test table
    log("")
    log("Per test (mean over runs; time in seconds, memory = peak RSS in MB;"
        " ratios are relative to " + base["name"] + "):")
    head1 = ["test"]
    head2 = [""]
    aligns = ["<"]
    for c in configs:
        head1 += [c["name"], "", ""]
        head2 += ["status", "time", "mem"]
        aligns += ["<", ">", ">"]
    rows = [head1, head2, None]
    for tid in test_ids:
        row = [tid]
        for c in configs:
            r = c["results"].get(tid)
            if r is None:
                row += ["n/a", "-", "-"]
                continue
            st = r["status"]
            if 0 < r.get("solved_runs", 0) < r.get("num_runs", 1):
                st += f" {r['solved_runs']}/{r['num_runs']}"
            row += [st, cell(c, tid, "wall_time", 3), cell(c, tid, "max_rss_mb", 1)]
        rows.append(row)
    print_table(rows, aligns)

    # totals
    common = [tid for tid in test_ids if all(solved(c, tid) for c in configs)]
    log("")
    log("Totals:")
    rows = [[""] + [c["name"] for c in configs], None]
    aligns = ["<"] + [">"] * len(configs)

    def total_row(label, fn, prec=None):
        row = [label]
        for c in configs:
            v = fn(c)
            row.append(fmt_num(v, prec) if prec is not None else str(v))
        rows.append(row)

    def count(c, status):
        return sum(1 for r in c["results"].values() if r["status"] == status)

    def total(c, key, tids):
        vals = [val(c, tid, key) for tid in tids if solved(c, tid)]
        vals = [v for v in vals if v is not None]
        return sum(vals) if vals else None

    def count_solved(c):
        return sum(1 for tid in test_ids if solved(c, tid))

    total_row("tests", lambda c: len(c["results"]))
    total_row("coverage", count_solved)
    total_row("sum time (solved)", lambda c: total(c, "wall_time", test_ids), 3)
    total_row("sum mem (solved)", lambda c: total(c, "max_rss_mb", test_ids), 1)
    if len(configs) > 1:
        total_row(f"sum time (commonly solved: {len(common)})",
                  lambda c: total(c, "wall_time", common), 3)
        total_row(f"sum mem (commonly solved: {len(common)})",
                  lambda c: total(c, "max_rss_mb", common), 1)
    total_row("timeouts", lambda c: count(c, "timeout"))
    total_row("memouts", lambda c: count(c, "memout"))
    total_row("unsolved (exit 0, no plan)", lambda c: count(c, "unsolved"))
    total_row("errors", lambda c: count(c, "error"))
    print_table(rows, aligns)


###############################################################################

def main():
    args = parse_args()
    ok = True
    if not args.summary_only:
        tests = resolve_tests()
        if not tests:
            die("PDDL_FILES is empty")
        ok = run_configuration(args, tests)

    if args.compare:
        names = [n for n in args.compare if n != args.name]
        for n in names:
            if not os.path.isfile(meta_path(n)):
                die(f"configuration not found: {n}")
    else:
        names = [n for n in list_configs() if n != args.name]
    # oldest first, the current configuration last
    names.sort(key=lambda n: (load_json(meta_path(n)) or {}).get("created", ""))
    if os.path.isfile(meta_path(args.name)):
        names.append(args.name)
    print_summary(names)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
