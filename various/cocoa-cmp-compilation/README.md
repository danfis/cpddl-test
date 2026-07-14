# Cocoa compilation vs. reference implementation: compiled semantics

Compares the compilations themselves -- `pddlStripsCompileAwayCondEffCocoa()`
against the reference implementation of the Cocoa compilation
(https://gitlab.com/EdmondDantes/cocoa2.0) -- without a planner in the
loop.

For every task, `run.sh` exports three ground tasks with
`bin/pddl-tool strips-as-py`:

- the original grounded task with conditional effects preserved,
- the task compiled with `--ce cocoa`,
- the reference's compiled PDDL, grounded by cpddl,

and `compare.py` then runs seeded random walks on the original task.
Every step applies one original operator (a) directly, using the
ground-truth conditional-effect semantics (all conditions evaluated on
the pre-state, add wins on conflicts), and (b) through the corresponding
compiled operator chain in each compilation: the start operator is found
by name, then chain operators are applied until the bookkeeping facts
(`cocoa-free`/`cocoa-noset` in ours, `pause_condeff` in the reference)
signal that the simulation is closed. After every step the compiled
states are projected onto the original atoms and compared with the
ground-truth successor. A structural summary (operator/fact counts,
auxiliary operators, twin facts) is printed per task.

Tasks: wumpus, flip, elevators-adl (s2-0, s3-0), citycar, nurikabe, and a
small synthetic domain (`cyc-*.pddl`) with a cyclic effect interference
graph, exercising the setup/run (twin-fact) part of the compilation.

## Usage

Compile the project first (the test needs `bin/pddl-tool`), then:

    ./run.sh

The script first fetches the reference implementation into `./cocoa-ref`
and prepares a python virtual environment with its dependencies, so the
reference project does not need to be stored in this repository; override
the location with the `COCOA_REF` environment variable. A completed
preparation is marked with a stamp file and skipped on subsequent runs;
an interrupted preparation is repaired automatically, and deleting
`./cocoa-ref` forces a fresh one. The preparation requires network
access and python3-venv. All artifacts are written to `./out`.

## Exit code and known deviation of the reference implementation

The exit code reflects only our compilation: every simulated state must
match the ground truth. Divergence of the reference is reported in the
summary only; on `various/flip/p01` it is expected -- the reference's
interference graph misses edges through negated effect conditions, its
feedback set twins only 5 of the 9 required atoms, and the compiled
operator chains evaluate the remaining negated conditions on an already
modified state.
