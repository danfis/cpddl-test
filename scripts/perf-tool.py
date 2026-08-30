#!/usr/bin/env python3
"""Performance comparison runner for bin/pddl-tool.

Usage:
    tests/scripts/perf-tool.py [-f] [-s] NAME

All settings (PDDL problems, pddl-tool arguments, limits, number of runs,
parallelism, core pinning, ...) are read from tests/config-perf-tool.toml.
If that file does not exist, the script generates it with default values
(fully commented) and exits so that it can be inspected and edited first;
the same happens when the script is run without NAME. The only command line
options are -f/--force (re-run all tests even if results exist) and
-s/--summary-only (print the summary of the existing results, run nothing).

NAME is the name of the configuration being measured: the results are stored
in tests/<results-dir>/NAME/ and the summary printed at the end compares
NAME with all other configurations found in tests/<results-dir>/.

Typical use:
    1. tests/scripts/perf-tool.py                  # generates the config file
    2. edit tests/config-perf-tool.toml
    3. tests/scripts/perf-tool.py base             # baseline
    4. modify the source, rebuild bin/pddl-tool
    5. tests/scripts/perf-tool.py v1               # runs v1, compares to base
    6. ... v2, v3, ...

See the comments in the generated configuration file for the description of
all options.
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
import string
import subprocess
import sys
import threading
import time
import tomllib
from concurrent.futures import ThreadPoolExecutor, as_completed

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
ROOT_DIR = os.path.abspath(os.path.join(TESTS_DIR, ".."))
TOOL_BIN = os.path.join(ROOT_DIR, "bin", "pddl-tool")
CONFIG_PATH = os.path.join(TESTS_DIR, "config-perf-tool.toml")

STATUS_ORDER = ["solved", "unsolved", "timeout", "memout", "error"]
LOG_PREFIX_RE = re.compile(r"^\[([0-9]+\.[0-9]+)s ([0-9]+)MB\] ?(.*)$")
LOG_PROCESSING_RE = re.compile(r"^PDDL: Processing (.+) and (.+)\.$")
MEMOUT_PATTERNS = ["Allocation of memory failed", "Memory allocation failed"]
KILL_GRACE = 5.0

# environment variables used when re-executing the script under `isolate`
ISOLATED_ENV = "PERF_TOOL_ISOLATED"
CORES_ENV = "PERF_TOOL_CORES"
ISOLATE_UNITS = ["init.scope", "system.slice", "user.slice", "machine.slice"]
ISOLATE_SLICE = "perf-tool.slice"

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
# Configuration file
###############################################################################

DEFAULTS = {
    "pddl-dir": "~/dev/pddl-data",
    "plan-file": "plan.out",
    "tool-args": ["gplan", "astar", "blind", "{plan}", "{problem}"],
    "max-time": 10,
    "max-mem": 4096,
    "results-dir": "perf",
    "runs": 3,
    "parallel": 1,
    "pin": True,
    "cores": "",
    "isolate": False,
    "compare": [],
    "pddl-files": [
        "bench/ipc-opt-noce/agricola18/p01",
        "bench/ipc-opt-noce/agricola18/p06",
        "bench/ipc-opt-noce/airport04/p01-airport1-p1",
        "bench/ipc-opt-noce/airport04/p17-airport3-p5",
        "bench/ipc-opt-noce/barman11/pfile01-001",
        "bench/ipc-opt-noce/barman11/pfile02-007",
        "bench/ipc-opt-noce/barman14/p435.1",
        "bench/ipc-opt-noce/barman14/p536.2",
        "bench/ipc-opt-noce/blocks00/probBLOCKS-4-1",
        "bench/ipc-opt-noce/blocks00/probBLOCKS-6-1",
        "bench/ipc-opt-noce/caldera18/p03",
        "bench/ipc-opt-noce/caldera18/p20",
        "bench/ipc-opt-noce/cavediving14/testing07_easy",
        "bench/ipc-opt-noce/cavediving14/testing18_easy",
        "bench/ipc-opt-noce/childsnack14/child-snack_pfile01",
        "bench/ipc-opt-noce/childsnack14/child-snack_pfile04",
        "bench/ipc-opt-noce/data-network18/p01",
        "bench/ipc-opt-noce/data-network18/p06",
        "bench/ipc-opt-noce/depot02/pfile1",
        "bench/ipc-opt-noce/depot02/pfile8",
        "bench/ipc-opt-noce/driverlog02/pfile1",
        "bench/ipc-opt-noce/driverlog02/pfile7",
        "bench/ipc-opt-noce/elevators08/p01",
        "bench/ipc-opt-noce/elevators08/p11",
        "bench/ipc-opt-noce/elevators11/p04",
        "bench/ipc-opt-noce/elevators11/p14",
        "bench/ipc-opt-noce/floortile11/opt-p01-001",
        "bench/ipc-opt-noce/floortile11/opt-p04-007",
        "bench/ipc-opt-noce/floortile14/p01-4-3-2",
        "bench/ipc-opt-noce/floortile14/p02-4-4-2",
        "bench/ipc-opt-noce/folding23/p01",
        "bench/ipc-opt-noce/folding23/p15",
        "bench/ipc-opt-noce/freecell00/pfile1",
        "bench/ipc-opt-noce/freecell00/probfreecell-6-3",
        "bench/ipc-opt-noce/ged14/d-1-4",
        "bench/ipc-opt-noce/ged14/d-2-3",
        "bench/ipc-opt-noce/gripper98/prob01",
        "bench/ipc-opt-noce/gripper98/prob07",
        "bench/ipc-opt-noce/hiking14/ptesting-1-2-3",
        "bench/ipc-opt-noce/hiking14/ptesting-2-2-4",
        "bench/ipc-opt-noce/labyrinth23/p01",
        "bench/ipc-opt-noce/labyrinth23/p06",
        "bench/ipc-opt-noce/logistics00/problogistics-4-0",
        "bench/ipc-opt-noce/logistics00/problogistics-6-9",
        "bench/ipc-opt-noce/logistics98/prob32",
        "bench/ipc-opt-noce/logistics98/prob17",
        "bench/ipc-opt-noce/maintenance14/maintenance.1.3.010.010.2-001",
        "bench/ipc-opt-noce/maintenance14/maintenance.1.3.010.010.2-002",
        "bench/ipc-opt-noce/miconic00/s1-0",
        "bench/ipc-opt-noce/miconic00/s11-0",
        "bench/ipc-opt-noce/movie98/prob01",
        "bench/ipc-opt-noce/movie98/prob11",
        "bench/ipc-opt-noce/mprime98/prob25",
        "bench/ipc-opt-noce/mprime98/prob03",
        "bench/ipc-opt-noce/mystery98/prob25",
        "bench/ipc-opt-noce/mystery98/prob29",
        "bench/ipc-opt-noce/nomystery11/p11",
        "bench/ipc-opt-noce/nomystery11/p04",
        "bench/ipc-opt-noce/openstacks06/p01",
        "bench/ipc-opt-noce/openstacks06/p12",
        "bench/ipc-opt-noce/openstacks08/p01",
        "bench/ipc-opt-noce/openstacks08/p11",
        "bench/ipc-opt-noce/openstacks11/p01",
        "bench/ipc-opt-noce/openstacks11/p07",
        "bench/ipc-opt-noce/openstacks14/p20_1",
        "bench/ipc-opt-noce/openstacks14/p25_3",
        "bench/ipc-opt-noce/organic-synthesis18/p04",
        "bench/ipc-opt-noce/organic-synthesis18/p08",
        "bench/ipc-opt-noce/parcprinter08/p01",
        "bench/ipc-opt-noce/parcprinter08/p14",
        "bench/ipc-opt-noce/parcprinter11/p02",
        "bench/ipc-opt-noce/parcprinter11/p08",
        "bench/ipc-opt-noce/parking11/pfile03-012",
        "bench/ipc-opt-noce/parking11/pfile05-017",
        "bench/ipc-opt-noce/parking14/p_12_7-01",
        "bench/ipc-opt-noce/parking14/p_14_8-03",
        "bench/ipc-opt-noce/pathways06/p01",
        "bench/ipc-opt-noce/pathways06/p10",
        "bench/ipc-opt-noce/pegsol08/p01",
        "bench/ipc-opt-noce/pegsol08/p11",
        "bench/ipc-opt-noce/pegsol11/p01",
        "bench/ipc-opt-noce/pegsol11/p11",
        "bench/ipc-opt-noce/petri-net-alignment18/p01",
        "bench/ipc-opt-noce/petri-net-alignment18/p07",
        "bench/ipc-opt-noce/pipesworld-notankage04/p01-net1-b6-g2",
        "bench/ipc-opt-noce/pipesworld-notankage04/p16-net2-b14-g6",
        "bench/ipc-opt-noce/pipesworld-tankage04/p11-net2-b10-g2-t30",
        "bench/ipc-opt-noce/pipesworld-tankage04/p08-net1-b12-g7-t80",
        "bench/ipc-opt-noce/psr-small04/p01-s2-n1-l2-f50",
        "bench/ipc-opt-noce/psr-small04/p42-s82-n3-l4-f50",
        "bench/ipc-opt-noce/quantum-layout23/p07",
        "bench/ipc-opt-noce/quantum-layout23/p11",
        "bench/ipc-opt-noce/recharging-robots23/p01",
        "bench/ipc-opt-noce/recharging-robots23/p04",
        "bench/ipc-opt-noce/ricochet-robots23/p11",
        "bench/ipc-opt-noce/ricochet-robots23/p08",
        "bench/ipc-opt-noce/rovers06/p02",
        "bench/ipc-opt-noce/rovers06/p14",
        "bench/ipc-opt-noce/satellite02/p01-pfile1",
        "bench/ipc-opt-noce/satellite02/p13-pfile13",
        "bench/ipc-opt-noce/scanalyzer08/p22",
        "bench/ipc-opt-noce/scanalyzer08/p05",
        "bench/ipc-opt-noce/scanalyzer11/p01",
        "bench/ipc-opt-noce/scanalyzer11/p12",
        "bench/ipc-opt-noce/slitherlink23/p01",
        "bench/ipc-opt-noce/slitherlink23/p07",
        "bench/ipc-opt-noce/snake18/p04",
        "bench/ipc-opt-noce/snake18/p10",
        "bench/ipc-opt-noce/sokoban08/p03",
        "bench/ipc-opt-noce/sokoban08/p08",
        "bench/ipc-opt-noce/sokoban11/p01",
        "bench/ipc-opt-noce/sokoban11/p13",
        "bench/ipc-opt-noce/spider18/p01",
        "bench/ipc-opt-noce/spider18/p15",
        "bench/ipc-opt-noce/storage06/p01",
        "bench/ipc-opt-noce/storage06/p11",
        "bench/ipc-opt-noce/termes18/p01",
        "bench/ipc-opt-noce/termes18/p03",
        "bench/ipc-opt-noce/tetris14/p02-4",
        "bench/ipc-opt-noce/tetris14/p04-6",
        "bench/ipc-opt-noce/tidybot11/p01",
        "bench/ipc-opt-noce/tidybot11/p07",
        "bench/ipc-opt-noce/tidybot14/p11",
        "bench/ipc-opt-noce/tidybot14/p08",
        "bench/ipc-opt-noce/tpp06/p01",
        "bench/ipc-opt-noce/tpp06/p09",
        "bench/ipc-opt-noce/transport08/p01",
        "bench/ipc-opt-noce/transport08/p13",
        "bench/ipc-opt-noce/transport11/p03",
        "bench/ipc-opt-noce/transport11/p13",
        "bench/ipc-opt-noce/transport14/p01",
        "bench/ipc-opt-noce/transport14/p06",
        "bench/ipc-opt-noce/trucks06/p01",
        "bench/ipc-opt-noce/trucks06/p13",
        "bench/ipc-opt-noce/visitall11/problem02-half",
        "bench/ipc-opt-noce/visitall11/problem05-half",
        "bench/ipc-opt-noce/visitall14/p-05-5",
        "bench/ipc-opt-noce/visitall14/p-05-8",
        "bench/ipc-opt-noce/woodworking08/p21",
        "bench/ipc-opt-noce/woodworking08/p24",
        "bench/ipc-opt-noce/woodworking11/p02",
        "bench/ipc-opt-noce/woodworking11/p06",
        "bench/ipc-opt-noce/zenotravel02/pfile1",
        "bench/ipc-opt-noce/zenotravel02/pfile7",
    ],
}

# Template of the generated configuration file. Values are substituted for
# the $key placeholders (string.Template), so braces can be used freely.
CONFIG_TEMPLATE = """\
# Configuration for tests/scripts/perf-tool.py -- the performance comparison
# runner for bin/pddl-tool.
#
#     tests/scripts/perf-tool.py NAME
#
# runs bin/pddl-tool (from the top directory of this repository) on every
# problem listed in `pddl-files`, records the runtime, the peak memory and
# whether a plan was found, stores the results in tests/<results-dir>/NAME/
# and prints a summary comparing NAME with all other configurations found in
# tests/<results-dir>/ (coverage, sums of runtime and memory over the solved
# and over the commonly solved tests, numbers of timeouts, memouts and
# errors, plus a per-test table with ratios relative to the oldest
# configuration).
#
# Typical workflow:
#   1. Edit this file (pddl-files, tool-args, limits, ...).
#   2. tests/scripts/perf-tool.py base      -- baseline results
#   3. Change the source code, rebuild bin/pddl-tool.
#   4. tests/scripts/perf-tool.py v1        -- runs v1, prints v1 vs. base
#   5. ... v2, v3, ...
#
# Tests that already have results are not re-run, unless
#   - the PDDL files of the test changed (only the affected tests are re-run),
#   - `tool-args`, `plan-file`, `max-time`, `max-mem` or `runs` changed,
#   - bin/pddl-tool changed (its sha256 is recorded), or
#   - the -f/--force option is given.
# With -s/--summary-only nothing is run and only the summary of the existing
# results is printed.
# An interrupted run (Ctrl-C) keeps the finished tests; the next run
# continues with the unfinished ones.
#
# Layout of the results: tests/<results-dir>/NAME/<test-id>/run-<k>/ holds
# the output of the k-th run of a test (log.out, stdout.log, stderr.log,
# cmd, plan file(s)), <test-id>/result.json the per-run measurements and
# their aggregation, and NAME/meta.json the fingerprint of the configuration
# (tool sha256, arguments, PDDL file hashes) used to decide what to re-run.
#
# All keys below are required; unknown keys are rejected.


