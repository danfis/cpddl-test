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

/** Prints PREFIX followed by the atoms from STATE sorted by name, and a
 *  terminating newline. */
static void printState(const char *prefix,
                       const pddl_iset_t *state,
                       const pddl_strips_maker_t *sm)
{
    printf("%s", prefix);
    int size = pddlISetSize(state);
    if (size > 0){
        char (*line)[LINE_SIZE] = calloc(size, LINE_SIZE);
        int li = 0;
        PDDL_ISET_FOR_EACH(state, fact_id)
            sformatAtom(line[li++], LINE_SIZE, sm, fact_id);
        pddlSort(line, size, LINE_SIZE, cmpLine, NULL);
        for (int i = 0; i < size; ++i)
            printf(" %s", line[i]);
        free(line);
    }
    printf("\n");
}

/** Prints PREFIX followed by "(func obj ...) = value" for every
 *  non-static fluent of SM in the numeric-state index order, and a
 *  terminating newline. */
static void printNumState(const char *prefix,
                          const pddl_strips_maker_t *sm,
                          const pddl_num_val_t *num_state)
{
    printf("%s", prefix);
    int size = pddlStripsMakerNonStaticFluentSize(sm);
    int offset = pddlStripsMakerNonStaticFluentOffset(sm);
    char buf[128];
    for (int i = 0; i < size; ++i){
        const pddl_ground_atom_t *ga = sm->fluent.atom[offset + i];
        printf(" (%s", C.pddl.func.pred[ga->pred].name);
        for (int j = 0; j < ga->arity; ++j)
            printf(" %s", C.pddl.obj.obj[ga->arg[j]].name);
        printf(") = %s", pddlNumValFmt(num_state + i, buf, sizeof(buf)));
    }
    printf("\n");
}

/** Formats PREFIX followed by "(func obj ...) old -> new" for every
 *  non-static fluent whose value in NUM_EFF differs from its value in
 *  NUM_STATE (in the numeric-state index order), and a terminating
 *  newline. */
