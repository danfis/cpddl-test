#!/bin/bash
#
# Set up a fresh cpddl-devel checkout for running the full test matrix on
# the cluster via Slurm:
#   1. clone git@github.com:danfis/cpddl-devel into X and switch to branch Y
#   2. check out the matching tests/ repo (scripts/checkout-tests.sh)
#   3. create a Python venv in X/pyenv and install tomli into it
#   4. write X/run.sh -- the Slurm batch job that builds cpddl and runs
#      "make check" over all tasks/tests
#   5. write X/submit.sh -- pulls the latest changes in X and X/tests and
#      submits run.sh with sbatch
#
# Usage: setup-cluster-run.sh X Y
#   X - directory to clone cpddl-devel into (must not already exist)
#   Y - branch to check out in X (and in X/tests)

set -euo pipefail

CURRENT_STEP="startup"
step() {
    CURRENT_STEP="$*"
    echo
    echo "=== $CURRENT_STEP ==="
}
on_error() {
    echo
    echo "FAILED at step: $CURRENT_STEP" >&2
    exit 1
}
trap on_error ERR

if [ $# -ne 2 ]; then
    echo "Usage: $0 X Y" >&2
    echo "  X - directory to clone git@github.com:danfis/cpddl-devel into" >&2
    echo "  Y - branch to check out in X (and in X/tests)" >&2
    exit 1
fi

X="$1"
Y="$2"

if [ -e "$X" ]; then
    echo "Error: '$X' already exists -- refusing to overwrite it." >&2
    exit 1
fi

step "Cloning git@github.com:danfis/cpddl-devel into '$X'"
git clone git@github.com:danfis/cpddl-devel "$X"

cd "$X"
ABS_X=$(pwd)

step "Switching to branch '$Y'"
git switch "$Y"

step "Checking out tests/ (scripts/checkout-tests.sh)"
./scripts/checkout-tests.sh

step "Creating Python venv in $ABS_X/pyenv and installing tomli"
python3 -m venv pyenv
"$ABS_X/pyenv/bin/pip" install tomli

step "Writing run.sh"
JOB_NAME=$(basename "$ABS_X")
cat >run.sh <<EOF
#!/bin/bash
#SBATCH -J ${JOB_NAME}
#SBATCH -N 1
#SBATCH -t 0-5:0:0
#SBATCH -p rome
#SBATCH --exclusive
#SBATCH -o run.out
#SBATCH -e run.err

set -ex

echo "WERROR = yes" >Makefile.config
echo "USE_SQLITE = yes" >>Makefile.config
echo "USE_CUDD = yes" >>Makefile.config
echo "USE_BLISS = yes" >>Makefile.config
echo "IBM_CPLEX_ROOT = /nfs/home/cs.aau.dk/zj37xu/opt/cplex/v22.1.1" >>Makefile.config
echo "DYNET_ROOT = /nfs/home/cs.aau.dk/zj37xu/opt/dynet/2.1.2" >>Makefile.config
echo "CLINGO_ROOT = /nfs/home/cs.aau.dk/zj37xu/opt/clingo/5.8.0" >>Makefile.config
echo "CADICAL_ROOT = /nfs/home/cs.aau.dk/zj37xu/opt/cadical/3.0.0" >>Makefile.config

make mrproper
make -j90 bin
make -C tests PYTHON=${ABS_X}/pyenv/bin/python

make check T="-A -a -p90 -x -m 300"
EOF
chmod +x run.sh

step "Writing submit.sh"
cat >submit.sh <<'EOF'
#!/bin/bash
#
# Pull the latest changes in the current checkout and in tests/, then
# submit run.sh via sbatch.

set -euo pipefail

CURRENT_STEP="startup"
step() {
    CURRENT_STEP="$*"
    echo
    echo "=== $CURRENT_STEP ==="
}
on_error() {
    echo
    echo "FAILED at step: $CURRENT_STEP" >&2
    exit 1
}
trap on_error ERR

step "Pulling latest changes in $(pwd)"
git pull

step "Pulling latest changes in $(pwd)/tests"
git -C tests pull

step "Submitting run.sh via sbatch"
sbatch run.sh
EOF
chmod +x submit.sh

step "Done"
echo "Set up '$ABS_X' on branch '$Y'."
echo "To submit the job: cd '$ABS_X' && ./submit.sh"
