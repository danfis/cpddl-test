#!/bin/bash

TIMEOUT_SLACK=5
MAX_MEM=4096

GENERATOR=no
if [ "$1" = "--generator" ]; then
    GENERATOR=yes
    shift
fi

OPTIMAL=no
if [ "$1" = "--optimal" ]; then
    OPTIMAL=yes
    shift
fi

exit_status=0
while read line; do
    if echo "$line" | grep -q '^#'; then
        continue
    fi

    input_file=$(echo "$line" | cut -f1 -d' ')
    pddl_file=../pddl-data/${input_file}
    [ ! -f "$pddl_file".pddl ] && echo "No pddl file $pddl_file.pddl" && exit -1

    plan_file=${pddl_file}.plan
    [ ! -f "$plan_file" ] && echo "No plan file $plan_file" && exit -1

    timeout=$(echo "$line" | cut -f2 -d' ')
    timeout=$(($timeout + $TIMEOUT_SLACK))

    outfn=tmp.$$.out
    errfn=tmp.$$.err
    valfn=tmp.$$.val
    timeoutfn=tmp.$$.timeout
    [ "$GENERATOR" = "yes" ] && timeout=30

    timeout $timeout \
        $@ "$pddl_file" \
            --max-mem $MAX_MEM \
            --gplan-out - \
            --lplan-out - \
            --symba-out - \
                >$outfn 2>$errfn
    [ "$?" = "124" ] && touch ${timeoutfn}

    time=$(tail -1 ${errfn} | cut -f1 -d' ' | cut -f2 -d'[' | cut -f1 -d'.')
    time=$(($time + 1))

    if [ -f ${timeoutfn} ]; then
        echo "${input_file} TIMEOUT $timeout / $time"
        exit_status=1

    elif cat ${errfn} | grep -q '[GL]PLAN: Plan found.\|SYMBA: Plan Cost:'; then
        domain_pddl=$(cat $errfn | grep 'PDDL: Processing .* and .*' | cut -f5 -d' ')
        if ! ../val/validate ${domain_pddl} ${pddl_file}.pddl ${outfn} >${valfn} 2>&1; then
            echo "${input_file} INVALID PLAN!"
            cat ${errfn}
            cat ${outfn}
            cat ${valfn}
            exit -1
            exit_status=1
        fi

        optimal_cost=$(cat ${plan_file} | grep -qi 'optimal cost:' | cut -f4 -d' ')
        cost=$(cat ${errfn} | grep -q '[GL]PLAN: Plan Cost:' | cut -f6 -d' ')
        if [ "$OPTIMAL" = "yes" ] && [ "$cost" != "$optimal_cost" ]; then
            echo "${input_file} SUB-OPTIMAL"
            exit_status=1
        else
            [ "$GENERATOR" = "yes" ] && echo "${input_file} ${time}"
        fi

    else
        echo "ERROR:"
        cat ${errfn}
        exit -1
    fi


    rm ${outfn}
    rm ${errfn}
    rm -f ${valfn}
    rm -f ${timeoutfn}
done

exit $exit_status
