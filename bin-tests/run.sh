#!/bin/bash

set -x

cd $(dirname $0)

select="head -20"

if [ "$1" = "--all" ]; then
    select=cat
    shift
fi

PDDL=../../bin/pddl
[ ! -f $PDDL ] && echo "Cannot find binary pddl" && exit -1

exit_status=0

cat tasks-gplan-lazy-ff.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G dl --gplan lazy --gplan-h ff \
        || exit_status=1

cat tasks-gplan-lazy-ff-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G sql --gplan lazy --gplan-h ff \
        || exit_status=1

cat tasks-gplan-lazy-add.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G dl --gplan lazy --gplan-h add \
        || exit_status=1

cat tasks-gplan-lazy-add-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G sql --gplan lazy --gplan-h add \
        || exit_status=1

cat tasks-lplan-lazy-add.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} --lplan lazy --lplan-h hadd \
        || exit_status=1

cat tasks-lplan-lazy-add-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} --lplan lazy --lplan-h hadd \
        --lplan-succ-gen sql \
        || exit_status=1



cat tasks-gplan-gbfs-ff.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G dl --gplan gbfs --gplan-h ff \
        || exit_status=1

cat tasks-gplan-gbfs-ff-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G sql --gplan gbfs --gplan-h ff \
        || exit_status=1

cat tasks-gplan-gbfs-add.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G dl --gplan gbfs --gplan-h add \
        || exit_status=1

cat tasks-gplan-gbfs-add-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} -G sql --gplan gbfs --gplan-h add \
        || exit_status=1

cat tasks-lplan-gbfs-add.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} --lplan gbfs --lplan-h hadd \
        || exit_status=1

cat tasks-lplan-gbfs-add-sql.txt | shuf | $select \
    | bash ./test-search.sh ${PDDL} --lplan gbfs --lplan-h hadd \
        --lplan-succ-gen sql \
        || exit_status=1



cat tasks-gplan-astar-blind.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --gplan astar \
        || exit_status=1

cat tasks-lplan-astar-blind.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --lplan astar \
        || exit_status=1

cat tasks-gplan-astar-max.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --gplan astar --gplan-h max \
        || exit_status=1

cat tasks-lplan-astar-max.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --lplan astar --lplan-h hmax \
        || exit_status=1

cat tasks-gplan-astar-lmc.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --gplan astar --gplan-h lmc \
        || exit_status=1

cat tasks-symba-fw-blind.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba fw \
        || exit_status=1

cat tasks-symba-fw-pot.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba fw \
        --symba-fw-pot --symba-fw-pot-cfg A+I \
        || exit_status=1

cat tasks-symba-bw-blind.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba bw \
        || exit_status=1

cat tasks-symba-bw-pot.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba bw \
        --symba-bw-pot --symba-bw-pot-cfg I \
        || exit_status=1

cat tasks-symba-bi-blind.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba bi \
        || exit_status=1

cat tasks-symba-bi-fwpot.txt | shuf | $select \
    | bash ./test-search.sh --optimal ${PDDL} --fdr-ess --h2 --symba bi \
        --symba-fw-pot --symba-fw-pot-cfg A+I \
        || exit_status=1

exit $exit_status