static void sformatNumEff(char *buf, int buf_size, const char *prefix,
                          const pddl_strips_maker_t *sm,
                          const pddl_num_val_t *num_state,
                          const pddl_num_val_t *num_eff)
{
    int w = snprintf(buf, buf_size, "%s", prefix);
    int size = pddlStripsMakerNonStaticFluentSize(sm);
    int offset = pddlStripsMakerNonStaticFluentOffset(sm);
    char buf1[128], buf2[128];
    for (int i = 0; i < size; ++i){
        if (pddlNumValCmp(num_state + i, num_eff + i) == 0)
            continue;
        const pddl_ground_atom_t *ga = sm->fluent.atom[offset + i];
        w += snprintf(buf + w, buf_size - w, " (%s",
                      C.pddl.func.pred[ga->pred].name);
        for (int j = 0; j < ga->arity; ++j){
            w += snprintf(buf + w, buf_size - w, " %s",
                          C.pddl.obj.obj[ga->arg[j]].name);
        }
        w += snprintf(buf + w, buf_size - w, ") %s -> %s",
                      pddlNumValFmt(num_state + i, buf1, sizeof(buf1)),
                      pddlNumValFmt(num_eff + i, buf2, sizeof(buf2)));
    }
    snprintf(buf + w, buf_size - w, "\n");
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
    pddlStripsMakerAddInitAndCollect(sm, init, NULL);

    // Numeric state of the walk -- NULL if the task has no non-static
    // fluents
    int num_state_size = pddlStripsMakerNonStaticFluentSize(sm);
    pddl_num_val_t *num_state = NULL;
    pddl_num_val_t *next_num_state = NULL;
    if (num_state_size > 0){
        num_state = calloc(num_state_size, sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(sm, num_state);
        next_num_state = calloc(num_state_size, sizeof(pddl_num_val_t));
        memcpy(next_num_state, num_state,
               sizeof(pddl_num_val_t) * num_state_size);
    }

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

        // The successor generator ignores numeric conditions, so filter
        // out the groundings whose numeric precondition is not satisfied:
        // app_action[0..size-1] are the indexes of the truly applicable
        // groundings
        int app_size = pddlLiftedAppActionSize(aa);
        int *app_action = calloc(app_size > 0 ? app_size : 1, sizeof(int));
        int size = 0;
        for (int i = 0; i < app_size; ++i){
            int aid = pddlLiftedAppActionId(aa, i);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlLiftedAppActionArgs(aa, i);
            int sat = pddlStripsMakerIsNumCondSatisfied(sm, action->pre,
                                                        num_state, args,
                                                        &C.err);
            assert(sat == 0 || sat == 1);
            if (sat)
                app_action[size++] = i;
        }

        if (print_limit >= 0){
            printf("step %d: state %d applicable %d\n",
                   step, pddlISetSize(&state), size);
            printState("    state:", &state, sm);
            if (num_state_size > 0)
                printNumState("    num:", sm, num_state);
        }
        if (size == 0){
            free(app_action);
            break;
        }

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
                int aid = pddlLiftedAppActionId(aa, app_action[i]);
                const pddl_action_t *action = C.pddl.action.action + aid;
                const int *args = pddlLiftedAppActionArgs(aa, app_action[i]);
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
        pddl_strips_maker_eff_t eff = PDDL_STRIPS_MAKER_EFF_INIT;
        for (int i = 0; i < size; ++i){
            int aid = pddlLiftedAppActionId(aa, app_action[i]);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlLiftedAppActionArgs(aa, app_action[i]);

            ret = pddlStripsMakerActionEffInState(sm, action, args, &state,
                                                  num_state, &eff, &C.err);
            assert(ret == 0);
            // The documented normalization of the returned effects:
            // del_eff is restricted to the state, add and delete effects
            // are disjoint, and add_eff contains only new facts
            assert(pddlISetIsDisjoint(&eff.add_eff, &eff.del_eff));
            assert(pddlISetIsSubset(&eff.del_eff, &state));
            assert(pddlISetIsDisjoint(&eff.add_eff, &state));
            // Every returned ID is a valid ground atom stored in SM
            PDDL_ISET_FOR_EACH(&eff.add_eff, fid)
                assert(pddlStripsMakerGroundAtomConst(sm, fid)->id == fid);
            PDDL_ISET_FOR_EACH(&eff.del_eff, fid)
                assert(pddlStripsMakerGroundAtomConst(sm, fid)->id == fid);
            // Format the cost according to its type; for an integer
            // action cost the output matches the former %d formatting
            char cost_s[128];
            switch (eff.cost_type){
                case PDDL_STRIPS_MAKER_EFF_INT_ACTION_COST:
                    assert(eff.cost.int_action_cost >= 0);
                    snprintf(cost_s, sizeof(cost_s), "%d",
                             eff.cost.int_action_cost);
                    break;
                case PDDL_STRIPS_MAKER_EFF_GENERAL_ACTION_COST:
                    pddlNumValFmt(&eff.cost.general_action_cost,
                                  cost_s, sizeof(cost_s));
                    break;
                case PDDL_STRIPS_MAKER_EFF_STATE_METRIC:
                    pddlNumValFmt(&eff.cost.state_metric,
                                  cost_s, sizeof(cost_s));
                    break;
            }

            pddlStripsMakerAddAction(sm, aid, 0, args, NULL);

            if (print_rank != NULL && print_rank[i] >= 0){
                char *b = block[print_rank[i]];
                int w = snprintf(b, BLOCK_SIZE, "%s :: cost: %s%s\n",
                                 aname[i], cost_s,
                                 i == step % size ? " [successor]" : "");
                w += sformatState(b + w, BLOCK_SIZE - w, "    add:",
                                  &eff.add_eff, sm);
                w += sformatState(b + w, BLOCK_SIZE - w, "    del:",
                                  &eff.del_eff, sm);
                if (num_state_size > 0){
                    sformatNumEff(b + w, BLOCK_SIZE - w, "    num:",
                                  sm, num_state, eff.num_eff);
                }
            }

            if (i == step % size){
                pddlISetMinus2(&next_state, &state, &eff.del_eff);
                pddlISetUnion(&next_state, &eff.add_eff);
                if (num_state_size > 0){
                    memcpy(next_num_state, eff.num_eff,
                           sizeof(pddl_num_val_t) * num_state_size);
                }
            }
        }
        pddlStripsMakerEffFree(&eff);
        free(app_action);
        if (print_limit >= 0){
            for (int r = 0; r < num_print; ++r)
                printf("%s", block[r]);
            // If the action generating the successor state is not among
            // the printed ones, name it explicitly
            if (print_rank[step % size] < 0)
                printf("    successor: %s\n", aname[step % size]);
            free(block);
            free(print_rank);
            free(aname);
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        if (num_state_size > 0){
            memcpy(num_state, next_num_state,
                   sizeof(pddl_num_val_t) * num_state_size);
        }
    }
    pddlISetFree(&state);
    if (num_state != NULL)
        free(num_state);
    if (next_num_state != NULL)
        free(next_num_state);
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
    // The numeric part plays no role in the closure -- the initial
    // numeric state is used throughout
    int num_state_size = pddlStripsMakerNonStaticFluentSize(sm);
    pddl_num_val_t *num_state = NULL;
    if (num_state_size > 0){
        num_state = calloc(num_state_size, sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(sm, num_state);
    }

    int prev_size = -1;
    while (prev_size != sm->ground_atom.atom_size){
        prev_size = sm->ground_atom.atom_size;

        PDDL_ISET(state);
        for (int i = 0; i < sm->ground_atom.atom_size; ++i)
            pddlISetAdd(&state, i);

        pddl_strips_maker_eff_t eff = PDDL_STRIPS_MAKER_EFF_INIT;
        for (int i = 0; i < sm->num_action_args; ++i){
            const pddl_ground_action_args_t *ga;
            ga = pddlStripsMakerActionArgs(sm, i);
            const pddl_action_t *action
                    = C.pddl.action.action + ga->action_id;

            int ret = pddlStripsMakerActionEffInState(sm, action, ga->arg,
                                                      &state, num_state,
                                                      &eff, &C.err);
            assert(ret == 0);
        }
        pddlStripsMakerEffFree(&eff);
        pddlISetFree(&state);
    }

    if (num_state != NULL)
        free(num_state);
}

TEST(strips_maker, pddl)
{
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
    int ret = pddlStripsMakerAddInitAndCollect(&sm, &facts, &static_facts);
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
                = pddlGroundAtomsFindAtom(&sm.fluent, fluent, NULL);
        assert(ga != NULL);
        const pddl_fluent_data_t *fd
                = pddlExtArrGet(sm.fluent_data, ga->id);
        assert(pddlNumValCmp(&fd->init_val, &val) == 0);
        ++num_fluents;
    }
    assert(num_fluents == sm.fluent.atom_size);

    // The fluents are ordered by their types: static fluents first, then
    // the action-cost fluent, then the non-static fluents
    for (int i = 0; i < sm.fluent.atom_size; ++i){
        const pddl_fluent_data_t *fd = pddlExtArrGet(sm.fluent_data, i);
        if (i < sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_STATIC);
        }else if (sm.has_action_cost_fluent && i == sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_ACTION_COST);
        }else{
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_NON_STATIC);
        }
    }
    assert(pddlStripsMakerNonStaticFluentOffset(&sm)
                == sm.num_static_fluent + sm.has_action_cost_fluent);
    assert(pddlStripsMakerNonStaticFluentOffset(&sm)
                    + pddlStripsMakerNonStaticFluentSize(&sm)
                == sm.fluent.atom_size);

    // The initial numeric state matches the initial values of the
    // non-static fluents
    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    if (num_state_size > 0){
        pddl_num_val_t *num_state = calloc(num_state_size,
                                           sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(&sm, num_state);
        int offset = pddlStripsMakerNonStaticFluentOffset(&sm);
        for (int i = 0; i < num_state_size; ++i){
            const pddl_fluent_data_t *fd
                    = pddlExtArrGet(sm.fluent_data, offset + i);
            assert(pddlNumValCmp(num_state + i, &fd->init_val) == 0);
        }
        free(num_state);
    }

    // Plain AddInit collects exactly the same sets
    pddl_strips_maker_t sm2;
    pddlStripsMakerInit(&sm2, &C.pddl);
    ret = pddlStripsMakerAddInit(&sm2);
    assert(ret == 0);
    assert(sm2.ground_atom.atom_size == sm.ground_atom.atom_size);
    assert(sm2.ground_atom_static.atom_size
                == sm.ground_atom_static.atom_size);
    assert(sm2.fluent.atom_size == sm.fluent.atom_size);
    pddlStripsMakerFree(&sm2);

    printf("init atoms: %d static: %d fluents: %d\n",
           sm.ground_atom.atom_size, sm.ground_atom_static.atom_size,
           sm.fluent.atom_size);

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

    // Translation of a numeric task to STRIPS is not supported -- every
    // variant below would be rejected the same way
    if (pddlIsNumeric(&C.pddl)){
        pddl_ground_config_t ncfg = PDDL_GROUND_CONFIG_INIT;
        pddl_strips_t ns;
        pddl_err_t err = PDDL_ERR_INIT;
        int nret = pddlStripsMakerMakeStrips(&sm, &ncfg, &ns, &err);
        assert(nret == -1);
        printf("numeric task: make-strips rejected\n");
        pddlISetFree(&goal);
        pddlISetFree(&init_facts);
        pddlStripsMakerFree(&sm);
        TEST_SKIP_CHILDREN;
        return;
    }

    // Variant A: keep static facts as they are
    pddl_ground_config_t cfg = PDDL_GROUND_CONFIG_INIT;
    cfg.remove_static_facts = pddl_false;
    pddl_strips_t s0;
    int ret = pddlStripsMakerMakeStrips(&sm, &cfg, &s0, &C.err);
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
        if (pddlIsUnitCost(&C.pddl))
            assert(op->cost == 1 || op->is_aux_remove_from_plan);
        assert(op->cost >= 0);
        assert(op->action_args_size == 0);
    }

    // Variant B: keep also all static facts
    cfg.keep_all_static_facts = pddl_true;
    pddl_strips_t s1;
    ret = pddlStripsMakerMakeStrips(&sm, &cfg, &s1, &C.err);
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
    ret = pddlStripsMakerMakeStrips(&sm, &cfg, &s2, &C.err);
    assert(ret == 0);
    assert(s2.fact.fact_size <= s0.fact.fact_size);
    printf("C: facts %d ops %d\n", s2.fact.fact_size, s2.op.op_size);

    // Variant D: keep action arguments in the operators
    cfg.remove_static_facts = pddl_false;
    cfg.keep_action_args = pddl_true;
    pddl_strips_t s3;
    ret = pddlStripsMakerMakeStrips(&sm, &cfg, &s3, &C.err);
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
        ret = pddlStripsMakerMakeStrips(&sm, &cfg, &s4, &C.err);
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

