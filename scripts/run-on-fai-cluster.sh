#!/bin/bash
# Clones cpddl and the experiment-runner (exp) repos at the given commit,
# builds both, and submits planning-experiment jobs for various
# grounder/heuristic/search combinations to the FAI cluster.

set -e

COMMIT="$1"
[ "$COMMIT" = "" ] && echo "No commit provided!" && exit -1

TOP=$(pwd)

git clone git@gitlab.com:danfis/exp exp
cd exp
git submodule update --init
make -j8
cd ..
EXP="${TOP}/exp/exp"

git clone git@gitlab.com:danfis/cpddl-devel cpddl
cd cpddl
git switch -c EXP $COMMIT
git submodule update --init --recursive
echo "IBM_CPLEX_ROOT = /mnt/data_server/common/opt/cplex/v22.1.0" >Makefile.config
make -j8 third-party
make -j8 bin
make -j8 -C t
cd ..

PDDL_DATA=${TOP}/cpddl/t/pddl-data
PDDL=${TOP}/cpddl/bin/pddl
VAL=${TOP}/cpddl/t/val/validate

[ ! -f "$EXP" ] && echo "Cannot find exp program" && exit -1
[ ! -d "$PDDL_DATA" ] && echo "Cannot find pddl-data directory" && exit -1
[ ! -f "$PDDL" ] && echo "Cannot find pddl program" && exit -1
[ ! -f "$VAL" ] && echo "Cannot find validate program" && exit -1

MAX_MEM=3072
MAX_TIME=120
CLUSTER=fai0
BENCH=${PDDL_DATA}/bench/all

function gen(){
    local name="$1"
    cat >run-${name}.sh <<EOF
#!/bin/bash
$PDDL $2 domain.pddl problem.pddl

if [ -f plan.out ] && [ -f $VAL ]; then
    $VAL -v domain.pddl problem.pddl plan.out >validate.out 2>validate.err
    echo $? >validate.ret

#    optimal_cost=\$(cat task.prop | grep optimal_cost | cut -f3 -d' ')
#    cost=\$(cat validate.out ...)
#    if [ "$optimal_cost" -ge 0 ]; then
#    fi
fi
EOF
    $EXP --no-systemd --max-time ${MAX_TIME} --max-mem ${MAX_MEM} -B ${BENCH} -t ${CLUSTER} gen $name run-${name}.sh
}

gen gplan-lazy-ff "-G dl --gplan lazy --gplan-h ff --gplan-out plan.out"
gen gplan-lazy-ff-sql "-G sql --gplan lazy --gplan-h ff --gplan-out plan.out"
gen gplan-lazy-add "-G dl --gplan lazy --gplan-h add --gplan-out plan.out"
gen gplan-lazy-add-sql "-G sql --gplan lazy --gplan-h add --gplan-out plan.out"
gen lplan-lazy-add "--lplan lazy --lplan-h hadd --lplan-out plan.out"
gen lplan-lazy-add-sql "--lplan lazy --lplan-h hadd --lplan-succ-gen sql --lplan-out plan.out"

gen gplan-astar-blind "--gplan astar --gplan-out plan.out"
gen lplan-astar-blind "--lplan astar --lplan-out plan.out"
gen gplan-astar-max "--gplan astar --gplan-h max --gplan-out plan.out"
gen lplan-astar-max "--lplan astar --lplan-h hmax --lplan-out plan.out"
gen gplan-astar-lmc "--gplan astar --gplan-h lmc --gplan-out plan.out"

gen symba-fw-blind "--fdr-ess --h2 --symba fw --symba-out plan.out"
gen symba-fw-pot "--fdr-ess --h2 --symba fw --symba-fw-pot --symba-fw-pot-cfg A+I --symba-out plan.out"
gen symba-bw-blind "--fdr-ess --h2 --symba bw --symba-out plan.out"
gen symba-bw-pot "--fdr-ess --h2 --symba bw --symba-bw-pot --symba-bw-pot-cfg I --symba-out plan.out"
gen symba-bi-blind "--fdr-ess --h2 --symba bi --symba-out plan.out"
gen symba-bi-fwpot "--fdr-ess --h2 --symba bi --symba-fw-pot --symba-fw-pot-cfg A+I --symba-out plan.out"
