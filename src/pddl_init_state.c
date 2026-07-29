/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the pddl_init_state_t initial state.
 *
 * All tests are TEST_ONCE (not per task): the initial state is
 * self-contained, so everything is built from atoms created directly with
 * pddlFmCreateFactAtom() and no PDDL files are needed.
 */

#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** Creates the grounded atom (PRED o1 ... ok) from the ARG_SIZE object IDs
 *  given as varargs. The caller frees it with delAtom(). */
static pddl_fm_atom_t *mkAtom(int pred, int arg_size, ...)
{
    int arg[8];
    va_list ap;
    assert(arg_size <= 8);
    va_start(ap, arg_size);
    for (int i = 0; i < arg_size; ++i)
        arg[i] = va_arg(ap, int);
    va_end(ap);
    return pddlFmCreateFactAtom(pred, arg_size, arg);
}

static void delAtom(pddl_fm_atom_t *a)
{
    pddlFmDel(&a->fm);
}

static pddl_num_val_t mkInt(int64_t v)
{
    pddl_num_val_t val;
    pddlNumValSetInt(&val, v);
    return val;
}

#define DUMP_LINE_SIZE 256

static int cmpLine(const void *a, const void *b, void *_)
{
    return strcmp(a, b);
}

/** Prints the whole content of the initial state without needing a
 *  pddl_t: predicates and functions are printed by their numeric IDs.
 *
 *  The lines are sorted, because the order in which the initial state
 *  stores its entries is unspecified. */
static void dump(const char *label, const pddl_init_state_t *is)
{
    printf("%s: unsolvable: %d, atoms: %d, fluents: %d\n",
           label, pddlInitStateIsUnsolvable(is),
           pddlInitStateAtomSize(is), pddlInitStateFluentSize(is));

    int line_size = pddlInitStateAtomSize(is) + pddlInitStateFluentSize(is);
    if (line_size == 0)
        return;

    char (*line)[DUMP_LINE_SIZE] = calloc(line_size, DUMP_LINE_SIZE);
    int li = 0;

    PDDL_INIT_STATE_FOR_EACH_ATOM(is, atom){
        int w = snprintf(line[li], DUMP_LINE_SIZE, "  atom: p%d(", atom->pred);
        for (int i = 0; i < atom->arg_size; ++i){
            w += snprintf(line[li] + w, DUMP_LINE_SIZE - w, "%s%d",
                          (i > 0 ? "," : ""), atom->arg[i].obj);
        }
        snprintf(line[li] + w, DUMP_LINE_SIZE - w, ")");
        ++li;
    }

    pddl_num_val_t val;
    char s[128];
    PDDL_INIT_STATE_FOR_EACH_FLUENT(is, fluent, &val){
        int w = snprintf(line[li], DUMP_LINE_SIZE, "  fluent: f%d(",
                         fluent->pred);
        for (int i = 0; i < fluent->arg_size; ++i){
            w += snprintf(line[li] + w, DUMP_LINE_SIZE - w, "%s%d",
                          (i > 0 ? "," : ""), fluent->arg[i].obj);
        }
        snprintf(line[li] + w, DUMP_LINE_SIZE - w, ") = %s",
                 pddlNumValFmt(&val, s, sizeof(s)));
        ++li;
    }
    assert(li == line_size);

    pddlSort(line, line_size, DUMP_LINE_SIZE, cmpLine, NULL);
    for (int i = 0; i < line_size; ++i)
        printf("%s\n", line[i]);
    free(line);
}

/** Recomputes the per-function count by iterating, so a drifting counter
 *  is caught. */
static int countFuncFluents(const pddl_init_state_t *is, int func_id)
{
    int num = 0;
    PDDL_INIT_STATE_FOR_EACH_FUNC_FLUENT(is, func_id, fluent, NULL){
        assert(fluent->pred == func_id);
        ++num;
    }
    return num;
}

/** Asserts that every per-function counter agrees with an explicit sweep
 *  and that the counters sum up to the array sizes. */
static void checkCounters(const pddl_init_state_t *is, int max_symbol)
{
    int atom_sum = 0;
    int fluent_sum = 0;
    for (int i = 0; i < max_symbol; ++i){
        int num_atoms = 0;
        PDDL_INIT_STATE_FOR_EACH_ATOM(is, atom){
            if (atom->pred == i)
                ++num_atoms;
        }
        assert(pddlInitStateHasPred(is, i) == (num_atoms > 0));
        atom_sum += num_atoms;

        int num_fluents = countFuncFluents(is, i);
        assert(pddlInitStateFuncFluentSize(is, i) == num_fluents);
        assert(pddlInitStateHasFunc(is, i) == (num_fluents > 0));
        fluent_sum += num_fluents;
    }
    assert(atom_sum == pddlInitStateAtomSize(is));
    assert(fluent_sum == pddlInitStateFluentSize(is));
}