// The numeric counterpart of the strips_maker test tree: runs only on
// numeric tasks and checks the fluent registry and the initial numeric
// state
TEST(strips_maker_numeric, pddl)
{
    if (!pddlIsNumeric(&C.pddl))
        return;

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &C.pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    // The fluents are ordered by their types
    for (int i = 0; i < sm.fluent.atom_size; ++i){
        const pddl_fluent_data_t *fd = pddlExtArrGet(sm.fluent_data, i);
        if (i < sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_STATIC);
        }else if (sm.has_action_cost_fluent && i == sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_ACTION_COST);
        }else{
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_NON_STATIC);
        }
    }

    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    int offset = pddlStripsMakerNonStaticFluentOffset(&sm);
    assert(offset + num_state_size == sm.fluent.atom_size);
    printf("fluents: %d static: %d cost: %d non-static: %d\n",
           sm.fluent.atom_size, sm.num_static_fluent,
           sm.has_action_cost_fluent, num_state_size);

    pddl_num_val_t *num_state = NULL;
    if (num_state_size > 0){
        num_state = calloc(num_state_size, sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(&sm, num_state);
        for (int i = 0; i < num_state_size; ++i){
            const pddl_fluent_data_t *fd
                    = pddlExtArrGet(sm.fluent_data, offset + i);
            assert(pddlNumValCmp(num_state + i, &fd->init_val) == 0);
        }
    }

    // Value of the metric expression in the initial numeric state --
    // unless the metric is the action-cost fluent, which must not be read
    // in a numeric expression
    if (!pddlIsUnitCost(&C.pddl) && pddlActionCostFuncId(&C.pddl) < 0){
        pddl_num_val_t val;
        ret = pddlStripsMakerEvalNumExp(&sm, C.pddl.minimize, num_state,
                                        NULL, &val, &C.err);
        assert(ret == 0);
        char buf[128];
        printf("init metric: %s\n", pddlNumValFmt(&val, buf, sizeof(buf)));
    }

    if (num_state != NULL)
        free(num_state);
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
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    printf("%s\n", header);
    int num = sm.fluent.atom_size;
    if (num > 0){
        char (*line)[LINE_SIZE] = calloc(num, LINE_SIZE);
        char buf[128];
        for (int i = 0; i < num; ++i){
            const pddl_ground_atom_t *ga = sm.fluent.atom[i];
            const pddl_fluent_data_t *fd
                    = pddlExtArrGet(sm.fluent_data, ga->id);
            char type = '?';
            switch (fd->type){
                case PDDL_STRIPS_MAKER_FLUENT_STATIC:
                    type = 'S';
                    break;
                case PDDL_STRIPS_MAKER_FLUENT_ACTION_COST:
                    type = 'C';
                    break;
                case PDDL_STRIPS_MAKER_FLUENT_NON_STATIC:
                    type = 'N';
                    break;
            }
            int w = snprintf(line[i], LINE_SIZE, "(%s",
                             pddl.func.pred[ga->pred].name);
            for (int j = 0; j < ga->arity; ++j){
                w += snprintf(line[i] + w, LINE_SIZE - w, " %s",
                              pddl.obj.obj[ga->arg[j]].name);
            }
            snprintf(line[i] + w, LINE_SIZE - w, ") = %s [%c]",
                     pddlNumValFmt(&fd->init_val, buf, sizeof(buf)), type);
        }
        pddlSort(line, num, LINE_SIZE, cmpLine, NULL);
        for (int i = 0; i < num; ++i)
            printf("%s\n", line[i]);
        free(line);
    }
    printf("static %d cost %d non-static %d\n",
           sm.num_static_fluent, sm.has_action_cost_fluent,
           pddlStripsMakerNonStaticFluentSize(&sm));

    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

TEST(strips_maker_once_fluents, strips_maker_once)
{
    dumpInitFluents("various/num-cost-expr/p01:",
                    "pddl/various/num-cost-expr/domain.pddl",
                    "pddl/various/num-cost-expr/p01.pddl");
    dumpInitFluents("various/num-float-coef/p01:",
                    "pddl/various/num-float-coef/domain.pddl",
                    "pddl/various/num-float-coef/p01.pddl");

    pddl_t pddl;
    loadPddl(&pddl, "pddl/various/num-cost-expr/domain.pddl",
             "pddl/various/num-cost-expr/p01.pddl");
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    // The fluents are stored in the order of their types
    for (int i = 0; i < sm.fluent.atom_size; ++i){
        const pddl_fluent_data_t *fd = pddlExtArrGet(sm.fluent_data, i);
        if (i < sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_STATIC);
        }else if (sm.has_action_cost_fluent && i == sm.num_static_fluent){
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_ACTION_COST);
        }else{
            assert(fd->type == PDDL_STRIPS_MAKER_FLUENT_NON_STATIC);
        }
    }

    // The initial numeric state matches the stored initial values
    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    int offset = pddlStripsMakerNonStaticFluentOffset(&sm);
    assert(offset + num_state_size == sm.fluent.atom_size);
    if (num_state_size > 0){
        pddl_num_val_t *num_state = calloc(num_state_size,
                                           sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(&sm, num_state);
        for (int i = 0; i < num_state_size; ++i){
            const pddl_fluent_data_t *fd
                    = pddlExtArrGet(sm.fluent_data, offset + i);
            assert(pddlNumValCmp(num_state + i, &fd->init_val) == 0);
        }
        free(num_state);
    }

    // A repeated AddInit is idempotent
    int num_fluents = sm.fluent.atom_size;
    int num_static_fluent = sm.num_static_fluent;
    int has_action_cost_fluent = sm.has_action_cost_fluent;
    ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);
    assert(sm.fluent.atom_size == num_fluents);
    assert(sm.num_static_fluent == num_static_fluent);
    assert(sm.has_action_cost_fluent == has_action_cost_fluent);

    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

