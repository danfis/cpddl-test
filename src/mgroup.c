#include "test.h"
#include "context.h"
#include <assert.h>

static int mgIsSubset(const pddl_mgroups_t *mgs,
                      const pddl_mgroup_t *m)
{
    for (int i = 0; i < mgs->mgroup_size; ++i){
        if (pddlISetIsSubset(&m->mgroup, &mgs->mgroup[i].mgroup))
            return 1;
    }
    return 0;
}

static int mgsCovered(const pddl_mgroups_t *mgs,
                      const pddl_mgroups_t *c)
{
    for (int i = 0; i < c->mgroup_size; ++i){
        if (!mgIsSubset(mgs, &c->mgroup[i]))
            return 0;
    }
    return 1;
}

static int mgsCovered2(const pddl_mgroups_t *mgs,
                       const pddl_mgroups_t *c)
{
    for (int i = 0; i < c->mgroup_size; ++i){
        if (pddlISetSize(&c->mgroup[i].mgroup) <= 1)
            continue;
        if (!mgIsSubset(mgs, &c->mgroup[i]))
            return 0;
    }
    return 1;
}

static int mgIsIn(const pddl_mgroups_t *mgs, const pddl_mgroup_t *m)
{
    for (int i = 0; i < mgs->mgroup_size; ++i){
        if (pddlISetEq(&m->mgroup, &mgs->mgroup[i].mgroup))
            return 1;
    }
    return 0;
}

static int mgsContained(const pddl_mgroups_t *mgs, const pddl_mgroups_t *c)
{
    for (int i = 0; i < c->mgroup_size; ++i){
        if (!mgIsIn(mgs, &c->mgroup[i]))
            return 0;
    }
    return 1;
}


static pddl_mgroups_t mg_fd;
static pddl_mgroups_t mg_max;
static pddl_mgroups_t mg_all;
static pddl_mgroups_t mg_h2;

TEST(b_mgroup, strips_pruned)
{
    pddlMutexPairsFree(&C.mutex);
    pddlMutexPairsInitStrips(&C.mutex, &C.strips);
    int ret = pddlH2FwMutex(&C.strips, &C.mutex, &C.err);
    assert(ret == 0);

    pddlMGroupsGround(&mg_fd, &C.pddl, &C.lmg_fd, &C.strips);
}

TEST_TEAR_DOWN(b_mgroup)
{
    pddlMGroupsFree(&mg_fd);
}

TEST_COND(famgroup_maximal, b_mgroup, LP)
{
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 1;

    pddlMGroupsInitEmpty(&mg_max);
    int ret = pddlFAMGroupsInfer(&mg_max, &C.strips, &cfg, NULL);
    assert(ret == 0);
    pddlMGroupsSetExactlyOne(&mg_max, &C.strips);
    pddlMGroupsSetGoal(&mg_max, &C.strips);
    pddlMGroupsSortUniq(&mg_max);

    PDDL_ISET(facts);
    for (int mi = 0; mi < mg_max.mgroup_size; ++mi){
        const pddl_mgroup_t *m = mg_max.mgroup + mi;
        assert(pddlStripsIsFAMGroup(&C.strips, &m->mgroup));
        pddlISetUnion(&facts, &m->mgroup);
    }

    int cover_num = pddlMGroupsCoverNumber(&mg_max, C.strips.fact.fact_size);
    assert(cover_num >= 0);
    assert(cover_num - (C.strips.fact.fact_size - pddlISetSize(&facts)) >= 0);
    assert(cover_num <= C.strips.fact.fact_size);
    assert(cover_num - (C.strips.fact.fact_size - pddlISetSize(&facts))
                    <= mg_max.mgroup_size);

    fprintf(stdout, "Maximal %d:\n", mg_max.mgroup_size);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mg_max, stdout);
    fprintf(stdout, "\n");
    fprintf(stdout, "Mutex Group Cover Number: %d\n", cover_num);
    fflush(stdout);
    pddlISetFree(&facts);

    assert(mgsCovered(&mg_max, &C.mg));
    assert(mgsCovered(&mg_max, &mg_fd));
}

TEST_TEAR_DOWN(famgroup_maximal)
{
    pddlMGroupsFree(&mg_max);
}

TEST_COND(famgroup_maximal_goal, famgroup_maximal, LP)
{
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 1;
    cfg.goal = 1;

    pddl_mgroups_t mg;
    pddlMGroupsInitEmpty(&mg);
    int ret = pddlFAMGroupsInfer(&mg, &C.strips, &cfg, NULL);
    assert(ret == 0);
    pddlMGroupsSetExactlyOne(&mg, &C.strips);
    pddlMGroupsSetGoal(&mg, &C.strips);
    pddlMGroupsSortUniq(&mg);

    for (int mi = 0; mi < mg.mgroup_size; ++mi){
        assert(mgIsIn(&mg_max, mg.mgroup + mi));
    }
    pddlMGroupsFree(&mg);
}


