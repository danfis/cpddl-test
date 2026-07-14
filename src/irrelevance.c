#include "test.h"
#include "context.h"
#include <assert.h>

TEST(irrelevance, strips_pruned)
{
    PDDL_ISET(irr_fact);
    PDDL_ISET(irr_op);
    PDDL_ISET(static_fact);

    int ret = pddlIrrelevanceAnalysis(&C.strips, &irr_fact, &irr_op,
                                      &static_fact, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&irr_fact) > 0){
        fprintf(stdout, "Irrelevant facts [%d/%d, static: %d]:\n",
                pddlISetSize(&irr_fact), C.strips.fact.fact_size,
                pddlISetSize(&static_fact));
        PDDL_ISET_FOR_EACH(&irr_fact, fact){
            fprintf(stdout, "  ");
            if (pddlISetIn(fact, &static_fact))
                fprintf(stdout, "S:");
            fprintf(stdout, "(%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&irr_op) > 0){
        fprintf(stdout, "Irrelevant ops [%d/%d]:\n",
                pddlISetSize(&irr_op), C.strips.op.op_size);
        PDDL_ISET_FOR_EACH(&irr_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&irr_fact);
    pddlISetFree(&irr_op);
    pddlISetFree(&static_fact);
}


// Returns true if the unconditional effect of OP adds or deletes a
// relevant fact.
static int refUncondEffTriggered(const pddl_strips_op_t *op,
                                 const int *rel_fact)
{
    PDDL_ISET_FOR_EACH(&op->add_eff, f){
        if (rel_fact[f])
            return 1;
    }
    PDDL_ISET_FOR_EACH(&op->del_eff, f){
        if (rel_fact[f])
            return 1;
    }
    return 0;
}

// Returns true if the conditional effect CE adds or deletes a relevant
// fact.
static int refCondEffTriggered(const pddl_strips_op_cond_eff_t *ce,
                               const int *rel_fact)
{
    PDDL_ISET_FOR_EACH(&ce->add_eff, f){
        if (rel_fact[f])
            return 1;
    }
    PDDL_ISET_FOR_EACH(&ce->del_eff, f){
        if (rel_fact[f])
            return 1;
    }
    return 0;
}

static void refMarkRelevant(const pddl_iset_t *facts,
                            const int *fact_static,
                            int *rel_fact,
                            int *change)
{
    PDDL_ISET_FOR_EACH(facts, f){
        if (!rel_fact[f] && !fact_static[f]){
            rel_fact[f] = 1;
            *change = 1;
        }
    }
}

// Brute-force reference implementation of the irrelevance analysis:
// iterate the definition of backward relevance over operators and their
// conditional effects until fixpoint.
static void refIrrelevanceStrips(const pddl_strips_t *s,
                                 pddl_iset_t *irr_fact,
                                 pddl_iset_t *irr_op,
                                 pddl_iset_t *static_fact)
{
    // Static facts: hold in the initial state and no effect ever changes
    // them
    int *fact_static = PDDL_ZALLOC_ARR(int, s->fact.fact_size);
    for (int fi = 0; fi < s->fact.fact_size; ++fi)
        fact_static[fi] = pddlISetIn(fi, &s->init);
    for (int op_id = 0; op_id < s->op.op_size; ++op_id){
        const pddl_strips_op_t *op = s->op.op[op_id];
        PDDL_ISET_FOR_EACH(&op->add_eff, f)
            fact_static[f] = 0;
        PDDL_ISET_FOR_EACH(&op->del_eff, f)
            fact_static[f] = 0;
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            PDDL_ISET_FOR_EACH(&ce->add_eff, f)
                fact_static[f] = 0;
            PDDL_ISET_FOR_EACH(&ce->del_eff, f)
                fact_static[f] = 0;
        }
    }

    // Relevant facts: non-static goal facts, and non-static facts from
    // conditions of triggered effects
    int *rel_fact = PDDL_ZALLOC_ARR(int, s->fact.fact_size);
    int change = 0;
    refMarkRelevant(&s->goal, fact_static, rel_fact, &change);

    change = 1;
    while (change){
        change = 0;
        for (int op_id = 0; op_id < s->op.op_size; ++op_id){
            const pddl_strips_op_t *op = s->op.op[op_id];
            if (refUncondEffTriggered(op, rel_fact))
                refMarkRelevant(&op->pre, fact_static, rel_fact, &change);
            for (int cei = 0; cei < op->cond_eff_size; ++cei){
                const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
                if (refCondEffTriggered(ce, rel_fact)){
                    refMarkRelevant(&op->pre, fact_static,
                                    rel_fact, &change);
                    refMarkRelevant(&ce->pre, fact_static,
                                    rel_fact, &change);
                }
            }
        }
    }

    for (int fi = 0; fi < s->fact.fact_size; ++fi){
        if (fact_static[fi]){
            pddlISetAdd(irr_fact, fi);
            pddlISetAdd(static_fact, fi);
        }else if (!rel_fact[fi]){
            pddlISetAdd(irr_fact, fi);
        }
    }

    // An operator is relevant iff at least one of its effects is
    // triggered
    for (int op_id = 0; op_id < s->op.op_size; ++op_id){
        const pddl_strips_op_t *op = s->op.op[op_id];
        int rel = refUncondEffTriggered(op, rel_fact);
        for (int cei = 0; !rel && cei < op->cond_eff_size; ++cei)
            rel = refCondEffTriggered(op->cond_eff + cei, rel_fact);
        if (!rel)
            pddlISetAdd(irr_op, op_id);
    }

    PDDL_FREE(fact_static);
    PDDL_FREE(rel_fact);
}

TEST(irrelevance_cond_eff, strips_ce)
{
    PDDL_ISET(irr_fact);
    PDDL_ISET(irr_op);
    PDDL_ISET(static_fact);
    int ret = pddlIrrelevanceAnalysis(&C.strips, &irr_fact, &irr_op,
                                      &static_fact, &C.err);
    assert(ret == 0);

    // Compare with the brute-force reference implementation
    PDDL_ISET(ref_irr_fact);
    PDDL_ISET(ref_irr_op);
    PDDL_ISET(ref_static_fact);
    refIrrelevanceStrips(&C.strips, &ref_irr_fact, &ref_irr_op,
                         &ref_static_fact);
    assert(pddlISetEq(&irr_fact, &ref_irr_fact));
    assert(pddlISetEq(&irr_op, &ref_irr_op));
    assert(pddlISetEq(&static_fact, &ref_static_fact));
    pddlISetFree(&ref_irr_fact);
    pddlISetFree(&ref_irr_op);
    pddlISetFree(&ref_static_fact);

    if (pddlISetSize(&irr_fact) > 0){
        fprintf(stdout, "Irrelevant facts [%d/%d, static: %d]:\n",
                pddlISetSize(&irr_fact), C.strips.fact.fact_size,
                pddlISetSize(&static_fact));
        PDDL_ISET_FOR_EACH(&irr_fact, fact){
            fprintf(stdout, "  ");
            if (pddlISetIn(fact, &static_fact))
                fprintf(stdout, "S:");
            fprintf(stdout, "(%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&irr_op) > 0){
        fprintf(stdout, "Irrelevant ops [%d/%d]:\n",
                pddlISetSize(&irr_op), C.strips.op.op_size);
        PDDL_ISET_FOR_EACH(&irr_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }

    pddlISetFree(&irr_fact);
    pddlISetFree(&irr_op);
    pddlISetFree(&static_fact);
}


// Returns true if the effect assigns a value to a relevant variable.
static int refFDREffTriggered(const pddl_fdr_part_state_t *eff,
                              const int *rel_var)
{
    for (int fi = 0; fi < eff->fact_size; ++fi){
        if (rel_var[eff->fact[fi].var])
            return 1;
    }
    return 0;
}

static void refFDRMarkRelevant(const pddl_fdr_part_state_t *pre,
                               int *rel_var,
                               int *change)
{
    for (int fi = 0; fi < pre->fact_size; ++fi){
        if (!rel_var[pre->fact[fi].var]){
            rel_var[pre->fact[fi].var] = 1;
            *change = 1;
        }
    }
}

// Brute-force reference implementation of the irrelevance analysis on FDR
// variables.
static void refIrrelevanceFDR(const pddl_fdr_t *fdr,
                              pddl_iset_t *irr_var,
                              pddl_iset_t *irr_op)
{
    int *rel_var = PDDL_ZALLOC_ARR(int, fdr->var.var_size);
    int change = 0;
    refFDRMarkRelevant(&fdr->goal, rel_var, &change);

    change = 1;
    while (change){
        change = 0;
        for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
            const pddl_fdr_op_t *op = fdr->op.op[op_id];
            if (refFDREffTriggered(&op->eff, rel_var))
                refFDRMarkRelevant(&op->pre, rel_var, &change);
            for (int cei = 0; cei < op->cond_eff_size; ++cei){
                const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
                if (refFDREffTriggered(&ce->eff, rel_var)){
                    refFDRMarkRelevant(&op->pre, rel_var, &change);
                    refFDRMarkRelevant(&ce->pre, rel_var, &change);
                }
            }
        }
    }

    for (int var = 0; var < fdr->var.var_size; ++var){
        if (!rel_var[var])
            pddlISetAdd(irr_var, var);
    }
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *op = fdr->op.op[op_id];
        int rel = refFDREffTriggered(&op->eff, rel_var);
        for (int cei = 0; !rel && cei < op->cond_eff_size; ++cei)
            rel = refFDREffTriggered(&op->cond_eff[cei].eff, rel_var);
        if (!rel)
            pddlISetAdd(irr_op, op_id);
    }

    PDDL_FREE(rel_var);
}

TEST(irrelevance_fdr_cond_eff, strips_ce)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInit(&mutex, C.strips.fact.fact_size);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    pddl_fdr_t fdr;
    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    int ret = pddlFDRInitFromStrips(&fdr, &C.strips, &C.mg, &mutex,
                                    &cfg, &C.err);
    assert(ret == 0);
    pddlMutexPairsFree(&mutex);

    PDDL_ISET(irr_var);
    PDDL_ISET(irr_op);
    ret = pddlIrrelevanceAnalysisFDR(&fdr, &irr_var, &irr_op, &C.err);
    assert(ret == 0);

    // Compare with the brute-force reference implementation
    PDDL_ISET(ref_irr_var);
    PDDL_ISET(ref_irr_op);
    refIrrelevanceFDR(&fdr, &ref_irr_var, &ref_irr_op);
    assert(pddlISetEq(&irr_var, &ref_irr_var));
    assert(pddlISetEq(&irr_op, &ref_irr_op));
    pddlISetFree(&ref_irr_var);
    pddlISetFree(&ref_irr_op);

    if (pddlISetSize(&irr_var) > 0){
        fprintf(stdout, "Irrelevant vars [%d/%d]:\n",
                pddlISetSize(&irr_var), fdr.var.var_size);
        PDDL_ISET_FOR_EACH(&irr_var, var){
            fprintf(stdout, "  var%d:", var);
            for (int val = 0; val < fdr.var.var[var].val_size; ++val)
                fprintf(stdout, " (%s)", fdr.var.var[var].val[val].name);
            fprintf(stdout, "\n");
        }
    }
    if (pddlISetSize(&irr_op) > 0){
        fprintf(stdout, "Irrelevant ops [%d/%d]:\n",
                pddlISetSize(&irr_op), fdr.op.op_size);
        PDDL_ISET_FOR_EACH(&irr_op, op)
            fprintf(stdout, "  (%s)\n", fdr.op.op[op]->name);
    }

    pddlISetFree(&irr_var);
    pddlISetFree(&irr_op);
    pddlFDRFree(&fdr);
}