# Base directory of the PDDL files listed in `pddl-files`. A leading "~" is
# expanded to the home directory.
pddl-dir = $pddl_dir

# Name of the plan file pddl-tool writes into the run directory; it is
# substituted for the {plan} placeholder in `tool-args`. A run counts as
# solved if any file matching "<plan-file>*" exists in the run directory
# afterwards (e.g. symb-gbfs --anytime writes <plan-file>.00001, ... in
# addition to <plan-file>).
plan-file = $plan_file

# Arguments passed to bin/pddl-tool. The script always prepends
# "--max-mem <max-mem> --log-out log.out" (the --max-mem part only if
# `max-mem` is positive). Placeholders:
#   {plan}    -> the value of `plan-file`
#   {problem} -> absolute path of the problem file without the .pddl suffix
#                (pddl-tool appends .pddl itself and finds the corresponding
#                domain file in the same directory)
# {problem} is appended automatically if it does not appear in the list.
# Examples:
#   ["gplan", "astar", "lmc", "{plan}", "{problem}"]
#   ["gplan", "--h2", "--pot-A+I", "astar", "pot", "{plan}", "{problem}"]
#   ["symb-astar", "A+I", "blind", "{plan}", "{problem}"]
#   ["symb-gbfs", "--anytime", "A+I,gc", "none", "{plan}", "{problem}"]
tool-args = $tool_args

