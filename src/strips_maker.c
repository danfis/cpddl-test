#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 256
#define BLOCK_SIZE 4096

static int cmpLine(const void *a, const void *b, void *_)
{
    return strcmp(a, b);
}

/** Formats the ground atom stored in SM under ATOM_ID as
 *  "(pred obj1 obj2 ...)". */
static void sformatAtom(char *buf, int buf_size,
                        const pddl_strips_maker_t *sm, int atom_id)
{
    const pddl_ground_atom_t *ga
            = pddlStripsMakerGroundAtomConst(sm, atom_id);
    int w = snprintf(buf, buf_size, "(%s", C.pddl.pred.pred[ga->pred].name);
    for (int i = 0; i < ga->arity; ++i){
        w += snprintf(buf + w, buf_size - w, " %s",
                      C.pddl.obj.obj[ga->arg[i]].name);
    }
    snprintf(buf + w, buf_size - w, ")");
}

/** Formats PREFIX followed by the atoms from STATE sorted by name, and a
 *  terminating newline. Returns the number of written characters. */
static int sformatState(char *buf, int buf_size, const char *prefix,
                        const pddl_iset_t *state,
                        const pddl_strips_maker_t *sm)
{
    int w = snprintf(buf, buf_size, "%s", prefix);
    int size = pddlISetSize(state);
    if (size > 0){
        char (*line)[LINE_SIZE] = calloc(size, LINE_SIZE);
        int li = 0;
        PDDL_ISET_FOR_EACH(state, fact_id)
            sformatAtom(line[li++], LINE_SIZE, sm, fact_id);
        pddlSort(line, size, LINE_SIZE, cmpLine, NULL);
        for (int i = 0; i < size; ++i)
            w += snprintf(buf + w, buf_size - w, " %s", line[i]);
        free(line);
    }
    w += snprintf(buf + w, buf_size - w, "\n");
    return w;
}

/** Compares the two int indexes pointed to by A and B by the names stored
 *  in the (char [][LINE_SIZE]) array passed as UD, ties broken by the
 *  index itself. */
static int cmpIdxByName(const void *a, const void *b, void *ud)
{
    const char (*name)[LINE_SIZE] = ud;
    int i1 = *(const int *)a;
    int i2 = *(const int *)b;
    int cmp = strcmp(name[i1], name[i2]);
    if (cmp == 0)
        return i1 - i2;
    return cmp;
}

/** Initializes SM, adds the initial state (collected in INIT), and walks
 *  NUM_STEPS steps of the task, registering in SM every applicable
 *  grounding together with its effects. If PRINT_LIMIT >= 0, prints a
 *  per-step summary and the effects of the first PRINT_LIMIT applicable
 *  actions of each step. */
