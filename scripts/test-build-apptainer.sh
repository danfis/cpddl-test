#!/bin/bash
# This script is supposed to be run from the top cpddl directory

set -e

function run_test(){
    echo "=============================================================="
    echo "===== $@ ====="
    echo "=============================================================="

    local apptainer_args="$@"
    local have_cudd=no
    local have_bliss=no
    local have_lp=no
    local have_minizinc=no
    local lp=
    local cp=
    echo "$apptainer_args" | grep -q -- '--no-cudd' || have_cudd=yes
    echo "$apptainer_args" | grep -q -- '--no-bliss' || have_bliss=yes
    echo "$apptainer_args" | grep -q -- '--highs' && have_lp=yes
    echo "$apptainer_args" | grep -q -- '--cplex' && have_lp=yes
    echo "$apptainer_args" | grep -q -- '--coin-or' && have_lp=yes
    echo "$apptainer_args" | grep -q -- '--minizinc' && have_minizinc=yes

    [ "$have_lp" = "yes" ] && lp="--mg fam"
    [ "$have_minizinc" = "yes" ] && cp="--P-endo fdr"

    set -ex
    python3 scripts/build-apptainer.py --output apptainer.img "$@"
    [ ! -f apptainer.img ] && exit -1

    ./apptainer.img --lplan astar --lplan-h blind --lplan-o test_output.plan \
            t/pddl-data/ipc-1998/gripper/prob01 
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "11" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img --lplan astar --lplan-h blind --lplan-o test_output.plan \
            t/pddl-data/various/miconic-fulladl/f2-4
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "6" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img --pddl-ce \
            --lplan astar --lplan-h blind --lplan-o test_output.plan \
            t/pddl-data/various/miconic-fulladl/f2-4
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "6" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img --ce \
            --gplan astar --gplan-h hmax --gplan-o test_output.plan \
            t/pddl-data/various/miconic-fulladl/f2-4
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "6" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img $lp --h2 \
            --gplan astar --gplan-h blind --gplan-o test_output.plan \
            t/pddl-data/ipc-1998/gripper/prob01 
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "11" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img --h2 --gplan astar --gplan-h lmc --gplan-o test_output.plan \
            t/pddl-data/ipc-2014/seq-opt/visitall/p-1-5
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "24" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img --lplan astar --lplan-h hmax --lplan-o test_output.plan \
            --pddl-compile-in-lmg \
            t/pddl-data/ipc-2011/seq-opt/scanalyzer/p01
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "13" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    ./apptainer.img $cp --P-h3fw --gplan astar --gplan-h lmc --gplan-o test_output.plan \
            --pddl-compile-in-lmg \
            t/pddl-data/ipc-2011/seq-opt/scanalyzer/p01
    cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
    [ "$cost" != "13" ] && echo "Test FAILED" && exit -1
    rm -f test_output.plan

    if [ "$have_cudd" = "yes" ]; then
        ./apptainer.img $cp --P-h3fw --symba bi --symba-out test_output.plan \
                --pddl-compile-in-lmg \
                t/pddl-data/ipc-2011/seq-opt/scanalyzer/p01
        cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
        [ "$cost" != "13" ] && echo "Test FAILED" && exit -1
        rm -f test_output.plan
    fi

    if [ "${have_cudd}${have_lp}" = "yesyes" ]; then
        ./apptainer.img $cp --P-h3fw --symba bi \
                --symba-fw-pot --symba-fw-pot-cfg A+I \
                --symba-out test_output.plan \
                --pddl-compile-in-lmg \
                t/pddl-data/ipc-2011/seq-opt/scanalyzer/p01
        cost=$(cat test_output.plan | grep ';; Cost:' | grep -o '[0-9]\+$')
        [ "$cost" != "13" ] && echo "Test FAILED" && exit -1
        rm -f test_output.plan
    fi
    set +x

    echo ""
    echo ""
    rm -f apptainer.img
}

if ! echo "$@" | grep -q -- --cplex; then
    run_test $@ --werror alpine
    run_test $@ --no-cudd --no-bliss --highs --werror alpine
    run_test $@ --no-cudd --no-bliss --highs --clingo --werror alpine
    run_test $@ --no-cudd --no-bliss --werror alpine
    run_test $@ --no-cudd --no-bliss --werror --clingo alpine
    run_test $@ --highs --werror alpine

    run_test "$@" --werror --clang alpine
    run_test "$@" --no-cudd --no-bliss --werror --clang alpine
fi

run_test $@ --werror photon
run_test $@ --no-cudd --no-bliss --werror photon
run_test $@ --highs --werror photon
run_test $@ --minizinc --highs --werror photon
run_test $@ --minizinc --highs --clingo --werror photon

run_test $@ --werror debian-bullseye
run_test $@ --werror --clang debian-bullseye
run_test $@ --werror --clang-ver 16 debian-bullseye
run_test $@ --highs --werror debian-bullseye
run_test $@ --highs --minizinc --werror debian-bullseye
run_test $@ --highs --minizinc --clingo --werror debian-bullseye
run_test $@ --coin-or --werror debian-bullseye

run_test $@ --werror debian-buster
run_test $@ --werror ubuntu-kinetic
run_test $@ --highs --werror ubuntu-kinetic
run_test $@ --coin-or --werror ubuntu-kinetic
run_test $@ --werror ubuntu-jammy
run_test $@ --highs --werror ubuntu-jammy
run_test $@ --werror ubuntu-focal
run_test $@ --werror ubuntu-bionic
run_test $@ --werror fedora
run_test $@ --highs --werror fedora
run_test $@ --highs --clingo --werror fedora

run_test "$@" --werror debian-buster
run_test "$@" --werror ubuntu-jammy
run_test "$@" --highs --werror ubuntu-jammy
run_test "$@" --highs --werror ubuntu-mantic
run_test "$@" --werror ubuntu-focal
run_test "$@" --werror ubuntu-bionic
run_test "$@" --werror fedora
run_test "$@" --highs --werror fedora
run_test "$@" --highs --werror --minizinc fedora

run_test "$@" --werror gcc-11
run_test "$@" --highs --werror gcc-11
run_test "$@" --werror gcc-12
run_test "$@" --highs --werror gcc-12
