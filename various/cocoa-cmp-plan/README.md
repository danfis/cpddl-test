# Cocoa compilation vs. reference implementation: optimal planning

Compares `pddlStripsCompileAwayCondEffCocoa()` against the reference
implementation of the Cocoa compilation by the paper's authors
(https://gitlab.com/EdmondDantes/cocoa2.0, see Gerevini, Percassi, Scala:
An Effective Polynomial Technique for Compiling Conditional Effects Away,
AAAI 2024) on the level of optimal planning.

For every task the optimal plan cost (A*/LM-cut via `bin/pddl-tool gplan`)
is computed for

- the task compiled with `--ce` (exponential compilation, baseline),
- the task compiled with `--ce-cocoa`,
- the output of the reference compiler (`cocoa2.0.py --translation COCOA`),

and compared with each other and with the optimal cost recorded in the
task's `.plan` file (or the `.unsolvable` marker). Tasks: wumpus, flip,
elevators-adl (s2-0, s3-0), citycar, nurikabe, and a small synthetic
domain (`cyc-*.pddl`) whose single conditional-effect action has a cyclic
effect interference graph, exercising the setup/run (twin-fact) part of
the compilation.

## Usage

Compile the project first (the test needs `bin/pddl-tool`), then:

    ./run.sh

The script first fetches the reference implementation into `../cocoa-ref`
and prepares a python virtual environment with its dependencies (bidict,
sympy, networkx, matplotlib, click), so the reference project does not
need to be stored in this repository; override the location with the
`COCOA_REF` environment variable. A completed preparation is marked with
a stamp file and skipped on subsequent runs; an interrupted preparation
is repaired automatically, and deleting `../cocoa-ref` forces a fresh
one. The preparation requires network access and python3-venv. All
artifacts (compiled tasks, plans, logs) are written to `./out`.

## Known deviation of the reference implementation

On `various/flip/p01` the reference implementation is unsound: its effect
interference graph misses edges that run through negated effect
conditions, so its feedback set does not break all cycles and the
evaluation order inside the compiled operator chains is wrong. The
compiled task then admits plans that are invalid in the original task
(flip p01 is unsolvable, the reference's compilation "solves" it with
cost 3; its own NEBEL translation correctly reports unsolvability). This
specific deviation is reported as `REF-BUG` and does not fail the test;
any other disagreement makes the test exit with a non-zero status.