static void walk(pddl_strips_maker_t *sm,
                 int num_steps,
                 int print_limit,
                 pddl_iset_t *init)
{
    pddlStripsMakerInit(sm, &C.pddl);
    pddlStripsMakerAddInitAndCollect(sm, &C.pddl, init, NULL);

    pddl_lifted_app_action_t *aa;
    aa = pddlLiftedAppActionNew(&C.pddl, PDDL_LIFTED_APP_ACTION_DL, &C.err);
    assert(aa != NULL);
    pddlLiftedAppActionClearState(aa);

    PDDL_ISET(state);
    pddlISetUnion(&state, init);
    for (int step = 0; step < num_steps; ++step){
        int ret = pddlLiftedAppActionSetStripsState(aa, sm, &state);
        assert(ret == 0);
        ret = pddlLiftedAppActionFindAppActions(aa);
        assert(ret == 0);
        pddlLiftedAppActionSort(aa, &C.pddl);

        int size = pddlLiftedAppActionSize(aa);
        if (print_limit >= 0){
            printf("step %d: state %d applicable %d\n",
                   step, pddlISetSize(&state), size);
        }
        if (size == 0)
            break;

        // Select the PRINT_LIMIT applicable actions that are first in the
        // by-name ordering: print_rank[i] is the position of the i'th
        // applicable action in the printed output, or -1
        int num_print = 0;
        char (*aname)[LINE_SIZE] = NULL;
        int *print_rank = NULL;
        char (*block)[BLOCK_SIZE] = NULL;
        if (print_limit >= 0){
            num_print = (print_limit < size ? print_limit : size);
            aname = calloc(size, LINE_SIZE);
            for (int i = 0; i < size; ++i){
                int aid = pddlLiftedAppActionId(aa, i);
                const pddl_action_t *action = C.pddl.action.action + aid;
                const int *args = pddlLiftedAppActionArgs(aa, i);
                int w = snprintf(aname[i], LINE_SIZE, "%s", action->name);
                for (int j = 0; j < action->param.param_size; ++j){
                    w += snprintf(aname[i] + w, LINE_SIZE - w, " %s",
                                  C.pddl.obj.obj[args[j]].name);
                }
            }
            int *order = calloc(size, sizeof(int));
            for (int i = 0; i < size; ++i)
                order[i] = i;
            pddlSort(order, size, sizeof(int), cmpIdxByName, aname);
            print_rank = calloc(size, sizeof(int));
            for (int i = 0; i < size; ++i)
                print_rank[i] = -1;
            for (int r = 0; r < num_print; ++r)
                print_rank[order[r]] = r;
            free(order);
            block = calloc(num_print, BLOCK_SIZE);
        }

        PDDL_ISET(next_state);
        for (int i = 0; i < size; ++i){
            int aid = pddlLiftedAppActionId(aa, i);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlLiftedAppActionArgs(aa, i);

            PDDL_ISET(add_eff);
            PDDL_ISET(del_eff);
            int cost;
            ret = pddlStripsMakerActionEffInState(sm, &C.pddl, action, args,
                                                  &state, &add_eff, &del_eff,
                                                  &cost, &C.err);
            assert(ret == 0);
            // The documented normalization of the returned effects:
            // del_eff is restricted to the state, add and delete effects
            // are disjoint, and add_eff contains only new facts
            assert(pddlISetIsDisjoint(&add_eff, &del_eff));
            assert(pddlISetIsSubset(&del_eff, &state));
            assert(pddlISetIsDisjoint(&add_eff, &state));
            // Every returned ID is a valid ground atom stored in SM
            PDDL_ISET_FOR_EACH(&add_eff, fid)
                assert(pddlStripsMakerGroundAtomConst(sm, fid)->id == fid);
            PDDL_ISET_FOR_EACH(&del_eff, fid)
                assert(pddlStripsMakerGroundAtomConst(sm, fid)->id == fid);
            if (C.pddl.metric){
                assert(cost >= 0);
            }else{
                cost = 1;
            }

            pddlStripsMakerAddAction(sm, aid, 0, args, NULL);

            if (print_rank != NULL && print_rank[i] >= 0){
                char *b = block[print_rank[i]];
                int w = snprintf(b, BLOCK_SIZE, "%s :: cost: %d\n",
                                 aname[i], cost);
                w += sformatState(b + w, BLOCK_SIZE - w, "    add:",
                                  &add_eff, sm);
                sformatState(b + w, BLOCK_SIZE - w, "    del:", &del_eff, sm);
            }

            if (i == step % size){
                pddlISetMinus2(&next_state, &state, &del_eff);
                pddlISetUnion(&next_state, &add_eff);
            }

            pddlISetFree(&add_eff);
            pddlISetFree(&del_eff);
        }
        if (print_limit >= 0){
            for (int r = 0; r < num_print; ++r)
                printf("%s", block[r]);
            free(block);
            free(print_rank);
            free(aname);
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
    }
    pddlISetFree(&state);
    pddlLiftedAppActionDel(aa);
}

struct reg_goal {
    pddl_strips_maker_t *sm;
    pddl_iset_t *goal;
    int goal_is_false;
};

static int regGoalAtom(pddl_fm_t *c, void *ud)
{
    struct reg_goal *rg = ud;

    if (pddlFmIsAtom(c)){
        const pddl_fm_atom_t *atom = pddlFmToAtomConst(c);
        if (pddlGroundAtomsFindAtom(&rg->sm->ground_atom_static,
                                    atom, NULL) == NULL){
            const pddl_ground_atom_t *ga;
            ga = pddlStripsMakerAddAtom(rg->sm, atom, NULL, NULL);
            pddlISetAdd(rg->goal, ga->id);
        }

    }else if (pddlFmIsBool(c)){
        if (!pddlFmToBoolConst(c)->val)
            rg->goal_is_false = 1;
    }
    return 0;
}

/** Registers every non-static goal atom in SM so that
 *  pddlStripsMakerMakeStrips() does not turn the problem into the
 *  trivial unsolvable one, and collects the corresponding ground atom
 *  IDs in RG->goal. */
static void registerGoal(pddl_strips_maker_t *sm, struct reg_goal *rg)
{
    rg->sm = sm;
    rg->goal_is_false = 0;
    pddlFmTraverseProp(C.pddl.goal, regGoalAtom, NULL, rg);
}

/** Closes the set of ground atoms stored in SM under the effects of all
 *  registered groundings: pddlStripsMakerMakeStrips() requires that every
 *  effect atom of a grounding whose conditional-effect precondition atoms
 *  are all present is itself present. Re-running
 *  pddlStripsMakerActionEffInState() with the state consisting of all
 *  ground atoms matches exactly the condition used by MakeStrips. */
static void closeAtomsUnderEffects(pddl_strips_maker_t *sm)
{
    int prev_size = -1;
    while (prev_size != sm->ground_atom.atom_size){
        prev_size = sm->ground_atom.atom_size;

        PDDL_ISET(state);
        for (int i = 0; i < sm->ground_atom.atom_size; ++i)
            pddlISetAdd(&state, i);

        for (int i = 0; i < sm->num_action_args; ++i){
            const pddl_ground_action_args_t *ga;
            ga = pddlStripsMakerActionArgs(sm, i);
            const pddl_action_t *action
                    = C.pddl.action.action + ga->action_id;

            PDDL_ISET(add_eff);
            PDDL_ISET(del_eff);
            int cost;
            int ret = pddlStripsMakerActionEffInState(sm, &C.pddl, action,
                                                      ga->arg, &state,
                                                      &add_eff, &del_eff,
                                                      &cost, &C.err);
            assert(ret == 0);
            pddlISetFree(&add_eff);
            pddlISetFree(&del_eff);
        }
        pddlISetFree(&state);
    }
}

TEST(strips_maker, pddl)
{
    if (pddlHasNumericFluents(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
}

TEST(strips_maker_atoms, strips_maker)
{
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &C.pddl);

    int num_atoms = 0;
    int num_static = 0;
    PDDL_INIT_STATE_FOR_EACH_ATOM(&C.pddl.init, a){
        int is_new;
        const pddl_ground_atom_t *ga;
        if (pddlIsPredStatic(&C.pddl, a->pred)){
            ga = pddlStripsMakerAddStaticAtom(&sm, a, NULL, &is_new);
            if (is_new){
                assert(ga->id == num_static);
                ++num_static;
            }else{
                assert(ga->id < num_static);
            }
        }else{
            ga = pddlStripsMakerAddAtom(&sm, a, NULL, &is_new);
            if (is_new){
                assert(ga->id == num_atoms);
                ++num_atoms;
            }else{
                assert(ga->id < num_atoms);
            }
        }
        assert(ga->pred == a->pred);
        assert(ga->arity == a->arity);
    }
    assert(sm.ground_atom.atom_size == num_atoms);
    assert(sm.ground_atom_static.atom_size == num_static);

    // The second identical pass must only deduplicate
    PDDL_INIT_STATE_FOR_EACH_ATOM(&C.pddl.init, a){
        int is_new;
        if (pddlIsPredStatic(&C.pddl, a->pred)){
            pddlStripsMakerAddStaticAtom(&sm, a, NULL, &is_new);
        }else{
            pddlStripsMakerAddAtom(&sm, a, NULL, &is_new);
        }
        assert(!is_new);
    }
    assert(sm.ground_atom.atom_size == num_atoms);
    assert(sm.ground_atom_static.atom_size == num_static);

    if (num_atoms > 0){
        // The Pred variant hits the same set as the atom variant
        const pddl_ground_atom_t *ga0 = pddlStripsMakerGroundAtom(&sm, 0);
        int is_new;
        const pddl_ground_atom_t *ga;
        ga = pddlStripsMakerAddAtomPred(&sm, ga0->pred, ga0->arg,
                                        ga0->arity, &is_new);
        assert(!is_new);
        assert(ga == ga0);

        // The static set is fully independent -- the same atom gets its
        // own ID there
        ga = pddlStripsMakerAddStaticAtomPred(&sm, ga0->pred, ga0->arg,
                                              ga0->arity, &is_new);
        assert(is_new);
        assert(ga->id == num_static);
        assert(sm.ground_atom.atom_size == num_atoms);
        assert(sm.ground_atom_static.atom_size == num_static + 1);
    }

    printf("atoms: %d static: %d\n", num_atoms, num_static);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_init, strips_maker)
{
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &C.pddl);

    PDDL_ISET(facts);
    PDDL_ISET(static_facts);
    int ret = pddlStripsMakerAddInitAndCollect(&sm, &C.pddl,
                                               &facts, &static_facts);
    assert(ret == 0);
    assert(pddlISetSize(&facts) == sm.ground_atom.atom_size);
    assert(pddlISetSize(&static_facts) == sm.ground_atom_static.atom_size);

    PDDL_ISET_FOR_EACH(&facts, fid){
        const pddl_ground_atom_t *ga
                = pddlStripsMakerGroundAtomConst(&sm, fid);
        assert(ga->id == fid);
        assert(!pddlIsPredStatic(&C.pddl, ga->pred));
    }
    PDDL_ISET_FOR_EACH(&static_facts, fid){
        const pddl_ground_atom_t *ga = sm.ground_atom_static.atom[fid];
        assert(ga->id == fid);
        assert(pddlIsPredStatic(&C.pddl, ga->pred));
    }

    // Every init atom is routed into exactly one of the two sets
    // according to the staticness of its predicate
    PDDL_INIT_STATE_FOR_EACH_ATOM(&C.pddl.init, a){
        if (pddlIsPredStatic(&C.pddl, a->pred)){
            assert(pddlGroundAtomsFindAtom(&sm.ground_atom_static,
                                           a, NULL) != NULL);
            assert(pddlGroundAtomsFindAtom(&sm.ground_atom, a, NULL) == NULL);
        }else{
            assert(pddlGroundAtomsFindAtom(&sm.ground_atom, a, NULL) != NULL);
            assert(pddlGroundAtomsFindAtom(&sm.ground_atom_static,
                                           a, NULL) == NULL);
        }
    }

    // Every init fluent is stored with its initial value
    int num_fluents = 0;
    pddl_num_val_t val;
    PDDL_INIT_STATE_FOR_EACH_FLUENT(&C.pddl.init, fluent, &val){
        const pddl_ground_atom_t *ga
                = pddlGroundAtomsFindAtom(&sm.ground_func, fluent, NULL);
        assert(ga != NULL);
        const pddl_ground_func_data_t *fd
                = pddlExtArrGet(sm.ground_func_data, ga->id);
        assert(pddlNumValCmp(&fd->init_val, &val) == 0);
        ++num_fluents;
    }
    assert(num_fluents == sm.ground_func.atom_size);

    // Plain AddInit collects exactly the same sets
    pddl_strips_maker_t sm2;
    pddlStripsMakerInit(&sm2, &C.pddl);
    ret = pddlStripsMakerAddInit(&sm2, &C.pddl);
    assert(ret == 0);
    assert(sm2.ground_atom.atom_size == sm.ground_atom.atom_size);
    assert(sm2.ground_atom_static.atom_size
                == sm.ground_atom_static.atom_size);
    assert(sm2.ground_func.atom_size == sm.ground_func.atom_size);
    pddlStripsMakerFree(&sm2);

    printf("init atoms: %d static: %d fluents: %d\n",
           sm.ground_atom.atom_size, sm.ground_atom_static.atom_size,
           sm.ground_func.atom_size);

    pddlISetFree(&facts);
    pddlISetFree(&static_facts);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_actions, strips_maker)
{
    if (C.pddl.action.action_size == 0 || C.pddl.obj.obj_size == 0)
        return;

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &C.pddl);

    int num = 0;
    for (int ai = 0; ai < C.pddl.action.action_size; ++ai){
        int psize = C.pddl.action.action[ai].param.param_size;
        int args[psize + 1];
        for (int j = 0; j < psize; ++j)
            args[j] = j % C.pddl.obj.obj_size;

        int is_new;
        const pddl_ground_action_args_t *g;
        g = pddlStripsMakerAddAction(&sm, ai, 0, args, &is_new);
        assert(is_new);
        assert(g->id == num);
        ++num;
        assert(g->action_id == ai);
        assert(g->action_id2 == 0);
        assert(memcmp(g->arg, args, sizeof(int) * psize) == 0);

        // Re-adding the same grounding deduplicates
        const pddl_ground_action_args_t *g2;
        g2 = pddlStripsMakerAddAction(&sm, ai, 0, args, &is_new);
        assert(!is_new);
        assert(g2 == g);

        // action_id2 is part of the identity of the grounding
        g2 = pddlStripsMakerAddAction(&sm, ai, ai + 1, args, &is_new);
        assert(is_new);
        assert(g2->id == num);
        ++num;
        assert(g2->action_id2 == ai + 1);

        if (psize > 0 && C.pddl.obj.obj_size > 1){
            for (int j = 0; j < psize; ++j)
                args[j] = (j + 1) % C.pddl.obj.obj_size;
            g2 = pddlStripsMakerAddAction(&sm, ai, 0, args, &is_new);
            assert(is_new);
            assert(g2->id == num);
            ++num;
        }
    }

    assert(sm.num_action_args == num);
    for (int i = 0; i < num; ++i)
        assert(pddlStripsMakerActionArgs(&sm, i)->id == i);

    printf("groundings: %d\n", num);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_eff_in_state, strips_maker)
{
    pddl_strips_maker_t sm;
    PDDL_ISET(init_facts);
    walk(&sm, 5, 3, &init_facts);
    printf("ground atoms: %d groundings: %d\n",
           sm.ground_atom.atom_size, sm.num_action_args);
    pddlISetFree(&init_facts);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_make_strips, strips_maker)
{
    pddl_strips_maker_t sm;
    PDDL_ISET(init_facts);
    walk(&sm, 5, -1, &init_facts);

    PDDL_ISET(goal);
    struct reg_goal rg = { &sm, &goal, 0 };
    registerGoal(&sm, &rg);
    closeAtomsUnderEffects(&sm);

    // Variant A: keep static facts as they are
    pddl_ground_config_t cfg = PDDL_GROUND_CONFIG_INIT;
    cfg.remove_static_facts = pddl_false;
    pddl_strips_t s0;
    int ret = pddlStripsMakerMakeStrips(&sm, &C.pddl, &cfg, &s0, &C.err);
    assert(ret == 0);
    assert(s0.goal_is_unreachable == rg.goal_is_false);
    printf("A: facts %d ops %d init %d goal %d unreachable %d\n",
           s0.fact.fact_size, s0.op.op_size, pddlISetSize(&s0.init),
           pddlISetSize(&s0.goal), (int)s0.goal_is_unreachable);
    if (s0.goal_is_unreachable){
        // The whole problem was replaced by the trivial unsolvable one,
        // so there is nothing more to check
        pddlStripsFree(&s0);
        pddlISetFree(&goal);
        pddlISetFree(&init_facts);
        pddlStripsMakerFree(&sm);
        return;
    }

    // Every non-static ground atom became a fact, and the facts are
    // sorted by name (so fact IDs differ from ground atom IDs in general)
    assert(s0.fact.fact_size == sm.ground_atom.atom_size);
    for (int i = 1; i < s0.fact.fact_size; ++i)
        assert(strcmp(s0.fact.fact[i - 1]->name, s0.fact.fact[i]->name) < 0);
    assert(pddlISetSize(&s0.init) == pddlISetSize(&init_facts));
    assert(pddlISetSize(&s0.goal) == pddlISetSize(&goal));
    for (int i = 0; i < s0.op.op_size; ++i){
        const pddl_strips_op_t *op = s0.op.op[i];
        // Operators with no effect are dropped
        assert(pddlISetSize(&op->add_eff) + pddlISetSize(&op->del_eff)
                    + op->cond_eff_size > 0);
        if (!C.pddl.metric)
            assert(op->cost == 1 || op->is_aux_remove_from_plan);
        assert(op->cost >= 0);
        assert(op->action_args_size == 0);
    }

    // Variant B: keep also all static facts
    cfg.keep_all_static_facts = pddl_true;
    pddl_strips_t s1;
    ret = pddlStripsMakerMakeStrips(&sm, &C.pddl, &cfg, &s1, &C.err);
    assert(ret == 0);
    assert(s1.fact.fact_size
                == sm.ground_atom.atom_size
                        + sm.ground_atom_static.atom_size);
    // All static atoms came from the initial state
    assert(pddlISetSize(&s1.init)
                == pddlISetSize(&s0.init) + sm.ground_atom_static.atom_size);
    assert(s1.op.op_size >= s0.op.op_size);
    printf("B: facts %d ops %d init %d\n", s1.fact.fact_size,
           s1.op.op_size, pddlISetSize(&s1.init));

    // Variant C: remove static facts
    cfg.keep_all_static_facts = pddl_false;
    cfg.remove_static_facts = pddl_true;
    pddl_strips_t s2;
    ret = pddlStripsMakerMakeStrips(&sm, &C.pddl, &cfg, &s2, &C.err);
    assert(ret == 0);
    assert(s2.fact.fact_size <= s0.fact.fact_size);
    printf("C: facts %d ops %d\n", s2.fact.fact_size, s2.op.op_size);

    // Variant D: keep action arguments in the operators
    cfg.remove_static_facts = pddl_false;
    cfg.keep_action_args = pddl_true;
    pddl_strips_t s3;
    ret = pddlStripsMakerMakeStrips(&sm, &C.pddl, &cfg, &s3, &C.err);
    assert(ret == 0);
    for (int i = 0; i < s3.op.op_size; ++i){
        const pddl_strips_op_t *op = s3.op.op[i];
        const pddl_action_t *action
                = C.pddl.action.action + op->pddl_action_id;
        assert(op->action_args_size == action->param.param_size);
        // The operator name is the action name followed by the arguments
        char name[1024];
        int w = snprintf(name, sizeof(name), "%s", action->name);
        for (int j = 0; j < op->action_args_size; ++j){
            w += snprintf(name + w, sizeof(name) - w, " %s",
                          C.pddl.obj.obj[op->action_args[j]].name);
        }
        assert(strcmp(name, op->name) == 0);
    }
    printf("D: facts %d ops %d\n", s3.fact.fact_size, s3.op.op_size);

    // A grounding with non-zero action_id2 is skipped whenever the same
    // grounding with action_id2 == 0 exists
    if (sm.num_action_args > 0){
        const pddl_ground_action_args_t *g0
                = pddlStripsMakerActionArgs(&sm, 0);
        int is_new;
        pddlStripsMakerAddAction(&sm, g0->action_id, 1000, g0->arg, &is_new);
        assert(is_new);

        cfg = (pddl_ground_config_t)PDDL_GROUND_CONFIG_INIT;
        cfg.remove_static_facts = pddl_false;
        pddl_strips_t s4;
        ret = pddlStripsMakerMakeStrips(&sm, &C.pddl, &cfg, &s4, &C.err);
        assert(ret == 0);
        assert(s4.op.op_size == s0.op.op_size);
        pddlStripsFree(&s4);
    }

    pddlStripsFree(&s3);
    pddlStripsFree(&s2);
    pddlStripsFree(&s1);
    pddlStripsFree(&s0);
    pddlISetFree(&goal);
    pddlISetFree(&init_facts);
    pddlStripsMakerFree(&sm);
}