/** Returns the ID of the function called NAME. */
static int findFunc(const pddl_t *pddl, const char *name)
{
    for (int i = 0; i < pddl->func.pred_size; ++i){
        if (strcmp(pddl->func.pred[i].name, name) == 0)
            return i;
    }
    assert(0);
    return -1;
}

/** Returns the ID of the object called NAME. */
static int findObj(const pddl_t *pddl, const char *name)
{
    for (int i = 0; i < pddl->obj.obj_size; ++i){
        if (strcmp(pddl->obj.obj[i].name, name) == 0)
            return i;
    }
    assert(0);
    return -1;
}

/** Creates a ground fluent atom (FUNC_ID obj), or a 0-ary (FUNC_ID) if
 *  OBJ is negative. */
static pddl_fm_atom_t *fluentAtom(int func_id, int obj)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(obj >= 0 ? 1 : 0);
    a->pred = func_id;
    if (obj >= 0)
        a->arg[0].obj = obj;
    return a;
}

static void evalMinimize(const char *header,
                         const char *domain_fn,
                         const char *problem_fn)
{
    pddl_t pddl;
    loadPddl(&pddl, domain_fn, problem_fn);
    assert(!pddlIsUnitCost(&pddl));
    assert(pddl.minimize != NULL);

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    pddl_num_val_t *num_state = NULL;
    if (num_state_size > 0){
        num_state = calloc(num_state_size, sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(&sm, num_state);
    }

    pddl_num_val_t val;
    ret = pddlStripsMakerEvalNumExp(&sm, pddl.minimize, num_state, NULL,
                                    &val, &C.err);
    assert(ret == 0);

    // Cross-check against the init-state evaluator -- the numeric state
    // holds the initial values, so both must agree
    pddl_num_val_t chk;
    pddl_fm_num_eval_status_t st
            = pddlInitStateCheckNumExpValue(&pddl.init, pddl.minimize, &chk);
    assert(st == PDDL_FM_NUM_EVAL_OK);
    assert(pddlNumValCmp(&val, &chk) == 0);

    char buf[128];
    printf("%s minimize = %s\n", header,
           pddlNumValFmt(&val, buf, sizeof(buf)));

    if (num_state != NULL)
        free(num_state);
    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

TEST(strips_maker_once_eval_num_exp, strips_maker_once)
{
    evalMinimize("various/num-metric-nonlinear/p01:",
                 "pddl/various/num-metric-nonlinear/domain.pddl",
                 "pddl/various/num-metric-nonlinear/p01.pddl");
    evalMinimize("various/num-metric-negdelta/p01:",
                 "pddl/various/num-metric-negdelta/domain.pddl",
                 "pddl/various/num-metric-negdelta/p01.pddl");
}

/** Evaluates the numeric precondition of every grounding of every action
 *  of the counters task in NUM_STATE and prints per-action counts. */
static void evalCountersPre(pddl_strips_maker_t *sm,
                            const pddl_t *pddl,
                            const char *header,
                            const pddl_num_val_t *num_state)
{
    printf("%s\n", header);
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        const pddl_action_t *a = pddl->action.action + ai;
        assert(a->param.param_size == 1);
        int sat = 0, unsat = 0;
        for (int obj = 0; obj < pddl->obj.obj_size; ++obj){
            int args[1] = { obj };
            int ret = pddlStripsMakerIsNumCondSatisfied(
                            sm, a->pre, num_state, args, &C.err);
            assert(ret == 0 || ret == 1);
            if (ret == 1){
                ++sat;
            }else{
                ++unsat;
            }
        }
        printf("  (%s): satisfied %d unsatisfied %d\n", a->name, sat, unsat);
    }
}

TEST(strips_maker_once_eval_num_cmp, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/ipc-2023/num/counters/domain.pddl",
             "pddl/ipc-2023/num/counters/pfile1.pddl");

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    assert(num_state_size > 0);
    pddl_num_val_t *num_state = calloc(num_state_size,
                                       sizeof(pddl_num_val_t));
    pddlStripsMakerInitNumState(&sm, num_state);
    evalCountersPre(&sm, &pddl, "counters/pfile1 init:", num_state);

    // All counters at 0: (decrement) is inapplicable everywhere
    for (int i = 0; i < num_state_size; ++i)
        pddlNumValSetInt(num_state + i, 0);
    evalCountersPre(&sm, &pddl, "counters/pfile1 all-zero:", num_state);

    // All counters at (max_int) = 40: (increment) is inapplicable
    // everywhere
    for (int i = 0; i < num_state_size; ++i)
        pddlNumValSetInt(num_state + i, 40);
    evalCountersPre(&sm, &pddl, "counters/pfile1 all-max:", num_state);

    free(num_state);
    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

TEST(strips_maker_once_eval_num_op, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/ipc-2023/num/counters/domain.pddl",
             "pddl/ipc-2023/num/counters/pfile1.pddl");

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
    int offset = pddlStripsMakerNonStaticFluentOffset(&sm);
    assert(num_state_size > 0);
    pddl_num_val_t *num_state = calloc(num_state_size,
                                       sizeof(pddl_num_val_t));
    pddlStripsMakerInitNumState(&sm, num_state);

    // Evaluate the numeric-op effects of every action grounded with the
    // first three objects
    char buf1[128], buf2[128];
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        assert(a->param.param_size == 1);
        for (int obj = 0; obj < 3 && obj < pddl.obj.obj_size; ++obj){
            int args[1] = { obj };
            pddl_fm_const_it_t it;
            PDDL_FM_FOR_EACH(a->eff, &it, fm){
                if (!pddlFmIsNumOp(fm))
                    continue;
                int fluent_id;
                pddl_num_val_t val;
                ret = pddlStripsMakerEvalNumOp(&sm, pddlFmToNumOpConst(fm),
                                               num_state, args,
                                               &fluent_id, &val, &C.err);
                assert(ret == 0);
                assert(fluent_id >= offset);
                assert(fluent_id < sm.fluent.atom_size);
                int idx = pddlStripsMakerNumStateIndex(&sm, fluent_id);
                printf("(%s %s): (value %s) %s -> %s\n",
                       a->name, pddl.obj.obj[obj].name,
                       pddl.obj.obj[obj].name,
                       pddlNumValFmt(num_state + idx, buf1, sizeof(buf1)),
                       pddlNumValFmt(&val, buf2, sizeof(buf2)));
            }
        }
    }

    // Apply (increment c0) in the initial state with
    // pddlStripsMakerActionEffInState() -- the numeric effect must change
    // exactly the touched fluent and the counters task is unit-cost
    {
        int c0 = findObj(&pddl, "c0");
        pddl_action_t *inc = NULL;
        for (int ai = 0; ai < pddl.action.action_size; ++ai){
            if (strcmp(pddl.action.action[ai].name, "increment") == 0)
                inc = pddl.action.action + ai;
        }
        assert(inc != NULL);
        int args[1] = { c0 };

        const pddl_fm_num_op_t *op = NULL;
        pddl_fm_const_it_t it;
        PDDL_FM_FOR_EACH(inc->eff, &it, fm){
            if (pddlFmIsNumOp(fm))
                op = pddlFmToNumOpConst(fm);
        }
        assert(op != NULL);
        int fluent_id;
        pddl_num_val_t val;
        ret = pddlStripsMakerEvalNumOp(&sm, op, num_state, args,
                                       &fluent_id, &val, &C.err);
        assert(ret == 0);

        PDDL_ISET(state);
        pddl_strips_maker_eff_t eff = PDDL_STRIPS_MAKER_EFF_INIT;
        ret = pddlStripsMakerActionEffInState(&sm, inc, args, &state,
                                              num_state, &eff, &C.err);
        assert(ret == 0);
        assert(eff.cost_type == PDDL_STRIPS_MAKER_EFF_INT_ACTION_COST);
        assert(eff.cost.int_action_cost == 1);
        assert(eff.num_eff_size == num_state_size);
        assert(pddlISetSize(&eff.add_eff) == 0);
        assert(pddlISetSize(&eff.del_eff) == 0);
        // .num_eff is a copy of the input numeric state except for the
        // touched fluent
        int touched_idx = pddlStripsMakerNumStateIndex(&sm, fluent_id);
        for (int i = 0; i < num_state_size; ++i){
            if (i == touched_idx){
                assert(pddlNumValCmp(eff.num_eff + i, &val) == 0);
                assert(pddlNumValCmp(eff.num_eff + i, num_state + i) != 0);
            }else{
                assert(pddlNumValCmp(eff.num_eff + i, num_state + i) == 0);
            }
        }
        printf("eff(increment c0): cost %d (value c0) -> %s\n",
               eff.cost.int_action_cost,
               pddlNumValFmt(eff.num_eff + touched_idx,
                             buf1, sizeof(buf1)));

        // Multiple increases of the same fluent accumulate: extend the
        // effect of (increment ?c) with (increase (value ?c) 2), so the
        // overall change of (value c0) is +3
        int value_func = findFunc(&pddl, "value");
        pddl_fm_atom_t *lhs = pddlFmNewEmptyAtom(1);
        lhs->pred = value_func;
        lhs->arg[0].param = 0;
        pddl_fm_num_op_t *op2
                = pddlFmNewNumOpIncrease(lhs, pddlFmNewNumExpNumInt(2));
        pddlFmJuncAdd(pddlFmToJunc(inc->eff), &op2->fm);

        ret = pddlStripsMakerActionEffInState(&sm, inc, args, &state,
                                              num_state, &eff, &C.err);
        assert(ret == 0);
        assert(eff.cost_type == PDDL_STRIPS_MAKER_EFF_INT_ACTION_COST);
        assert(eff.cost.int_action_cost == 1);
        pddl_num_val_t exp_val;
        pddlNumValSet(&exp_val, num_state + touched_idx);
        pddl_num_val_t delta;
        pddlNumValSetInt(&delta, 3);
        pddl_num_val_status_t vst = pddlNumValAdd(&exp_val, &delta);
        assert(vst == PDDL_NUM_VAL_OK);
        assert(pddlNumValCmp(eff.num_eff + touched_idx, &exp_val) == 0);
        for (int i = 0; i < num_state_size; ++i){
            if (i != touched_idx)
                assert(pddlNumValCmp(eff.num_eff + i, num_state + i) == 0);
        }
        printf("eff(increment+2 c0): cost %d (value c0) -> %s\n",
               eff.cost.int_action_cost,
               pddlNumValFmt(eff.num_eff + touched_idx,
                             buf1, sizeof(buf1)));

        pddlStripsMakerEffFree(&eff);
        pddlISetFree(&state);
    }

    free(num_state);
    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

