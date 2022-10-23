#include <assert.h>
#include "test.h"
#include "context.h"

context_t C = { 0 };

static int optimalCost(const char *problem_fn)
{
    int cost = -1;
    int fnlen = strlen(problem_fn);
    char *fn = PDDL_ALLOC_ARR(char, fnlen + 2);
    strcpy(fn, problem_fn);
    strcpy(fn + fnlen - 4, "plan");

    FILE *fin = fopen(fn, "r");
    if (fin == NULL){
        PDDL_FREE(fn);
        return -1;
    }

    size_t len = 0;
    char *line = NULL;
    ssize_t nread;

    while ((nread = getline(&line, &len, fin)) != -1){
        char *f = strstr(line, "Optimal cost:");
        if (f != NULL){
            char *p = f + strlen("Optimal cost:");
            for (; *p == ' '; ++p);
            char *end = p;
            for (; *end >= '0' && *end <= '9'; ++end);
            *end = 0;
            cost = atoi(p);
            break;
        }
    }
    fclose(fin);

    if (line != NULL)
        free(line);

    PDDL_FREE(fn);
    return cost;
}

int validateLiftedPlan(const pddl_lifted_plan_t *plan)
{
    char fn[128];
    sprintf(fn, "validate-%d-XXXXXX", (int)getpid());
    int fd = mkstemp(fn);
    FILE *fplan = fdopen(fd, "w");
    assert(fplan != NULL);
    for (int i = 0; i < plan->plan_len; ++i){
        fprintf(fplan, "(%s)\n", plan->plan[i]);
    }
    fflush(fplan);
    fclose(fplan);

    char cmd[256];
    sprintf(cmd, "val/validate -v %s %s %s >/dev/null 2>&1",
            C.files.domain_pddl, C.files.problem_pddl, fn);
    int sret = system(cmd);

    unlink(fn);
    return sret;
}

TEST(r, _)
{
    pddlErrInit(&C.err);
    //pddlErrWarnEnable(&C.err, stderr);
    //pddlErrInfoEnable(&C.err, stderr);
    if (pddlFiles(&C.files, "pddl-data/", TEST_TASK, &C.err) != 0){
        pddlErrPrint(&C.err, 1, stderr);
        assert(0);
    }
    C.optimal_cost = optimalCost(C.files.problem_pddl);
}

TEST_GLOBAL_TEAR_DOWN()
{
    if (C.fdr_app_op_set)
        pddlFDRAppOpFree(&C.fdr_app_op);
    if (C.fdr_set)
        pddlFDRFree(&C.fdr);
    if (C.mutex_set)
        pddlMutexPairsFree(&C.mutex);
    if (C.mg_set)
        pddlMGroupsFree(&C.mg);
    if (C.strips_set)
        pddlStripsFree(&C.strips);
    if (C.lmg_set)
        pddlLiftedMGroupsFree(&C.lmg);
    if (C.lmg_fd_set)
        pddlLiftedMGroupsFree(&C.lmg_fd);
    if (C.lmg_mono_set)
        pddlLiftedMGroupsFree(&C.lmg_mono);
    if (C.pddl_set)
        pddlFree(&C.pddl);
}