TEST_ONCE(strips_maker_once)
{
}

static void loadPddl(pddl_t *pddl, const char *domain_fn,
                     const char *problem_fn)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlInit(pddl, domain_fn, problem_fn, &cfg, &err);
    if (ret != 0)
        pddlErrPrint(&err, 1, stderr);
    assert(ret == 0);
}

static void dumpInitFluents(const char *header,
                            const char *domain_fn,
                            const char *problem_fn)
{
    pddl_t pddl;
    loadPddl(&pddl, domain_fn, problem_fn);

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm, &pddl);
    assert(ret == 0);

    printf("%s\n", header);
    int num = sm.ground_func.atom_size;
    if (num > 0){
        char (*line)[LINE_SIZE] = calloc(num, LINE_SIZE);
        char buf[128];
        for (int i = 0; i < num; ++i){
            const pddl_ground_atom_t *ga = sm.ground_func.atom[i];
            const pddl_ground_func_data_t *fd
                    = pddlExtArrGet(sm.ground_func_data, ga->id);
            int w = snprintf(line[i], LINE_SIZE, "(%s",
                             pddl.func.pred[ga->pred].name);
            for (int j = 0; j < ga->arity; ++j){
                w += snprintf(line[i] + w, LINE_SIZE - w, " %s",
                              pddl.obj.obj[ga->arg[j]].name);
            }
            snprintf(line[i] + w, LINE_SIZE - w, ") = %s",
                     pddlNumValFmt(&fd->init_val, buf, sizeof(buf)));
        }
        pddlSort(line, num, LINE_SIZE, cmpLine, NULL);
        for (int i = 0; i < num; ++i)
            printf("%s\n", line[i]);
        free(line);
    }

    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

