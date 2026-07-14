#!/bin/bash
# Semantic and structural comparison of pddlStripsCompileAwayCondEffCocoa()
# against the reference implementation of the Cocoa compilation
# (https://gitlab.com/EdmondDantes/cocoa2.0), without a planner in the
# loop.
#
# For every task three ground tasks are exported with
# "bin/pddl-tool strips-as-py":
#   *-orig.py -- the original grounded task, conditional effects preserved
#   *-ours.py -- the task compiled with --ce cocoa
#   *-ref.py  -- the reference's compiled PDDL, grounded by cpddl
# and compare.py then simulates seeded random walks on the original task,
# replaying every operator application through the compiled operator
# chains of both compilations and comparing the projected states with the
# ground-truth conditional-effect semantics. See compare.py for details.
#
# The reference implementation is cloned and set up in ./cocoa-ref on the
# first run (override the location with the COCOA_REF environment
# variable). All artifacts are written to ./out.
#
# The exit code reflects only our compilation (every simulated state must
# match the ground truth). Divergence of the reference is reported in the
# summary; on various/flip/p01 it is expected, because the reference
# implementation is unsound there (its interference graph misses edges
# through negated effect conditions).

set -u

DIR=$(cd "$(dirname "$0")" && pwd)
TESTS=$(cd "$DIR/../.." && pwd)
ROOT=$(cd "$TESTS/.." && pwd)
TOOL=$ROOT/bin/pddl-tool
REF=${COCOA_REF:-$DIR/cocoa-ref}
OUT=$DIR/out
REF_TIMEOUT=600

if [ ! -x "$TOOL" ]; then
    echo "Cannot find $TOOL -- compile the project first." >&2
    exit 2
fi

# Fetch the reference implementation and prepare a python virtual
# environment with all its dependencies, so the reference project does
# not need to be stored in this repository. The stamp file is created
# only after every step succeeded, so an interrupted preparation is
# repaired on the next run, and a completed preparation is skipped.
ensure_ref() {
    if [ -f "$REF/.prepared" ]; then
        return
    fi
    echo "Preparing the reference implementation in $REF ..."
    mkdir -p "$REF"
    if [ ! -f "$REF/cocoa2.0/cocoa2.0.py" ]; then
        rm -rf "$REF/cocoa2.0"
        git clone --depth 1 https://gitlab.com/EdmondDantes/cocoa2.0 \
                "$REF/cocoa2.0" \
            || { echo "Cloning the reference implementation failed." >&2;
                 exit 2; }
    fi
    if [ ! -x "$REF/venv/bin/python" ]; then
        rm -rf "$REF/venv"
        python3 -m venv "$REF/venv" \
            || { echo "Creating the python virtual environment failed." >&2;
                 exit 2; }
    fi
    "$REF/venv/bin/pip" install -q bidict sympy networkx matplotlib click \
        || { echo "Installing the python dependencies failed." >&2; exit 2; }
    touch "$REF/.prepared"
    echo "Reference implementation prepared."
}
ensure_ref
PY=$REF/venv/bin/python
COCOA=$REF/cocoa2.0/cocoa2.0.py

mkdir -p "$OUT"
SPECS=()

# prep <name> <domain> <problem>
prep() {
    local name=$1 dom=$2 prob=$3
    local dir=$OUT/$name
    mkdir -p $dir

    # Reference compilation; it names its outputs compiled_<input-basename>
    (cd "$REF/cocoa2.0" && timeout $REF_TIMEOUT $PY $COCOA \
        --domain_path $dom --problem_path $prob --output_path $dir/ \
        --translation COCOA --semantics DELbeforeADD) \
        >$dir/ref-compile.log 2>&1 \
        || { echo "$name: reference compilation failed"; exit 1; }
    local cdom cprob
    cdom=$dir/compiled_$(basename $dom)
    cprob=$dir/compiled_$(basename $prob)

    $TOOL strips-as-py $dir/orig.py $dom $prob >$dir/orig.log 2>&1 \
        || { echo "$name: export of the original task failed"; exit 1; }
    $TOOL strips-as-py --ce cocoa $dir/ours.py $dom $prob \
        >$dir/ours.log 2>&1 \
        || { echo "$name: export of the --ce cocoa task failed"; exit 1; }
    $TOOL strips-as-py $dir/ref.py $cdom $cprob >$dir/ref.log 2>&1 \
        || { echo "$name: export of the reference task failed"; exit 1; }

    SPECS+=("$name,$dir/orig.py,$dir/ours.py,$dir/ref.py")
}

prep wumpus-p01 $TESTS/pddl/various/wumpus/domain.pddl \
                $TESTS/pddl/various/wumpus/p01.pddl
prep flip-p01   $TESTS/pddl/various/flip/domain.pddl \
                $TESTS/pddl/various/flip/p01.pddl
prep cyc-synth  $DIR/cyc-domain.pddl $DIR/cyc-problem.pddl
prep elevators-s2-0 $TESTS/pddl/ipc-2000/elevators-adl/domain.pddl \
                    $TESTS/pddl/ipc-2000/elevators-adl/s2-0.pddl
prep elevators-s3-0 $TESTS/pddl/ipc-2000/elevators-adl/domain.pddl \
                    $TESTS/pddl/ipc-2000/elevators-adl/s3-0.pddl
prep citycar-p2-2-2-1-2 $TESTS/pddl/ipc-2014/seq-opt/citycar/domain.pddl \
                        $TESTS/pddl/ipc-2014/seq-opt/citycar/p2-2-2-1-2.pddl
prep nurikabe-p01 $TESTS/pddl/ipc-2018/seq-opt/nurikabe/domain.pddl \
                  $TESTS/pddl/ipc-2018/seq-opt/nurikabe/p01.pddl

python3 $DIR/compare.py "${SPECS[@]}"