TEST_ONCE(pddl_init_state_basic)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);
    assert(pddlInitStateAtomSize(&is) == 0);
    assert(pddlInitStateFluentSize(&is) == 0);
    assert(!pddlInitStateIsUnsolvable(&is));
    dump("empty", &is);

    int arg[2] = { 0, 1 };
    pddlInitStateAddAtomByPredArgs(&is, 0, 2, arg);
    pddl_num_val_t v = mkInt(7);
    pddl_fm_atom_t *f = mkAtom(1, 1, 3);
    pddlInitStateSetFluent(&is, f, &v);
    delAtom(f);
    dump("one-of-each", &is);

    pddlInitStateClear(&is);
    assert(pddlInitStateAtomSize(&is) == 0);
    assert(pddlInitStateFluentSize(&is) == 0);
    assert(!pddlInitStateHasPred(&is, 0));
    assert(!pddlInitStateHasFunc(&is, 1));
    dump("cleared", &is);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_atoms)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *a1 = mkAtom(0, 2, 0, 1);
    pddl_fm_atom_t *a2 = mkAtom(0, 2, 1, 0);
    pddl_fm_atom_t *a3 = mkAtom(2, 0);

    assert(pddlInitStateAddAtom(&is, a1) == 0);
    assert(pddlInitStateAddAtom(&is, a2) == 0);
    assert(pddlInitStateAddAtom(&is, a3) == 0);
    assert(pddlInitStateAtomSize(&is) == 3);

    // Adding a fact that is already there fails and changes nothing
    assert(pddlInitStateAddAtom(&is, a1) == -1);
    int arg[2] = { 0, 1 };
    assert(pddlInitStateAddAtomByPredArgs(&is, 0, 2, arg) == -1);
    assert(pddlInitStateAtomSize(&is) == 3);
    dump("three-atoms", &is);

    assert(pddlInitStateHasAtom(&is, a1));
    assert(pddlInitStateHasAtom(&is, a3));
    assert(pddlInitStateHasAtomPred(&is, 0, 2, arg));
    int other[2] = { 1, 1 };
    assert(!pddlInitStateHasAtomPred(&is, 0, 2, other));
    assert(!pddlInitStateHasAtomPred(&is, 1, 2, arg));

    // The .neg flag of the query is ignored
    pddl_fm_atom_t *neg = mkAtom(0, 2, 0, 1);
    neg->neg = pddl_true;
    assert(pddlInitStateHasAtom(&is, neg));

    // ... and so is the .neg flag of the atom being removed
    assert(pddlInitStateRmAtom(&is, neg) == 0);
    assert(!pddlInitStateHasAtom(&is, a1));
    assert(pddlInitStateAtomSize(&is) == 2);
    delAtom(neg);

    // Removing something that is not there fails and changes nothing
    assert(pddlInitStateRmAtom(&is, a1) == -1);
    assert(pddlInitStateAtomSize(&is) == 2);
    dump("after-rm", &is);
    checkCounters(&is, 4);

    delAtom(a1);
    delAtom(a2);
    delAtom(a3);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_partial_match)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *a = mkAtom(0, 2, 3, 4);
    pddlInitStateAddAtom(&is, a);
    delAtom(a);

    // (p 3 4) matches exactly
    pddl_fm_atom_t *q = mkAtom(0, 2, 3, 4);
    assert(pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);

    // (p 3 ?x) matches, the parameter is a wildcard
    q = mkAtom(0, 2, 3, 4);
    q->arg[1].obj = -1;
    q->arg[1].param = 0;
    assert(pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);

    // (p ?x 4) matches, the parameter is a wildcard
    q = mkAtom(0, 2, 3, 4);
    q->arg[0].obj = -1;
    q->arg[0].param = 0;
    assert(pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);

    // (p 9 ?x) does not match
    q = mkAtom(0, 2, 9, 4);
    q->arg[1].obj = -1;
    q->arg[1].param = 0;
    assert(!pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);

    // A different predicate and a different arity never match
    q = mkAtom(1, 2, 3, 4);
    assert(!pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);
    q = mkAtom(0, 1, 3);
    assert(!pddlInitStateHasAtomPartialMatch(&is, q));
    delAtom(q);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_fluents)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *f1 = mkAtom(0, 1, 5);
    pddl_fm_atom_t *f2 = mkAtom(1, 0);

    pddl_num_val_t v = mkInt(3);
    pddlInitStateSetFluent(&is, f1, &v);
    v = mkInt(-2);
    pddlInitStateSetFluent(&is, f2, &v);
    assert(pddlInitStateFluentSize(&is) == 2);
    dump("two-fluents", &is);

    // Setting an existing fluent replaces the value, it never appends
    v = mkInt(11);
    pddlInitStateSetFluent(&is, f1, &v);
    assert(pddlInitStateFluentSize(&is) == 2);

    pddl_num_val_t got;
    assert(pddlInitStateHasFluent(&is, f1));
    assert(pddlInitStateFluentVal(&is, f1, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 11);

    // AddNumCmp adds rather than replaces, so it reports the duplicate and
    // leaves the stored value alone
    pddl_fm_atom_t *a = mkAtom(0, 1, 5);
    pddl_fm_num_cmp_t *cmp
            = pddlFmNewNumCmpEq(pddlFmNewNumExpFluent(a),
                                pddlFmNewNumExpNumInt(42));
    assert(pddlInitStateAddNumCmp(&is, cmp) == -1);
    pddlFmDel(&cmp->fm);
    assert(pddlInitStateFluentSize(&is) == 2);
    assert(pddlInitStateFluentVal(&is, f1, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 11);
    dump("after-replace", &is);

    // A fluent that is not there
    pddl_fm_atom_t *f3 = mkAtom(0, 1, 6);
    assert(!pddlInitStateHasFluent(&is, f3));
    assert(pddlInitStateFluentVal(&is, f3, &got) == -1);

    assert(pddlInitStateRmFluent(&is, f1) == 0);
    assert(pddlInitStateFluentSize(&is) == 1);
    assert(!pddlInitStateHasFluent(&is, f1));
    // Removing something that is not there fails and changes nothing
    assert(pddlInitStateRmFluent(&is, f3) == -1);
    assert(pddlInitStateFluentSize(&is) == 1);
    dump("after-rm", &is);
    checkCounters(&is, 3);

    delAtom(f1);
    delAtom(f2);
    delAtom(f3);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_pred_func_cnt)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    // An ID that was never inserted, and one beyond the counter arrays
    assert(!pddlInitStateHasPred(&is, 0));
    assert(!pddlInitStateHasPred(&is, 1000));
    assert(!pddlInitStateHasFunc(&is, 1000));
    assert(pddlInitStateFuncFluentSize(&is, 1000) == 0);

    pddl_fm_atom_t *a1 = mkAtom(2, 1, 0);
    pddl_fm_atom_t *a2 = mkAtom(2, 1, 1);
    pddlInitStateAddAtom(&is, a1);
    pddlInitStateAddAtom(&is, a2);
    assert(pddlInitStateHasPred(&is, 2));
    assert(!pddlInitStateHasPred(&is, 1));

    // The predicate stays present until its last fact is gone
    pddlInitStateRmAtom(&is, a1);
    assert(pddlInitStateHasPred(&is, 2));
    pddlInitStateRmAtom(&is, a2);
    assert(!pddlInitStateHasPred(&is, 2));

    pddl_fm_atom_t *f1 = mkAtom(3, 1, 0);
    pddl_fm_atom_t *f2 = mkAtom(3, 1, 1);
    pddl_fm_atom_t *f3 = mkAtom(4, 0);
    pddl_num_val_t v = mkInt(1);
    pddlInitStateSetFluent(&is, f1, &v);
    pddlInitStateSetFluent(&is, f2, &v);
    pddlInitStateSetFluent(&is, f3, &v);
    assert(pddlInitStateFuncFluentSize(&is, 3) == 2);
    assert(pddlInitStateFuncFluentSize(&is, 4) == 1);
    assert(pddlInitStateFuncFluentSize(&is, 5) == 0);
    checkCounters(&is, 6);

    // Replacing a value must not change any count
    v = mkInt(9);
    pddlInitStateSetFluent(&is, f1, &v);
    assert(pddlInitStateFuncFluentSize(&is, 3) == 2);
    checkCounters(&is, 6);

    pddlInitStateRmFluent(&is, f1);
    pddlInitStateRmFluent(&is, f2);
    assert(!pddlInitStateHasFunc(&is, 3));
    assert(pddlInitStateHasFunc(&is, 4));
    checkCounters(&is, 6);

    delAtom(a1);
    delAtom(a2);
    delAtom(f1);
    delAtom(f2);
    delAtom(f3);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_iter)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    for (int i = 0; i < 4; ++i){
        pddl_fm_atom_t *a = mkAtom(0, 1, i);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }
    for (int i = 0; i < 3; ++i){
        pddl_fm_atom_t *f = mkAtom(i % 2, 1, i);
        pddl_num_val_t v = mkInt(i * 10);
        pddlInitStateSetFluent(&is, f, &v);
        delAtom(f);
    }

    int num = 0;
    PDDL_INIT_STATE_FOR_EACH_ATOM(&is, atom){
        assert(atom->pred == 0);
        ++num;
    }
    assert(num == 4);

    // Nested iteration over unordered pairs of facts
    printf("pairs:");
    PDDL_INIT_STATE_FOR_EACH_ATOM(&is, a1){
        PDDL_INIT_STATE_FOR_EACH_ATOM(&is, a2){
            if (pddlFmAtomCmp(a1, a2) < 0)
                printf(" (%d,%d)", a1->arg[0].obj, a2->arg[0].obj);
        }
    }
    printf("\n");

    // The value output parameter may be NULL
    num = 0;
    PDDL_INIT_STATE_FOR_EACH_FLUENT(&is, fluent, NULL){
        (void)fluent;
        ++num;
    }
    assert(num == 3);

    pddl_num_val_t val;
    char s[128];
    printf("fluents:");
    PDDL_INIT_STATE_FOR_EACH_FLUENT(&is, fluent, &val){
        printf(" f%d(%d)=%s", fluent->pred, fluent->arg[0].obj,
               pddlNumValFmt(&val, s, sizeof(s)));
    }
    printf("\n");

    printf("func 1:");
    PDDL_INIT_STATE_FOR_EACH_FUNC_FLUENT(&is, 1, fluent, &val){
        printf(" f%d(%d)=%s", fluent->pred, fluent->arg[0].obj,
               pddlNumValFmt(&val, s, sizeof(s)));
    }
    printf("\n");
    assert(countFuncFluents(&is, 0) == 2);
    assert(countFuncFluents(&is, 1) == 1);
    assert(countFuncFluents(&is, 7) == 0);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_num_exp)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *f = mkAtom(0, 1, 1);
    pddl_num_val_t v = mkInt(5);
    pddlInitStateSetFluent(&is, f, &v);
    delAtom(f);

    char s[128];
    pddl_num_val_t val;

    // (+ 2 (f 1)) is fully defined
    pddl_fm_num_exp_t *e
            = pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(2),
                                  pddlFmNewNumExpFluent(mkAtom(0, 1, 1)));
    pddl_fm_num_eval_status_t st = pddlInitStateCheckNumExpValue(&is, e, &val);
    printf("eval (+ 2 (f 1)): status: %d, val: %s\n",
           st, (st == PDDL_FM_NUM_EVAL_OK
                    ? pddlNumValFmt(&val, s, sizeof(s)) : "-"));
    assert(st == PDDL_FM_NUM_EVAL_OK);
    int num = pddlInitStateNumExpSubstFluents(&is, &e);
    printf("subst: %d replacements\n", num);
    assert(num == 1);
    st = pddlInitStateCheckNumExpValue(&is, e, &val);
    assert(st == PDDL_FM_NUM_EVAL_OK);
    printf("after subst: %s\n", pddlNumValFmt(&val, s, sizeof(s)));
    pddlFmDel(&e->fm);

    // (* (g 1) 3) references an undefined fluent
    e = pddlFmNewNumExpMult(pddlFmNewNumExpFluent(mkAtom(1, 1, 1)),
                            pddlFmNewNumExpNumInt(3));
    st = pddlInitStateCheckNumExpValue(&is, e, &val);
    printf("eval (* (g 1) 3): status: %d\n", st);
    assert(st == PDDL_FM_NUM_EVAL_UNDEF);
    num = pddlInitStateNumExpSubstFluents(&is, &e);
    printf("subst: %d replacements\n", num);
    assert(num == 0);
    pddlFmDel(&e->fm);

    // Division by zero is reported as such
    e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                           pddlFmNewNumExpNumInt(0));
    st = pddlInitStateCheckNumExpValue(&is, e, &val);
    printf("eval (/ 1 0): status: %d\n", st);
    assert(st == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    pddlFmDel(&e->fm);

    // A fluent that is the whole expression
    e = pddlFmNewNumExpFluent(mkAtom(0, 1, 1));
    num = pddlInitStateNumExpSubstFluents(&is, &e);
    assert(num == 1);
    assert(e->fm.type == PDDL_FM_NUM_EXP_NUM);
    printf("whole-expression subst: %s\n",
           pddlNumValFmt(&e->e.num, s, sizeof(s)));
    pddlFmDel(&e->fm);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_copy)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *a = mkAtom(0, 2, 1, 2);
    pddl_fm_atom_t *f = mkAtom(1, 0);
    pddl_num_val_t v = mkInt(4);
    pddlInitStateAddAtom(&is, a);
    pddlInitStateSetFluent(&is, f, &v);

    pddl_init_state_t copy;
    pddlInitStateInitCopy(&copy, &is);
    dump("copy", &copy);
    assert(pddlInitStateHasAtom(&copy, a));
    assert(pddlInitStateHasFluent(&copy, f));
    assert(pddlInitStateHasPred(&copy, 0));
    assert(pddlInitStateHasFunc(&copy, 1));
    checkCounters(&copy, 3);

    // The copy is independent of the source
    pddlInitStateClear(&is);
    assert(pddlInitStateAtomSize(&copy) == 1);
    assert(pddlInitStateFluentSize(&copy) == 1);
    dump("copy-after-source-cleared", &copy);
    pddlInitStateFree(&copy);

    // Copying an empty and an unsolvable initial state
    pddlInitStateInitCopy(&copy, &is);
    dump("copy-of-empty", &copy);
    pddlInitStateFree(&copy);

    pddlInitStateSetUnsolvable(&is);
    pddlInitStateInitCopy(&copy, &is);
    dump("copy-of-unsolvable", &copy);
    assert(pddlInitStateIsUnsolvable(&copy));
    pddlInitStateFree(&copy);

    delAtom(a);
    delAtom(f);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_unsolvable)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    pddl_fm_atom_t *a = mkAtom(0, 1, 1);
    pddl_fm_atom_t *f = mkAtom(1, 0);
    pddl_num_val_t v = mkInt(4);
    pddlInitStateAddAtom(&is, a);
    pddlInitStateSetFluent(&is, f, &v);

    pddlInitStateSetUnsolvable(&is);
    assert(pddlInitStateIsUnsolvable(&is));
    assert(pddlInitStateAtomSize(&is) == 0);
    assert(pddlInitStateFluentSize(&is) == 0);
    assert(!pddlInitStateHasPred(&is, 0));
    assert(!pddlInitStateHasFunc(&is, 1));
    dump("unsolvable", &is);

    // Clearing takes the flag back off
    pddlInitStateClear(&is);
    assert(!pddlInitStateIsUnsolvable(&is));
    dump("cleared", &is);

    delAtom(a);
    delAtom(f);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_remap_objs)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    for (int i = 0; i < 3; ++i){
        pddl_fm_atom_t *a = mkAtom(0, 1, i);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }
    pddl_fm_atom_t *f = mkAtom(1, 1, 2);
    pddl_num_val_t v = mkInt(8);
    pddlInitStateSetFluent(&is, f, &v);
    delAtom(f);
    dump("before", &is);

    // A plain permutation keeps everything
    int perm[3] = { 2, 1, 0 };
    pddlInitStateRemapObjs(&is, perm);
    dump("permuted", &is);
    assert(pddlInitStateAtomSize(&is) == 3);
    assert(pddlInitStateFluentSize(&is) == 1);
    checkCounters(&is, 3);

    // -1 drops every fact and fluent mentioning that object
    int rm[3] = { 0, -1, 1 };
    pddlInitStateRemapObjs(&is, rm);
    dump("after-removal", &is);
    assert(pddlInitStateAtomSize(&is) == 2);
    checkCounters(&is, 3);

    // A remap that makes two distinct facts identical merges them
    int merge[2] = { 0, 0 };
    pddlInitStateRemapObjs(&is, merge);
    dump("after-merge", &is);
    assert(pddlInitStateAtomSize(&is) == 1);
    checkCounters(&is, 3);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_remap_preds_funcs)
{
    int pred_remap[3] = { 2, -1, 0 };
    int func_remap[2] = { 1, -1 };

    pddl_init_state_t is;
    pddlInitStateInit(&is);
    for (int p = 0; p < 3; ++p){
        pddl_fm_atom_t *a = mkAtom(p, 1, 0);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }
    for (int f = 0; f < 2; ++f){
        pddl_fm_atom_t *fl = mkAtom(f, 1, 1);
        pddl_num_val_t v = mkInt(f);
        pddlInitStateSetFluent(&is, fl, &v);
        delAtom(fl);
    }
    dump("before", &is);

    // drop_removed: the entries of the removed symbols simply go away
    pddl_init_state_t drop;
    pddlInitStateInitCopy(&drop, &is);
    pddlInitStateRemapPredsFuncs(&drop, pred_remap, func_remap, pddl_true);
    dump("dropped", &drop);
    assert(!pddlInitStateIsUnsolvable(&drop));
    assert(pddlInitStateAtomSize(&drop) == 2);
    assert(pddlInitStateFluentSize(&drop) == 1);
    assert(pddlInitStateHasPred(&drop, 0));
    assert(pddlInitStateHasPred(&drop, 2));
    assert(!pddlInitStateHasPred(&drop, 1));
    assert(pddlInitStateHasFunc(&drop, 1));
    assert(!pddlInitStateHasFunc(&drop, 0));
    checkCounters(&drop, 4);
    pddlInitStateFree(&drop);

    // Otherwise a removed symbol makes the whole initial state false
    pddl_init_state_t unsolv;
    pddlInitStateInitCopy(&unsolv, &is);
    pddlInitStateRemapPredsFuncs(&unsolv, pred_remap, func_remap, pddl_false);
    dump("unsolvable", &unsolv);
    assert(pddlInitStateIsUnsolvable(&unsolv));
    pddlInitStateFree(&unsolv);

    // ... but a remap that removes nothing is applied as usual
    int keep_pred[3] = { 2, 1, 0 };
    int keep_func[2] = { 1, 0 };
    pddl_init_state_t keep;
    pddlInitStateInitCopy(&keep, &is);
    pddlInitStateRemapPredsFuncs(&keep, keep_pred, keep_func, pddl_false);
    dump("kept", &keep);
    assert(!pddlInitStateIsUnsolvable(&keep));
    assert(pddlInitStateAtomSize(&keep) == 3);
    checkCounters(&keep, 4);
    pddlInitStateFree(&keep);

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_pred_func_flags)
{
    pddl_preds_t preds;
    pddl_preds_t funcs;
    pddlPredsInitEmpty(&preds);
    pddlPredsInitEmpty(&funcs);
    for (int i = 0; i < 3; ++i){
        pddl_pred_t *p = pddlPredsAdd(&preds);
        pddlPredSetName(p, "p");
        // Start from the wrong value so that clearing is tested too
        p->in_init = pddl_true;
        pddl_pred_t *f = pddlPredsAdd(&funcs);
        pddlPredSetName(f, "f");
        f->in_init = pddl_true;
    }

    pddl_init_state_t is;
    pddlInitStateInit(&is);
    pddl_fm_atom_t *a = mkAtom(1, 1, 0);
    pddlInitStateAddAtom(&is, a);
    delAtom(a);
    pddl_fm_atom_t *fl = mkAtom(2, 0);
    pddl_num_val_t v = mkInt(0);
    pddlInitStateSetFluent(&is, fl, &v);
    delAtom(fl);

    pddlInitStateSetPredFuncFlags(&is, &preds, &funcs);
    for (int i = 0; i < preds.pred_size; ++i)
        printf("pred %d: in-init: %d\n", i, preds.pred[i].in_init);
    for (int i = 0; i < funcs.pred_size; ++i)
        printf("func %d: in-init: %d\n", i, funcs.pred[i].in_init);
    assert(!preds.pred[0].in_init);
    assert(preds.pred[1].in_init);
    assert(!preds.pred[2].in_init);
    assert(!funcs.pred[0].in_init);
    assert(!funcs.pred[1].in_init);
    assert(funcs.pred[2].in_init);

    pddlInitStateFree(&is);
    pddlPredsFree(&preds);
    pddlPredsFree(&funcs);
}