TEST(strips_maker_once_add_func, strips_maker_once)
{
    dumpInitFluents("various/num-cost-expr/p01:",
                    "pddl/various/num-cost-expr/domain.pddl",
                    "pddl/various/num-cost-expr/p01.pddl");
    dumpInitFluents("various/num-float-coef/p01:",
                    "pddl/various/num-float-coef/domain.pddl",
                    "pddl/various/num-float-coef/p01.pddl");

    // Re-adding a function atom overwrites the stored value
    pddl_t pddl;
    loadPddl(&pddl, "pddl/various/num-cost-expr/domain.pddl",
             "pddl/various/num-cost-expr/p01.pddl");
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm, &pddl);
    assert(ret == 0);

    const pddl_fm_atom_t *first = NULL;
    pddl_num_val_t orig;
    PDDL_INIT_STATE_FOR_EACH_FLUENT(&pddl.init, fluent, NULL){
        if (first == NULL)
            first = fluent;
    }
    assert(first != NULL);

    const pddl_ground_atom_t *ga
            = pddlGroundAtomsFindAtom(&sm.ground_func, first, NULL);
    assert(ga != NULL);
    int orig_id = ga->id;
    const pddl_ground_func_data_t *fd
            = pddlExtArrGet(sm.ground_func_data, ga->id);
    pddlNumValSet(&orig, &fd->init_val);
    char buf[128];
    printf("before: %s\n", pddlNumValFmt(&fd->init_val, buf, sizeof(buf)));

    pddl_num_val_t v;
    pddlNumValSetInt(&v, 42);
    int is_new;
    const pddl_ground_atom_t *ga2
            = pddlStripsMakerAddFunc(&sm, first, NULL, &v, &is_new);
    assert(!is_new);
    assert(ga2->id == orig_id);
    fd = pddlExtArrGet(sm.ground_func_data, ga2->id);
    assert(pddlNumValCmp(&fd->init_val, &v) == 0);
    printf("after: %s\n", pddlNumValFmt(&fd->init_val, buf, sizeof(buf)));

    // Restore the original value
    ga2 = pddlStripsMakerAddFunc(&sm, first, NULL, &orig, &is_new);
    assert(!is_new);
    assert(ga2->id == orig_id);
    fd = pddlExtArrGet(sm.ground_func_data, ga2->id);
    assert(pddlNumValCmp(&fd->init_val, &orig) == 0);

    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

static void addInitFn(void *userdata)
{
    pddl_t *pddl = userdata;
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, pddl);
    pddlStripsMakerAddInit(&sm, pddl);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_once_unsolvable_init, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/various/num-cost-renamed/domain.pddl",
             "pddl/various/num-cost-renamed/p01.pddl");

    // A solvable initial state does not panic
    assert(!testPanic(addInitFn, &pddl));

    // AddInit panics on an initial state marked as unsolvable
    pddlInitStateSetUnsolvable(&pddl.init);
    assert(testPanic(addInitFn, &pddl));

    pddlFree(&pddl);
    printf("unsolvable init state panics\n");
}