# Wall-clock time limit of one run in seconds, enforced by this script
# (SIGTERM, then SIGKILL after a few seconds). Runs killed by the limit are
# reported as "timeout". 0 = no limit.
max-time = $max_time

# Memory limit of one run in MB, passed to pddl-tool as --max-mem (which
# sets an address-space rlimit, so allocations fail instead of the process
# being killed). Runs that fail with an allocation error are reported as
# "memout". 0 = no limit.
max-mem = $max_mem

# Directory with the results, relative to the tests/ directory (an absolute
# path is used as is): tests/<results-dir>/NAME/<test-id>/run-<k>/. Every
# subdirectory with a meta.json file is treated as a configuration in the
# summary.
results-dir = $results_dir

# How many times every test is run. The summary reports the mean over the
# runs; a test counts as solved only if all its runs found a plan. Changing
# this value re-runs everything.
runs = $runs

# How many tests are run at the same time. Each of them runs on its own core
# (see `pin`), so this should not exceed the number of (physical) cores
# available to the tests.
parallel = $parallel

# Pin every run to one core with taskset (recommended for stable
# measurements). With false, the runs are not pinned; `cores` must then be
# empty and `isolate` false.
pin = $pin

# Which cores to use for the runs, as a cpu list such as "2,4-5" (at least
# `parallel` cores; the first `parallel` of them are used). The empty string
# selects the `parallel` least loaded physical cores automatically (one
# hyper-thread sibling per physical core, chosen by sampling /proc/stat
# briefly before the run; a warning is printed if the chosen cores are not
# idle). Set this explicitly when using cores isolated by the kernel
# (isolcpus=/nohz_full= boot options), which are not in the default
# affinity mask and therefore never selected automatically. Two instances
# of the script running at the same time must be given distinct cores.
cores = $cores