/** Builds a minimal pddl_t holding only the names needed for printing:
 *  predicates p, q (q is non-static), functions f, g, and objects a, b. */
static void mkPddl(pddl_t *pddl)
{
    memset(pddl, 0, sizeof(*pddl));
    pddlPredsInitEmpty(&pddl->pred);
    pddlPredsInitEmpty(&pddl->func);
    pddlObjsInit(&pddl->obj);

    pddlPredSetName(pddlPredsAdd(&pddl->pred), "p");
    pddl_pred_t *q = pddlPredsAdd(&pddl->pred);
    pddlPredSetName(q, "q");
    // A predicate that some action writes is not static
    q->write = pddl_true;

    pddlPredSetName(pddlPredsAdd(&pddl->func), "f");
    pddlPredSetName(pddlPredsAdd(&pddl->func), "g");

    pddlObjsAdd(&pddl->obj, "a");
    pddlObjsAdd(&pddl->obj, "b");
}

static void freePddl(pddl_t *pddl)
{
    pddlPredsFree(&pddl->pred);
    pddlPredsFree(&pddl->func);
    pddlObjsFree(&pddl->obj);
}

TEST_ONCE(pddl_init_state_print)
{
    pddl_t pddl;
    mkPddl(&pddl);

    pddl_init_state_t is;
    pddlInitStateInit(&is);

    // Inserted out of order, so that the sorting of the debug output is
    // visible: q is non-static and must be printed after the static p
    pddl_fm_atom_t *a = mkAtom(1, 1, 1);
    pddlInitStateAddAtom(&is, a);
    delAtom(a);
    a = mkAtom(0, 2, 1, 0);
    pddlInitStateAddAtom(&is, a);
    delAtom(a);
    a = mkAtom(0, 2, 0, 1);
    pddlInitStateAddAtom(&is, a);
    delAtom(a);

    pddl_fm_atom_t *f = mkAtom(1, 0);
    pddl_num_val_t v = mkInt(2);
    pddlInitStateSetFluent(&is, f, &v);
    delAtom(f);
    f = mkAtom(0, 1, 0);
    v = mkInt(-1);
    pddlInitStateSetFluent(&is, f, &v);
    delAtom(f);

    printf("--- PDDL ---\n");
    pddlInitStatePrintPDDL(&is, &pddl, stdout);
    printf("--- debug ---\n");
    pddlInitStatePrintDebug(&is, &pddl, stdout);

    printf("--- empty ---\n");
    pddl_init_state_t empty;
    pddlInitStateInit(&empty);
    pddlInitStatePrintPDDL(&empty, &pddl, stdout);
    pddlInitStatePrintDebug(&empty, &pddl, stdout);
    pddlInitStateFree(&empty);

    pddlInitStateFree(&is);
    freePddl(&pddl);
}