TEST(strips_maker_once_eval_err, strips_maker_once)
{
    // Unknown fluent: remove (value c0) from the initial state before
    // the strips maker is initialized -- both the expression and the
    // comparator evaluation must report an error (and the comparator
    // must not report a mere "unsatisfied")
    {
        pddl_t pddl;
        loadPddl(&pddl, "pddl/ipc-2023/num/counters/domain.pddl",
                 "pddl/ipc-2023/num/counters/pfile1.pddl");
        int value_func = findFunc(&pddl, "value");
        int c0 = findObj(&pddl, "c0");
        pddl_fm_atom_t *rm_atom = fluentAtom(value_func, c0);
        int ret = pddlInitStateRmFluent(&pddl.init, rm_atom);
        assert(ret == 0);
        pddlFmDel(&rm_atom->fm);

        pddl_strips_maker_t sm;
        pddlStripsMakerInit(&sm, &pddl);
        ret = pddlStripsMakerAddInit(&sm);
        assert(ret == 0);
        int num_state_size = pddlStripsMakerNonStaticFluentSize(&sm);
        pddl_num_val_t *num_state = calloc(num_state_size,
                                           sizeof(pddl_num_val_t));
        pddlStripsMakerInitNumState(&sm, num_state);

        pddl_err_t err = PDDL_ERR_INIT;
        pddl_fm_num_exp_t *e
                = pddlFmNewNumExpFluent(fluentAtom(value_func, c0));
        pddl_num_val_t val;
        ret = pddlStripsMakerEvalNumExp(&sm, e, num_state, NULL,
                                        &val, &err);
        assert(ret == -1);
        pddlFmDel(&e->fm);

        pddl_err_t err2 = PDDL_ERR_INIT;
        pddl_fm_num_cmp_t *cmp = pddlFmNewNumCmpGE(
                    pddlFmNewNumExpFluent(fluentAtom(value_func, c0)),
                    pddlFmNewNumExpNumInt(0));
        ret = pddlStripsMakerEvalNumCmp(&sm, cmp, num_state, NULL, &err2);
        assert(ret == -1);
        pddlFmDel(&cmp->fm);

        printf("unknown fluent: as expected\n");
        free(num_state);
        pddlStripsMakerFree(&sm);
        pddlFree(&pddl);
    }

    // The action-cost fluent must not be read in a numeric expression,
    // and a static fluent must not appear on the left hand side of a
    // numeric operation
    {
        pddl_t pddl;
        loadPddl(&pddl, "pddl/various/num-cost-expr/domain.pddl",
                 "pddl/various/num-cost-expr/p01.pddl");
        int cost_func = pddlActionCostFuncId(&pddl);
        assert(cost_func >= 0);

        pddl_strips_maker_t sm;
        pddlStripsMakerInit(&sm, &pddl);
        int ret = pddlStripsMakerAddInit(&sm);
        assert(ret == 0);
        assert(sm.has_action_cost_fluent);
        assert(pddlStripsMakerNonStaticFluentSize(&sm) == 0);

        pddl_err_t err = PDDL_ERR_INIT;
        pddl_fm_num_exp_t *e
                = pddlFmNewNumExpFluent(fluentAtom(cost_func, -1));
        pddl_num_val_t val;
        ret = pddlStripsMakerEvalNumExp(&sm, e, NULL, NULL, &val, &err);
        assert(ret == -1);
        pddlFmDel(&e->fm);
        printf("action-cost fluent in expression: as expected\n");

        pddl_err_t err2 = PDDL_ERR_INIT;
        int g_func = findFunc(&pddl, "g");
        int l1 = findObj(&pddl, "l1");
        pddl_fm_num_op_t *op = pddlFmNewNumOpAssign(
                    fluentAtom(g_func, l1), pddlFmNewNumExpNumInt(1));
        int fluent_id;
        ret = pddlStripsMakerEvalNumOp(&sm, op, NULL, NULL,
                                       &fluent_id, &val, &err2);
        assert(ret == -1);
        pddlFmDel(&op->fm);
        printf("static fluent on the left hand side: as expected\n");

        // Division by zero
        pddl_err_t err3 = PDDL_ERR_INIT;
        e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                               pddlFmNewNumExpNumInt(0));
        ret = pddlStripsMakerEvalNumExp(&sm, e, NULL, NULL, &val, &err3);
        assert(ret == -1);
        pddlFmDel(&e->fm);
        printf("division by zero: as expected\n");

        pddlStripsMakerFree(&sm);
        pddlFree(&pddl);
    }
}