# Keep the selected cores for the tests only. With true, the script uses
# sudo to restrict all other processes -- the init.scope, system.slice,
# user.slice and machine.slice systemd units -- to the remaining cores
# (systemctl set-property --runtime ... AllowedCPUs=), re-executes itself
# inside a dedicated top-level perf-tool.slice scope restricted to the
# selected cores (systemd-run, dropping back to the current user with
# runuser), and lifts the restrictions when it finishes (they are
# runtime-only, i.e. a reboot clears them in any case). Requires systemd
# on a cgroup v2 host with the cpuset controller, sudo, and runuser, and
# at least one core left for the rest of the system. Kernel threads and
# interrupt handlers can still run on the selected cores; for that level
# of isolation boot with isolcpus=/nohz_full= and use `cores` instead.
isolate = $isolate

# Names of the configurations to compare with in the summary (NAME itself
# is always included). The empty list compares with all configurations
# found in tests/<results-dir>/. The configurations are ordered by their
# creation time, NAME last; ratios are relative to the first one.
compare = $compare

# Problems to run: paths relative to `pddl-dir` without the .pddl suffix
# (pddl-tool appends .pddl and finds the domain file itself). The path is
# also the test id used in the result directories and in the summary. The
# domain file actually used by pddl-tool is recorded from its log and its
# changes are detected as well.
pddl-files = $pddl_files
"""


def toml_value(val, multiline=False):
    """Render VAL as a TOML value. Lists are rendered one item per line if
    MULTILINE."""
    if isinstance(val, bool):
        return "true" if val else "false"
    if isinstance(val, (int, float)):
        return str(val)
    if isinstance(val, str):
        return json.dumps(val)
    if isinstance(val, list):
        if not val:
            return "[]"
        if multiline:
            return "[\n" + "".join(f"    {toml_value(v)},\n" for v in val) + "]"
        return "[" + ", ".join(toml_value(v) for v in val) + "]"
    raise TypeError(f"cannot render {val!r} as TOML")


def write_default_config(path):
    subst = {}
    for key, val in DEFAULTS.items():
        subst[key.replace("-", "_")] = toml_value(val, multiline=(key == "pddl-files"))
    text = string.Template(CONFIG_TEMPLATE).substitute(subst)
    with open(path, "w") as fh:
        fh.write(text)


class Config:
    """Validated content of the configuration file, with attributes named
    after the keys (hyphens replaced by underscores)."""

    def __init__(self, path, data):
        self.path = path
        unknown = sorted(set(data) - set(DEFAULTS))
        if unknown:
            hint = ""
            if set(unknown) & {"force", "summary-only"}:
                hint = (" (force and summary-only are the command line options "
                        "-f/--force and -s/--summary-only now; remove them "
                        "from the file)")
            self.error(f"unknown key(s): {', '.join(unknown)}{hint}")
        missing = [k for k in DEFAULTS if k not in data]
        if missing:
            self.error(f"missing key(s): {', '.join(missing)} (all keys are "
                       "required; delete the file to regenerate the defaults)")

        self.pddl_dir = self.expect(data, "pddl-dir", str, nonempty=True)
        self.plan_file = self.expect(data, "plan-file", str, nonempty=True)
        if "/" in self.plan_file:
            self.error("plan-file must be a plain file name")
        self.tool_args = self.expect(data, "tool-args", list, item_type=str)
        self.max_time = self.expect(data, "max-time", (int, float), minimum=0)
        self.max_mem = self.expect(data, "max-mem", int, minimum=0)
        self.results_dir = self.expect(data, "results-dir", str, nonempty=True)
        self.runs = self.expect(data, "runs", int, minimum=1)
        self.parallel = self.expect(data, "parallel", int, minimum=1)
        self.pin = self.expect(data, "pin", bool)
        cores = self.expect(data, "cores", str)
        self.isolate = self.expect(data, "isolate", bool)
        self.compare = self.expect(data, "compare", list, item_type=str)
        self.pddl_files = self.expect(data, "pddl-files", list, item_type=str)

        self.cores = None
        if cores.strip():
            try:
                self.cores = parse_cores(cores)
            except ValueError as exc:
                self.error(f"cores: {exc}")
            if len(self.cores) < self.parallel:
                self.error(f"cores lists {len(self.cores)} cores but parallel = "
                           f"{self.parallel}")
        if not self.pin and (self.cores is not None or self.isolate):
            self.error("pin = false cannot be combined with cores or isolate")
        for n in self.compare:
            if not n or "/" in n or n in (".", ".."):
                self.error(f"compare: invalid configuration name {n!r}")

    def error(self, msg):
        die(f"{self.path}: {msg}")

    def expect(self, data, key, typ, nonempty=False, minimum=None, item_type=None):
        val = data[key]
        # bool is a subclass of int -- keep the types apart
        if isinstance(val, bool) and typ is not bool:
            self.error(f"{key}: expected {self.type_name(typ)}, got a boolean")
        if not isinstance(val, typ):
            self.error(f"{key}: expected {self.type_name(typ)}, got {type(val).__name__}")
        if nonempty and not val:
            self.error(f"{key}: must not be empty")
        if minimum is not None and val < minimum:
            self.error(f"{key}: must be >= {minimum}")
        if item_type is not None:
            for i, item in enumerate(val):
                if not isinstance(item, item_type):
                    self.error(f"{key}[{i}]: expected {self.type_name(item_type)}, "
                               f"got {type(item).__name__}")
        return val

    @staticmethod
    def type_name(typ):
        if isinstance(typ, tuple):
            return " or ".join(t.__name__ for t in typ)
        if typ is str:
            return "a string"
        if typ is bool:
            return "true or false"
        if typ is list:
            return "an array"
        return typ.__name__


def load_config(path):
    try:
        with open(path, "rb") as fh:
            data = tomllib.load(fh)
    except OSError as exc:
        die(f"cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        die(f"{path}: {exc}")
    return Config(path, data)


###############################################################################
# Command line
###############################################################################

def parse_args():
    p = argparse.ArgumentParser(
        description="Performance comparison runner for bin/pddl-tool. All "
                    f"settings are read from {CONFIG_PATH}; the file is "
                    "generated with defaults if it does not exist.")
    p.add_argument("name", metavar="NAME", nargs="?",
                   help="Name of the configuration being measured (results "
                        "go to tests/<results-dir>/NAME/)")
    p.add_argument("-f", "--force", action="store_true",
                   help="Re-run all tests even if up-to-date results exist")
    p.add_argument("-s", "--summary-only", action="store_true",
                   help="Do not run anything, only print the summary of the "
                        "existing results")
    args = p.parse_args()
    if args.force and args.summary_only:
        p.error("--force and --summary-only cannot be combined")
    if args.name is not None and ("/" in args.name or args.name in (".", "..")):
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


def resolve_tests(cfg):
    """Turn pddl-files into a list of test dicts, sorted by test id."""
    pddl_dir = os.path.abspath(os.path.expanduser(cfg.pddl_dir))
    if not os.path.isdir(pddl_dir):
        die(f"pddl-dir does not exist: {pddl_dir}")

    tests = {}
    for entry in cfg.pddl_files:
        test_id = os.path.normpath(entry)
        if test_id.endswith(".pddl"):
            test_id = test_id[:-5]
        if test_id.startswith("..") or os.path.isabs(test_id):
            die(f"pddl-files entry must be relative to pddl-dir: {entry}")
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


def tool_args_fingerprint(cfg):
    return {
        "tool_args": list(cfg.tool_args),
        "plan_file": cfg.plan_file,
        "max_time": cfg.max_time,
        "max_mem": cfg.max_mem,
    }


def build_cmd(cfg, test):
    cmd = [TOOL_BIN]
    if cfg.max_mem > 0:
        cmd += ["--max-mem", str(cfg.max_mem)]
    cmd += ["--log-out", "log.out"]
    subst = {"plan": cfg.plan_file, "problem": test["problem"]}
    args = list(cfg.tool_args)
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


def parse_cores(s):
    """Parse a cpu list such as "0,2-3" into a sorted list of ints."""
    cores = set()
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            if "-" in part:
                lo, hi = part.split("-", 1)
                lo, hi = int(lo), int(hi)
                if lo > hi:
                    raise ValueError
                cores.update(range(lo, hi + 1))
            else:
                cores.add(int(part))
        except ValueError:
            raise ValueError(f"invalid cpu list: {s!r}")
    if not cores or min(cores) < 0:
        raise ValueError(f"invalid cpu list: {s!r}")
    ncpu = os.cpu_count() or 0
    if ncpu and max(cores) >= ncpu:
        raise ValueError(f"cpu list {s!r} exceeds the {ncpu} cpus of this machine")
    return sorted(cores)


def format_cores(cores):
    """Format a list of cpu ids as a compact cpu list, e.g. "0,2-3"."""
    out = []
    cores = sorted(cores)
    i = 0
    while i < len(cores):
        j = i
        while j + 1 < len(cores) and cores[j + 1] == cores[j] + 1:
            j += 1
        out.append(str(cores[i]) if i == j else f"{cores[i]}-{cores[j]}")
        i = j + 1
    return ",".join(out)


def read_proc_stat():
    """Return {cpu_id: (busy_ticks, total_ticks)} from /proc/stat."""
    res = {}
    try:
        with open("/proc/stat") as fh:
            for line in fh:
                if not line.startswith("cpu") or line.startswith("cpu "):
                    continue
                f = line.split()
                cpu = int(f[0][3:])
                vals = [int(x) for x in f[1:]]
                idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
                res[cpu] = (sum(vals) - idle, sum(vals))
    except (OSError, ValueError, IndexError):
        pass
    return res


def cpu_busy(cpus, interval=0.5):
    """Return {cpu: busy fraction} measured over INTERVAL seconds."""
    a = read_proc_stat()
    if not a:
        return {}
    time.sleep(interval)
    b = read_proc_stat()
    res = {}
    for cpu in cpus:
        if cpu in a and cpu in b:
            db = b[cpu][0] - a[cpu][0]
            dt = b[cpu][1] - a[cpu][1]
            res[cpu] = db / dt if dt > 0 else 0.0
    return res


def select_cores(n, explicit=None):
    """Return a list of N cpu ids to pin runs to, or None if not possible."""
    if explicit is not None:
        if len(explicit) < n:
            die(f"cores lists {len(explicit)} cores but {n} are needed for parallel = {n}")
        return explicit[:n]

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

    # prefer idle cores
    busy = cpu_busy(cores)
    cores.sort(key=lambda c: (round(busy.get(c, 0.0), 2), c))
    chosen = cores[:n]
    loaded = [c for c in chosen if busy.get(c, 0.0) > 0.05]
    if loaded:
        warn("selected cores are not idle: "
             + ", ".join(f"cpu{c} {busy[c] * 100:.0f}%" for c in loaded)
             + " -- consider setting isolate or cores in the configuration")
    return chosen


def current_affinity():
    try:
        return sorted(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return None


###############################################################################
# Running
###############################################################################

class Runner:
    def __init__(self, cfg, cores):
        """CORES is the list of cpu ids to pin the runs to (one per parallel
        worker), or None to run unpinned."""
        self.cfg = cfg
        self.cores = queue.Queue()
        self.pin = cores is not None
        self.stop = threading.Event()
        self.active = set()
        self.active_lock = threading.Lock()

        if self.pin:
            for c in cores[:cfg.parallel]:
                self.cores.put(c)
        else:
            for i in range(cfg.parallel):
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

        cmd = build_cmd(self.cfg, test)
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
        if self.cfg.max_time > 0:
            timer = threading.Timer(self.cfg.max_time, kill)
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
        res.update(parse_plans(self.cfg, run_dir))
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


def parse_plans(cfg, run_dir):
    pattern = os.path.join(run_dir, glob.escape(cfg.plan_file) + "*")
    files = sorted(os.path.basename(f) for f in glob.glob(pattern))
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

def results_root(cfg):
    return os.path.join(TESTS_DIR, cfg.results_dir)


def config_dir(cfg, name):
    return os.path.join(results_root(cfg), name)


def meta_path(cfg, name):
    return os.path.join(config_dir(cfg, name), "meta.json")


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


def load_results(cfg, name):
    """Load meta.json and all result.json files of a configuration."""
    meta = load_json(meta_path(cfg, name))
    if meta is None:
        return None
    results = {}
    for test_id in meta.get("tests", {}):
        res = load_json(os.path.join(config_dir(cfg, name), test_id, "result.json"))
        if res is not None:
            results[test_id] = res
    return {"name": name, "meta": meta, "results": results}


def list_configs(cfg):
    base = results_root(cfg)
    if not os.path.isdir(base):
        return []
    return [n for n in sorted(os.listdir(base))
            if os.path.isfile(meta_path(cfg, n))]


def select_tests_to_run(cfg, name, tests, tool_sha, fingerprint, force):
    """Return (tests_to_run, reason, old_meta)."""
    cdir = config_dir(cfg, name)
    old = load_json(meta_path(cfg, name))
    if force:
        return tests, "--force given", old
    if old is None:
        return tests, "no previous results", None
    if old.get("tool_sha256") != tool_sha:
        return tests, "bin/pddl-tool changed", old
    if old.get("tool_args") != fingerprint:
        return tests, "tool arguments or limits changed", old
    if old.get("runs") != cfg.runs:
        return tests, f"number of runs changed ({old.get('runs')} -> {cfg.runs})", old

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
        if res is None or res.get("num_runs", 0) < cfg.runs:
            to_run.append(t)
    return to_run, "PDDL files changed or results missing", old


def run_configuration(cfg, name, tests, cores, force):
    cdir = config_dir(cfg, name)
    if not os.path.isfile(TOOL_BIN):
        die(f"binary not found: {TOOL_BIN}")
    tool_sha = sha256_file(TOOL_BIN)
    fingerprint = tool_args_fingerprint(cfg)

    to_run, reason, old = select_tests_to_run(cfg, name, tests, tool_sha,
                                              fingerprint, force)
    os.makedirs(cdir, exist_ok=True)

    ids = {t["id"] for t in tests}
    dropped = [k for k in (old or {}).get("tests", {}) if k not in ids]
    for k in dropped:
        log(f"note: {k} is no longer in pddl-files; {os.path.join(cdir, k)} is left on disk")

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
            "runs": cfg.runs,
            "tests": entries,
        }

    if not to_run:
        log(f"{name}: all {len(tests)} tests are up to date, nothing to run")
        save_json(meta_path(cfg, name), make_meta())
        return True

    if len(to_run) == len(tests):
        log(f"{name}: running all {len(tests)} tests ({reason})")
    else:
        log(f"{name}: running {len(to_run)} of {len(tests)} tests ({reason})")
    log(f"  tool: {TOOL_BIN} ({git_describe()})")
    log(f"  args: {shlex.join(build_cmd(cfg, to_run[0])[1:])}")
    log(f"  runs: {cfg.runs}, parallel: {cfg.parallel}, "
        f"max-time: {cfg.max_time}s, max-mem: {cfg.max_mem}MB, "
        f"cores: {format_cores(cores) if cores else 'not pinned'}")

    # Remove stale results of the tests that are re-run so that an
    # interrupted run never leaves a mix of old and new runs behind.
    for t in to_run:
        tdir = os.path.join(cdir, t["id"])
        if os.path.exists(tdir):
            shutil.rmtree(tdir)
        os.makedirs(tdir)

    runner = Runner(cfg, cores)

    jobs = [(t, k) for t in to_run for k in range(1, cfg.runs + 1)]
    total = len(jobs)
    id_w = max(len(t["id"]) for t in to_run)
    done_runs = {t["id"]: {} for t in to_run}
    done_lock = threading.Lock()
    ok = True

    def job(item):
        t, k = item
        run_dir = os.path.join(cdir, t["id"], f"run-{k}")
        return t, k, runner.run(t, run_dir)

    executor = ThreadPoolExecutor(max_workers=cfg.parallel)
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
                if len(done_runs[t["id"]]) == cfg.runs:
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
    save_json(meta_path(cfg, name), make_meta())
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


def print_summary(cfg, names):
    configs = [c for c in (load_results(cfg, n) for n in names) if c is not None]
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
# Core isolation (isolate = true)
###############################################################################

def sudo_run(cmd, check=True):
    """Run CMD via sudo; return the exit code (die on failure if CHECK)."""
    full = ["sudo"] + cmd
    try:
        ret = subprocess.run(full).returncode
    except OSError as exc:
        die(f"cannot run {shlex.join(full)}: {exc}")
    if check and ret != 0:
        die(f"command failed (exit code {ret}): {shlex.join(full)}")
    return ret


def isolate_check_prerequisites():
    for tool in ("sudo", "systemctl", "systemd-run", "runuser"):
        if shutil.which(tool) is None:
            die(f"isolate = true requires {tool}, which was not found")
    controllers = ""
    try:
        with open("/sys/fs/cgroup/cgroup.controllers") as fh:
            controllers = fh.read().split()
    except OSError:
        die("isolate = true requires the cgroup v2 unified hierarchy mounted "
            "at /sys/fs/cgroup")
    if "cpuset" not in controllers:
        die("isolate = true requires the cpuset cgroup controller")


def run_isolated(name, cores, force):
    """Restrict all other processes to the cores not in CORES, re-execute
    this script in a scope restricted to CORES, then lift the restrictions.
    Returns the exit code of the re-executed script."""
    isolate_check_prerequisites()
    all_cpus = list(range(os.cpu_count()))
    others = [c for c in all_cpus if c not in cores]
    if not others:
        die("isolate = true needs at least one core left for the rest of the system")

    log(f"isolating cores {format_cores(cores)}: restricting "
        f"{', '.join(ISOLATE_UNITS)} to cores {format_cores(others)} (sudo)")
    env = {k: v for k, v in os.environ.items()
           if k in ("HOME", "PATH", "USER", "LOGNAME", "LANG", "LC_ALL", "TERM",
                    "PYTHONPATH")}
    env[ISOLATED_ENV] = "1"
    env[CORES_ENV] = format_cores(cores)
    user = os.environ.get("USER") or str(os.getuid())
    cmd = ["systemd-run", "--quiet", "--scope", f"--slice={ISOLATE_SLICE}",
           f"--property=AllowedCPUs={format_cores(cores)}",
           "--", "runuser", "-u", user, "--",
           "env"] + [f"{k}={v}" for k, v in env.items()] + \
          [sys.executable, os.path.abspath(__file__)] + \
          (["--force"] if force else []) + [name]

    restricted = []
    ret = 1
    try:
        for unit in ISOLATE_UNITS:
            # a missing unit (e.g. machine.slice) only yields a drop-in that
            # takes effect if the unit ever starts, so failures are fatal
            sudo_run(["systemctl", "set-property", "--runtime", unit,
                      f"AllowedCPUs={format_cores(others)}"])
            restricted.append(unit)
        proc = subprocess.Popen(["sudo"] + cmd)
        while True:
            try:
                ret = proc.wait()
                break
            except KeyboardInterrupt:
                # the child got the SIGINT as well and is shutting down; sudo
                # runs as root so we cannot kill it -- just keep waiting
                continue
    finally:
        if restricted:
            log("removing the core restrictions (sudo)")
            for unit in restricted:
                sudo_run(["systemctl", "set-property", "--runtime", unit,
                          "AllowedCPUs="], check=False)
    return ret


def verify_isolation(cores):
    """Inside the isolated scope: check that this process is restricted to
    CORES and that the other units are kept off them."""
    aff = current_affinity()
    if aff is None or set(aff) != set(cores):
        warn(f"expected affinity {format_cores(cores)}, but got "
             f"{format_cores(aff) if aff else '?'}")
    for unit in ISOLATE_UNITS:
        path = os.path.join("/sys/fs/cgroup", unit, "cpuset.cpus.effective")
        try:
            with open(path) as fh:
                eff = parse_cores(fh.read().strip() or "")
        except (OSError, ValueError):
            continue
        overlap = sorted(set(eff) & set(cores))
        if overlap:
            warn(f"{unit} still allowed on cores {format_cores(overlap)}")
    log(f"isolated: running on cores {format_cores(cores)}")


###############################################################################

def main():
    args = parse_args()
    script = os.path.relpath(os.path.abspath(__file__))

    if not os.path.isfile(CONFIG_PATH):
        write_default_config(CONFIG_PATH)
        log(f"Generated the configuration file {CONFIG_PATH} with default settings.")
        log("Nothing was run. Inspect the file, change it to your liking, and run")
        log(f"    {script} NAME")
        log("where NAME is the name of the configuration to measure (e.g. base).")
        sys.exit(1)

    if args.name is None:
        log(f"usage: {script} [-f] [-s] NAME")
        log("")
        log("The configuration name NAME is required (e.g. base, v1, ...); all other")
        log(f"settings are read from {CONFIG_PATH}.")
        sys.exit(1)

    cfg = load_config(CONFIG_PATH)
    name = args.name
    ok = True
    if not args.summary_only:
        tests = resolve_tests(cfg)
        if not tests:
            die(f"{CONFIG_PATH}: pddl-files is empty")

        cores = None
        if cfg.pin:
            if shutil.which("taskset") is None:
                warn("taskset not found, runs are not pinned")
            else:
                explicit = cfg.cores
                if os.environ.get(CORES_ENV):
                    # set by run_isolated() for the re-executed script
                    try:
                        explicit = parse_cores(os.environ[CORES_ENV])
                    except ValueError as exc:
                        die(f"{CORES_ENV}: {exc}")
                cores = select_cores(cfg.parallel, explicit)
                if cores is None:
                    warn("cannot determine cpu affinity, runs are not pinned")
        if cfg.isolate:
            if cores is None:
                die("isolate = true needs pinning, which is not available")
            if os.environ.get(ISOLATED_ENV) != "1":
                sys.exit(run_isolated(name, cores, args.force))
            verify_isolation(cores)

        ok = run_configuration(cfg, name, tests, cores, args.force)

    if cfg.compare:
        names = [n for n in cfg.compare if n != name]
        for n in names:
            if not os.path.isfile(meta_path(cfg, n)):
                die(f"configuration not found: {n}")
    else:
        names = [n for n in list_configs(cfg) if n != name]
    # oldest first, the current configuration last
    names.sort(key=lambda n: (load_json(meta_path(cfg, n)) or {}).get("created", ""))
    if os.path.isfile(meta_path(cfg, name)):
        names.append(name)
    print_summary(cfg, names)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