TEST_COND(famgroup_all, famgroup_maximal, LP)
{
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 0;

    pddlMGroupsInitEmpty(&mg_all);
    int ret = pddlFAMGroupsInfer(&mg_all, &C.strips, &cfg, NULL);
    assert(ret == 0);
    pddlMGroupsSetExactlyOne(&mg_all, &C.strips);
    pddlMGroupsSetGoal(&mg_all, &C.strips);
    pddlMGroupsSortUniq(&mg_all);

    PDDL_ISET(facts);
    for (int mi = 0; mi < mg_all.mgroup_size; ++mi){
        const pddl_mgroup_t *m = mg_all.mgroup + mi;
        assert(pddlStripsIsFAMGroup(&C.strips, &m->mgroup));
        pddlISetUnion(&facts, &m->mgroup);
    }

    int cover_num = pddlMGroupsCoverNumber(&mg_all, C.strips.fact.fact_size);
    assert(cover_num >= 0);
    assert(cover_num - (C.strips.fact.fact_size - pddlISetSize(&facts)) >= 0);
    assert(cover_num <= C.strips.fact.fact_size);
    assert(cover_num - (C.strips.fact.fact_size - pddlISetSize(&facts))
                    <= mg_all.mgroup_size);

    fprintf(stdout, "Non-Maximal %d:\n", mg_all.mgroup_size);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mg_all, stdout);
    fprintf(stdout, "\n");
    fprintf(stdout, "Mutex Group Cover Number: %d\n", cover_num);
    fflush(stdout);
    pddlISetFree(&facts);

    assert(mgsContained(&mg_all, &mg_max));
    assert(mgsCovered(&mg_max, &mg_all));
    assert(mgsContained(&mg_all, &C.mg));
    assert(mgsContained(&mg_all, &mg_fd));
}

TEST_TEAR_DOWN(famgroup_all)
{
    pddlMGroupsFree(&mg_all);
}

TEST_COND(famgroup_maximal_sym, famgroup_maximal, LP BLISS)
{
    pddl_strips_sym_t sym;
    pddlStripsSymInitPDG(&sym, &C.strips);

    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 1;
    cfg.sym = &sym;

    pddl_mgroups_t mg;
    pddlMGroupsInitEmpty(&mg);
    int ret = pddlFAMGroupsInfer(&mg, &C.strips, &cfg, &C.err);
    assert(ret == 0);
    pddlMGroupsSetExactlyOne(&mg, &C.strips);
    pddlMGroupsSetGoal(&mg, &C.strips);
    pddlMGroupsSortUniq(&mg);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mg, stdout);
    fflush(stdout);
    assert(mgsContained(&mg, &mg_max));
    assert(mgsContained(&mg_max, &mg));
    pddlMGroupsFree(&mg);
    pddlStripsSymFree(&sym);
}


TEST_COND(famgroup_all_sym, famgroup_all, LP BLISS)
{
    pddl_strips_sym_t sym;
    pddlStripsSymInitPDG(&sym, &C.strips);

    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 0;
    cfg.sym = &sym;

    pddl_mgroups_t mg;
    pddlMGroupsInitEmpty(&mg);
    int ret = pddlFAMGroupsInfer(&mg, &C.strips, &cfg, &C.err);
    assert(ret == 0);
    pddlMGroupsSetExactlyOne(&mg, &C.strips);
    pddlMGroupsSetGoal(&mg, &C.strips);
    pddlMGroupsSortUniq(&mg);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mg, stdout);
    fflush(stdout);
    assert(mgsContained(&mg, &mg_all));
    assert(mgsContained(&mg_all, &mg));
    pddlMGroupsFree(&mg);
    pddlStripsSymFree(&sym);
}


TEST(h2mgroup, famgroup_all)
{
    pddl_mutex_pairs_t h2;
    pddlMutexPairsInitStrips(&h2, &C.strips);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH2FwMutex(&C.strips, &h2, &C.err);
    assert(ret == 0);

    pddlMGroupsInitEmpty(&mg_h2);
    pddlMutexPairsInferMutexGroups(&h2, &mg_h2, &C.err);
    pddlMGroupsSetExactlyOne(&mg_h2, &C.strips);
    pddlMGroupsSetGoal(&mg_h2, &C.strips);
    pddlMGroupsSortUniq(&mg_h2);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mg_h2, stdout);
    fflush(stdout);
    assert(mgsCovered2(&mg_h2, &mg_all));
    assert(mgsCovered2(&mg_h2, &mg_max));

    pddlMutexPairsFree(&h2);
}

TEST_TEAR_DOWN(h2mgroup)
{
    pddlMGroupsFree(&mg_h2);
}

TEST(h3mgroup, h2mgroup)
{
    pddl_mutex_pairs_t h3;
    pddlMutexPairsInitStrips(&h3, &C.strips);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH3FwMutex(&C.strips, &h3, &C.err);
    assert(ret == 0);

    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    pddlMutexPairsInferMutexGroups(&h3, &mgs, &C.err);
    pddlMGroupsSortUniq(&mgs);
    fprintf(stdout, "Mutex groups: %d\n", mgs.mgroup_size);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mgs, stdout);
    assert(mgsCovered2(&mgs, &mg_all));
    assert(mgsCovered2(&mgs, &mg_max));
    assert(mgsCovered2(&mgs, &mg_h2));
    pddlMGroupsFree(&mgs);

    pddlMutexPairsFree(&h3);
}