// Translation of a numeric task to STRIPS is not supported
TEST(strips_maker_once_make_strips_numeric, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/ipc-2023/num/counters/domain.pddl",
             "pddl/ipc-2023/num/counters/pfile1.pddl");
    assert(pddlIsNumeric(&pddl));

    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);

    pddl_ground_config_t cfg = PDDL_GROUND_CONFIG_INIT;
    pddl_strips_t strips;
    pddl_err_t err = PDDL_ERR_INIT;
    ret = pddlStripsMakerMakeStrips(&sm, &cfg, &strips, &err);
    assert(ret == -1);
    printf("make-strips on a numeric task: as expected\n");

    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}

static void addInit(pddl_t *pddl)
{
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, pddl);
    pddlStripsMakerAddInit(&sm);
    pddlStripsMakerFree(&sm);
}

TEST(strips_maker_once_solvable_init, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/various/num-cost-renamed/domain.pddl",
             "pddl/various/num-cost-renamed/p01.pddl");

    // A solvable initial state does not panic
    addInit(&pddl);

    pddlFree(&pddl);
}

// AddInit panics on an initial state marked as unsolvable
TEST_PANIC_ONCE(strips_maker_unsolvable_init)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/various/num-cost-renamed/domain.pddl",
             "pddl/various/num-cost-renamed/p01.pddl");
    pddlInitStateSetUnsolvable(&pddl.init);
    addInit(&pddl);
}

// pddlStripsMakerActionEffInState() reports an error when a task with
// non-static fluents is given no numeric state
TEST(strips_maker_once_eff_no_num_state, strips_maker_once)
{
    pddl_t pddl;
    loadPddl(&pddl, "pddl/ipc-2023/num/counters/domain.pddl",
             "pddl/ipc-2023/num/counters/pfile1.pddl");
    pddl_strips_maker_t sm;
    pddlStripsMakerInit(&sm, &pddl);
    int ret = pddlStripsMakerAddInit(&sm);
    assert(ret == 0);
    assert(pddlStripsMakerNonStaticFluentSize(&sm) > 0);

    PDDL_ISET(state);
    pddl_strips_maker_eff_t eff = PDDL_STRIPS_MAKER_EFF_INIT;
    pddl_err_t err = PDDL_ERR_INIT;
    int args[1] = { 0 };
    ret = pddlStripsMakerActionEffInState(&sm, pddl.action.action, args,
                                          &state, NULL, &eff, &err);
    assert(ret == -1);
    printf("missing numeric state: as expected\n");

    pddlStripsMakerEffFree(&eff);
    pddlISetFree(&state);
    pddlStripsMakerFree(&sm);
    pddlFree(&pddl);
}