/** Encodes the I'th fact of the bulk population below: predicate i % 5 with
 *  two object arguments. Distinct I always give distinct facts. */
static pddl_fm_atom_t *bulkAtom(int i)
{
    return mkAtom(i % 5, 2, i / 5, i % 7);
}

TEST_ONCE(pddl_init_state_bulk)
{
    const int size = 400;

    pddl_init_state_t is;
    pddlInitStateInit(&is);

    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);

        pddl_fm_atom_t *f = mkAtom(i % 3, 1, i);
        pddl_num_val_t v = mkInt(i);
        pddlInitStateSetFluent(&is, f, &v);
        delAtom(f);
    }
    assert(pddlInitStateAtomSize(&is) == size);
    assert(pddlInitStateFluentSize(&is) == size);
    checkCounters(&is, 8);

    // Every entry must still be found once the table has grown
    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        assert(pddlInitStateHasAtom(&is, a));
        delAtom(a);

        pddl_fm_atom_t *f = mkAtom(i % 3, 1, i);
        pddl_num_val_t got;
        assert(pddlInitStateFluentVal(&is, f, &got) == 0);
        assert(pddlNumValIsInt(&got) && got.v.i == i);
        delAtom(f);
    }

    // Re-adding every fact must be a no-op
    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }
    assert(pddlInitStateAtomSize(&is) == size);

    printf("bulk: atoms: %d, fluents: %d\n",
           pddlInitStateAtomSize(&is), pddlInitStateFluentSize(&is));
    for (int p = 0; p < 5; ++p)
        printf("  pred %d: has: %d\n", p, pddlInitStateHasPred(&is, p));
    for (int f = 0; f < 3; ++f)
        printf("  func %d: fluents: %d\n", f, pddlInitStateFuncFluentSize(&is, f));

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_rm_from_middle)
{
    const int size = 100;

    pddl_init_state_t is;
    pddlInitStateInit(&is);
    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }

    // Remove every third fact. Removal moves the last entry into the freed
    // slot, so this is what catches a stale .idx or a stale hash table hook
    int removed = 0;
    for (int i = 0; i < size; i += 3){
        pddl_fm_atom_t *a = bulkAtom(i);
        pddlInitStateRmAtom(&is, a);
        delAtom(a);
        ++removed;
    }
    assert(pddlInitStateAtomSize(&is) == size - removed);
    checkCounters(&is, 8);

    // Every surviving fact must still be findable, and every removed one
    // must be gone
    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        assert(pddlInitStateHasAtom(&is, a) == (i % 3 != 0));
        delAtom(a);
    }

    // Putting them back must restore the original content exactly once each
    for (int i = 0; i < size; i += 3){
        pddl_fm_atom_t *a = bulkAtom(i);
        pddlInitStateAddAtom(&is, a);
        pddlInitStateAddAtom(&is, a);
        delAtom(a);
    }
    assert(pddlInitStateAtomSize(&is) == size);
    for (int i = 0; i < size; ++i){
        pddl_fm_atom_t *a = bulkAtom(i);
        assert(pddlInitStateHasAtom(&is, a));
        delAtom(a);
    }
    checkCounters(&is, 8);
    printf("rm-from-middle: removed %d, final size %d\n",
           removed, pddlInitStateAtomSize(&is));

    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_key_paths_agree)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    // Both entry points must agree for every arity, including 0
    int arg[3] = { 4, 5, 6 };
    for (int arity = 0; arity <= 3; ++arity){
        pddlInitStateAddAtomByPredArgs(&is, arity, arity, arg);

        pddl_fm_atom_t *a = pddlFmCreateFactAtom(arity, arity, arg);
        assert(pddlInitStateHasAtom(&is, a));
        assert(pddlInitStateHasAtomPred(&is, arity, arity, arg));
        // And a duplicate add through the other entry point is a no-op
        int before = pddlInitStateAtomSize(&is);
        pddlInitStateAddAtom(&is, a);
        assert(pddlInitStateAtomSize(&is) == before);
        delAtom(a);
    }
    assert(pddlInitStateAtomSize(&is) == 4);

    // A fact that is not there must miss through both entry points
    int missing[1] = { 99 };
    pddl_fm_atom_t *a = pddlFmCreateFactAtom(1, 1, missing);
    assert(!pddlInitStateHasAtom(&is, a));
    assert(!pddlInitStateHasAtomPred(&is, 1, 1, missing));
    delAtom(a);

    // A negated query still finds the stored positive fact
    a = pddlFmCreateFactAtom(2, 2, arg);
    a->neg = pddl_true;
    assert(pddlInitStateHasAtom(&is, a));
    delAtom(a);

    dump("key-paths", &is);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_distinct_keys)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    // Same objects in a different order, and the same objects under a
    // different predicate, must stay distinct entries
    int ab[2] = { 0, 1 };
    int ba[2] = { 1, 0 };
    pddlInitStateAddAtomByPredArgs(&is, 0, 2, ab);
    pddlInitStateAddAtomByPredArgs(&is, 0, 2, ba);
    pddlInitStateAddAtomByPredArgs(&is, 1, 2, ab);
    assert(pddlInitStateAtomSize(&is) == 3);
    assert(pddlInitStateHasAtomPred(&is, 0, 2, ab));
    assert(pddlInitStateHasAtomPred(&is, 0, 2, ba));
    assert(pddlInitStateHasAtomPred(&is, 1, 2, ab));
    assert(!pddlInitStateHasAtomPred(&is, 1, 2, ba));

    // Removing one must not disturb the others
    pddl_fm_atom_t *a = pddlFmCreateFactAtom(0, 2, ab);
    pddlInitStateRmAtom(&is, a);
    delAtom(a);
    assert(pddlInitStateAtomSize(&is) == 2);
    assert(!pddlInitStateHasAtomPred(&is, 0, 2, ab));
    assert(pddlInitStateHasAtomPred(&is, 0, 2, ba));
    assert(pddlInitStateHasAtomPred(&is, 1, 2, ab));
    checkCounters(&is, 3);

    dump("distinct-keys", &is);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_return_codes)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    int arg[2] = { 7, 8 };
    pddl_fm_atom_t *a = mkAtom(0, 2, 7, 8);
    pddl_num_val_t v1 = mkInt(1);
    pddl_num_val_t v2 = mkInt(2);
    pddl_fm_atom_t *f = mkAtom(0, 1, 9);

    // Removing from an empty initial state fails
    assert(pddlInitStateRmAtom(&is, a) == -1);
    assert(pddlInitStateRmFluent(&is, f) == -1);

    // First add succeeds, second reports the duplicate
    assert(pddlInitStateAddAtom(&is, a) == 0);
    assert(pddlInitStateAddAtom(&is, a) == -1);
    assert(pddlInitStateAddAtomByPredArgs(&is, 0, 2, arg) == -1);
    assert(pddlInitStateAtomSize(&is) == 1);

    assert(pddlInitStateAddFluent(&is, f, &v1) == 0);
    assert(pddlInitStateAddFluent(&is, f, &v2) == -1);
    assert(pddlInitStateFluentSize(&is) == 1);

    // A failed AddFluent must not have touched the stored value
    pddl_num_val_t got;
    assert(pddlInitStateFluentVal(&is, f, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 1);

    // SetFluent replaces instead, reporting 1 because the fluent was
    // already there; inserting a fresh one reports 0
    assert(pddlInitStateSetFluent(&is, f, &v2) == 1);
    assert(pddlInitStateFluentSize(&is) == 1);
    pddl_fm_atom_t *f2 = mkAtom(0, 1, 10);
    assert(pddlInitStateSetFluent(&is, f2, &v1) == 0);
    assert(pddlInitStateSetFluent(&is, f2, &v2) == 1);
    assert(pddlInitStateFluentSize(&is) == 2);
    assert(pddlInitStateRmFluent(&is, f2) == 0);
    delAtom(f2);
    assert(pddlInitStateFluentSize(&is) == 1);
    assert(pddlInitStateFluentVal(&is, f, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 2);

    // AddNumCmp is AddFluent with the fluent and the value packaged as a
    // comparator, so it reports the duplicate the same way
    pddl_fm_num_cmp_t *cmp
            = pddlFmNewNumCmpEq(pddlFmNewNumExpFluent(mkAtom(0, 1, 9)),
                                pddlFmNewNumExpNumInt(3));
    assert(pddlInitStateAddNumCmp(&is, cmp) == -1);
    pddlFmDel(&cmp->fm);
    assert(pddlInitStateFluentVal(&is, f, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 2);

    cmp = pddlFmNewNumCmpEq(pddlFmNewNumExpFluent(mkAtom(1, 0)),
                            pddlFmNewNumExpNumInt(4));
    assert(pddlInitStateAddNumCmp(&is, cmp) == 0);
    pddlFmDel(&cmp->fm);
    assert(pddlInitStateFluentSize(&is) == 2);

    // Removal succeeds once, then reports that it is gone
    assert(pddlInitStateRmAtom(&is, a) == 0);
    assert(pddlInitStateRmAtom(&is, a) == -1);
    assert(pddlInitStateRmFluent(&is, f) == 0);
    assert(pddlInitStateRmFluent(&is, f) == -1);
    checkCounters(&is, 3);

    dump("return-codes", &is);

    delAtom(a);
    delAtom(f);
    pddlInitStateFree(&is);
}

TEST_ONCE(pddl_init_state_remap_objs_merge_fluents)
{
    pddl_init_state_t is;
    pddlInitStateInit(&is);

    // Two distinct fluents of the same function, and two of another one
    pddl_fm_atom_t *f1 = mkAtom(0, 1, 0);
    pddl_fm_atom_t *f2 = mkAtom(0, 1, 1);
    pddl_fm_atom_t *g = mkAtom(1, 2, 0, 1);
    pddl_num_val_t v = mkInt(11);
    assert(pddlInitStateAddFluent(&is, f1, &v) == 0);
    v = mkInt(22);
    assert(pddlInitStateAddFluent(&is, f2, &v) == 0);
    v = mkInt(33);
    assert(pddlInitStateAddFluent(&is, g, &v) == 0);
    assert(pddlInitStateFluentSize(&is) == 3);
    dump("before", &is);

    // Collapsing both objects onto 0 makes f0(0) and f0(1) identical, and
    // likewise g(0,1) and g(0,0) -- the duplicates must be merged away
    int merge[2] = { 0, 0 };
    pddlInitStateRemapObjs(&is, merge);
    dump("after-merge", &is);
    assert(pddlInitStateFluentSize(&is) == 2);
    checkCounters(&is, 3);

    // The first entry wins, so the surviving values are the first ones
    pddl_num_val_t got;
    assert(pddlInitStateFluentVal(&is, f1, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 11);

    pddl_fm_atom_t *g00 = mkAtom(1, 2, 0, 0);
    assert(pddlInitStateFluentVal(&is, g00, &got) == 0);
    assert(pddlNumValIsInt(&got) && got.v.i == 33);
    delAtom(g00);

    // The merged-away entry must be gone from the table, not just the array
    assert(pddlInitStateFluentSize(&is) == 2);

    delAtom(f1);
    delAtom(f2);
    delAtom(g);
    pddlInitStateFree(&is);
}
