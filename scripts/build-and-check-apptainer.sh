#!/bin/bash
# This script is supposed to be run from the top cpddl directory

set -e

function run_test(){
    echo "=============================================================="
    echo "===== $@ ====="
    echo "=============================================================="

    set -ex
    python3 scripts/build-apptainer.py --output apptainer.img "$@"
    set +x

    [ ! -f apptainer.img ] && exit -1

    rm -f apptainer.img
    echo ""
    echo ""
}

run_test "$@" --run-all-tests --werror --no-cudd --no-bliss debian-bookworm
run_test "$@" --run-all-tests --werror --no-cudd debian-bookworm
run_test "$@" --run-all-tests --werror debian-bookworm
run_test "$@" --run-all-tests --werror --coin-or debian-bookworm
run_test "$@" --run-all-tests --werror --highs --clingo debian-bookworm
run_test "$@" --run-all-tests --werror --minizinc debian-bookworm
run_test "$@" --run-all-tests --werror --cplex /opt/cplex/cplex_studio2211.linux_x86_64.bin debian-bookworm

run_test "$@" --run-all-tests --werror alpine
run_test "$@" --run-all-tests --werror photon
run_test "$@" --run-all-tests --werror debian-bullseye
run_test "$@" --run-all-tests --werror --cplex /opt/cplex/cplex_studio2211.linux_x86_64.bin ubuntu-mantic
run_test "$@" --run-all-tests --werror --cplex /opt/cplex/cplex_studio2211.linux_x86_64.bin ubuntu-jammy
run_test "$@" --run-all-tests --werror --cplex /opt/cplex/cplex_studio2211.linux_x86_64.bin fedora
