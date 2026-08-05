/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the formula transformations pddlFmNormalize(), pddlFmSimplify(),
 * pddlFmSimplifyEff(), pddlFmDeduplicate(), pddlFmDeduplicateAtoms(), and
 * pddlFmDeconflictEff().
 *
 * All tests are TEST_ONCE (not per task): every test hand-builds its input
 * formulas and a minimal in-memory pddl_t (see mkPddl()), so no PDDL files
 * are needed. Statuses are asserted; the transformed formulas are printed
 * to stdout and diffed against the golden baselines in reg/_/.
 *
 * Run with:  cd tests && make && ./test -T _ -s fm_
 * (note that -s fm_ also matches the fm_num_* tests; use -t <name> for an
 * exact selection)
 */

#include "pddl/pddl.h"
#include "test.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* IDs created by mkPddl() */
#define TYPE_OBJECT 0
/** Type holding objects a and b */
#define TYPE_T 1
/** Type without any objects */
#define TYPE_EMPTY 2
#define OBJ_A 0
#define OBJ_B 1
#define OBJ_C 2
#define PRED_EQ 0
/** Static predicate of arity 1; (ps a) holds in the initial state */
#define PRED_PS 1
/** Static predicate of arity 2; (ps2 a b) holds in the initial state */
#define PRED_PS2 2
/** Non-static predicate of arity 1; negation pair with PRED_NP */
#define PRED_P 3
/** Non-static predicate of arity 1; negation pair with PRED_P */
#define PRED_NP 4
/** Non-static predicate of arity 1 */
#define PRED_Q 5
/** Non-static predicate of arity 2 */
#define PRED_R 6
/** 0-ary function */
#define FUNC_F 0

static pddl_pred_t *addPred(pddl_preds_t *ps, const char *name, int arity)
{
    pddl_pred_t *p = pddlPredsAdd(ps);
    pddlPredSetName(p, name);
    pddlPredAllocParams(p, arity);
    for (int i = 0; i < arity; ++i)
        pddlPredSetParamType(p, i, TYPE_OBJECT);
    return p;
}

/** Builds a minimal pddl_t with the types, objects, predicates, function,
 *  and initial state described by the macros above. */
static void mkPddl(pddl_t *pddl)
{
    memset(pddl, 0, sizeof(*pddl));

    pddlTypesInit(&pddl->type);
    int type_t = pddlTypesAdd(&pddl->type, "t", TYPE_OBJECT);
    assert(type_t == TYPE_T);
    int type_empty = pddlTypesAdd(&pddl->type, "t0", TYPE_OBJECT);
    assert(type_empty == TYPE_EMPTY);

    pddlObjsInit(&pddl->obj);
    pddlObjsAdd(&pddl->obj, "a")->type = TYPE_T;
    pddlObjsAdd(&pddl->obj, "b")->type = TYPE_T;
    pddlObjsAdd(&pddl->obj, "c")->type = TYPE_OBJECT;
    assert(pddl->obj.obj_size == 3);
    pddlTypesAddObj(&pddl->type, OBJ_A, TYPE_OBJECT);
    pddlTypesAddObj(&pddl->type, OBJ_B, TYPE_OBJECT);
    pddlTypesAddObj(&pddl->type, OBJ_C, TYPE_OBJECT);
    pddlTypesAddObj(&pddl->type, OBJ_A, TYPE_T);
    pddlTypesAddObj(&pddl->type, OBJ_B, TYPE_T);

    pddlPredsInitEq(&pddl->pred);
    assert(pddl->pred.eq_pred == PRED_EQ);
    assert(addPred(&pddl->pred, "ps", 1)->id == PRED_PS);
    assert(addPred(&pddl->pred, "ps2", 2)->id == PRED_PS2);
    assert(addPred(&pddl->pred, "p", 1)->id == PRED_P);
    assert(addPred(&pddl->pred, "np", 1)->id == PRED_NP);
    assert(addPred(&pddl->pred, "q", 1)->id == PRED_Q);
    assert(addPred(&pddl->pred, "r", 2)->id == PRED_R);
    pddl->pred.pred[PRED_P].neg_of = PRED_NP;
    pddl->pred.pred[PRED_NP].neg_of = PRED_P;

    pddlPredsInitEmpty(&pddl->func);
    assert(addPred(&pddl->func, "f", 0)->id == FUNC_F);

    pddlInitStateInit(&pddl->init);
    int arg1[1] = { OBJ_A };
    int ret = pddlInitStateAddAtomByPredArgs(&pddl->init, PRED_PS, 1, arg1);
    assert(ret == 0);
    int arg2[2] = { OBJ_A, OBJ_B };
    ret = pddlInitStateAddAtomByPredArgs(&pddl->init, PRED_PS2, 2, arg2);
    assert(ret == 0);

    pddlResetProps(pddl);
    pddl->props.pred_prop[PRED_P].in_eff = pddl_true;
    pddl->props.pred_prop[PRED_NP].in_eff = pddl_true;
    pddl->props.pred_prop[PRED_Q].in_eff = pddl_true;
    pddl->props.pred_prop[PRED_R].in_eff = pddl_true;
}

static void freePddl(pddl_t *pddl)
{
    pddlPropsFree(&pddl->props);
    pddlInitStateFree(&pddl->init);
    pddlPredsFree(&pddl->pred);
    pddlPredsFree(&pddl->func);
    pddlObjsFree(&pddl->obj);
    pddlTypesFree(&pddl->type);
}

/** Creates the atom (pred obj), negated if NEG */
static pddl_fm_t *atomO(int pred, int obj, pddl_bool_t neg)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].obj = obj;
    a->neg = neg;
    return &a->fm;
}

/** Creates the atom (pred o1 o2), negated if NEG */
static pddl_fm_t *atomOO(int pred, int o1, int o2, pddl_bool_t neg)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(2);
    a->pred = pred;
    a->arg[0].obj = o1;
    a->arg[1].obj = o2;
    a->neg = neg;
    return &a->fm;
}

/** Creates the atom (pred ?param), negated if NEG */
static pddl_fm_t *atomP(int pred, int param, pddl_bool_t neg)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].param = param;
    a->neg = neg;
    return &a->fm;
}

/** Creates the atom (pred ?param obj), negated if NEG */
static pddl_fm_t *atomPO(int pred, int param, int obj, pddl_bool_t neg)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(2);
    a->pred = pred;
    a->arg[0].param = param;
    a->arg[1].obj = obj;
    a->neg = neg;
    return &a->fm;
}

/** Creates the equality atom (= o1 o2), negated if NEG */
static pddl_fm_t *eqOO(int o1, int o2, pddl_bool_t neg)
{
    return atomOO(PRED_EQ, o1, o2, neg);
}

/** Creates the equality atom (= ?param obj) */
static pddl_fm_t *eqPO(int param, int obj)
{
    return atomPO(PRED_EQ, param, obj, pddl_false);
}

/** Creates the equality atom (= obj ?param) */
static pddl_fm_t *eqOP(int obj, int param)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(2);
    a->pred = PRED_EQ;
    a->arg[0].obj = obj;
    a->arg[1].param = param;
    return &a->fm;
}

/** Creates an and/or node from N formulas */
static pddl_fm_t *mkJunc(pddl_fm_t *junc, int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; ++i)
        pddlFmJuncAdd(pddlFmToJunc(junc), va_arg(ap, pddl_fm_t *));
    va_end(ap);
    return junc;
}

#define mkAnd(N, ...) mkJunc(pddlFmNewEmptyAnd(), (N), __VA_ARGS__)
#define mkOr(N, ...) mkJunc(pddlFmNewEmptyOr(), (N), __VA_ARGS__)

/** Creates a quantifier node with a single parameter of the given type
 *  quantifying QFM */
static pddl_fm_t *mkQuant(int fm_type, int param_type, pddl_fm_t *qfm)
{
    pddl_fm_quant_t *q = pddlFmNewEmptyQuant(fm_type);
    pddl_param_t *p = pddlParamsAdd(&q->param);
    p->name = PDDL_STRDUP("?x");
    p->type = param_type;
    q->qfm = qfm;
    return &q->fm;
}

/** Creates the fluent atom (f) */
static pddl_fm_atom_t *fluentAtomF(void)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(0);
    a->pred = FUNC_F;
    return a;
}

/** Creates the numeric fluent expression (f) */
static pddl_fm_num_exp_t *fluentF(void)
{
    return pddlFmNewNumExpFluent(fluentAtomF());
}

static void show(const char *label, const pddl_fm_t *fm,
                 const pddl_t *pddl, const pddl_params_t *params)
{
    char buf[1024];
    if (fm == NULL){
        printf("%s: <null>\n", label);
    }else{
        printf("%s: %s\n", label,
               pddlFmFmt(fm, pddl, params, buf, sizeof(buf)));
    }
}

typedef int (*fm_fn)(pddl_fm_t **fm, const pddl_t *pddl,
                     const pddl_params_t *params, pddl_err_t *err);

/** Prints FM, applies FN on it, prints the status and the result, and
 *  returns the status. */
static int run(fm_fn fn, pddl_fm_t **fm, const pddl_t *pddl,
               const pddl_params_t *params, pddl_err_t *err)
{
    show("  in ", *fm, pddl, params);
    int st = fn(fm, pddl, params, err);
    printf("  status: %d\n", st);
    show("  out", *fm, pddl, params);
    return st;
}

/** run() + delete the output formula */
static int runDel(fm_fn fn, pddl_fm_t *fm, const pddl_t *pddl,
                  const pddl_params_t *params, pddl_err_t *err)
{
    int st = run(fn, &fm, pddl, params, err);
    if (fm != NULL)
        pddlFmDel(fm);
    return st;
}


TEST_ONCE(fm_deduplicate_atoms)
{
    pddl_t pddl;
    mkPddl(&pddl);

    // Duplicate atom removed from an and node
    printf("dup in and:\n");
    pddl_fm_t *fm = mkAnd(3, atomO(PRED_P, OBJ_A, 0), atomO(PRED_Q, OBJ_A, 0),
                          atomO(PRED_P, OBJ_A, 0));
    show("  in ", fm, &pddl, NULL);
    int st = pddlFmDeduplicateAtoms(&fm, &pddl);
    printf("  status: %d\n", st);
    show("  out", fm, &pddl, NULL);
    assert(st == 1);

    // Second call changes nothing
    st = pddlFmDeduplicateAtoms(&fm, &pddl);
    assert(st == 0);
    pddlFmDel(fm);

    // Duplicate atom removed from an or node, pddl may be NULL
    printf("dup in or (NULL pddl):\n");
    fm = mkOr(3, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_A, 0),
              atomO(PRED_Q, OBJ_B, 0));
    show("  in ", fm, &pddl, NULL);
    st = pddlFmDeduplicateAtoms(&fm, NULL);
    printf("  status: %d\n", st);
    show("  out", fm, &pddl, NULL);
    assert(st == 1);
    pddlFmDel(fm);

    // An atom and its negation are not duplicates
    printf("atom vs negated atom:\n");
    fm = mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_A, 1));
    show("  in ", fm, &pddl, NULL);
    st = pddlFmDeduplicateAtoms(&fm, &pddl);
    printf("  status: %d\n", st);
    show("  out", fm, &pddl, NULL);
    assert(st == 0);
    pddlFmDel(fm);

    freePddl(&pddl);
}

TEST_ONCE(fm_deduplicate)
{
    pddl_t pddl;
    mkPddl(&pddl);

    // Duplicate subformulas are removed; junction comparison is
    // order-insensitive
    printf("dup or subformulas:\n");
    pddl_fm_t *fm = mkAnd(3,
            mkOr(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_Q, OBJ_A, 0)),
            mkOr(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_P, OBJ_A, 0)),
            atomO(PRED_Q, OBJ_B, 0));
    show("  in ", fm, &pddl, NULL);
    int st = pddlFmDeduplicate(&fm, &pddl);
    printf("  status: %d\n", st);
    show("  out", fm, &pddl, NULL);
    assert(st == 1);

    // Second call changes nothing
    st = pddlFmDeduplicate(&fm, &pddl);
    assert(st == 0);
    pddlFmDel(fm);

    freePddl(&pddl);
}

TEST_ONCE(fm_deconflict_eff)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;

    // Conflicting literals in an and node: the positive literal is kept
    // ("first delete then add"); the result is re-normalized
    printf("conflict in and:\n");
    pddl_fm_t *fm = mkAnd(3, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_A, 1),
                          atomO(PRED_Q, OBJ_B, 0));
    int st = run(pddlFmDeconflictEff, &fm, &pddl, NULL, &err);
    assert(st == 1);
    pddlFmDel(fm);

    // No conflict: nothing changes
    printf("no conflict:\n");
    fm = mkAnd(2, atomO(PRED_P, OBJ_A, 1), atomO(PRED_Q, OBJ_B, 0));
    st = run(pddlFmDeconflictEff, &fm, &pddl, NULL, &err);
    assert(st == 0);
    pddlFmDel(fm);

    // A negation pair of two different predicates is not a conflict here
    printf("neg_of pair is not a conflict:\n");
    fm = mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_NP, OBJ_A, 0));
    st = run(pddlFmDeconflictEff, &fm, &pddl, NULL, &err);
    assert(st == 0);
    pddlFmDel(fm);

    // Effects of (when ...) are deconflicted, its condition is kept intact
    printf("conflict inside when effect:\n");
    pddl_fm_t *wpre = mkAnd(2, atomO(PRED_P, OBJ_C, 0), atomO(PRED_P, OBJ_C, 1));
    pddl_fm_t *weff = mkAnd(2, atomOO(PRED_R, OBJ_A, OBJ_B, 0),
                            atomOO(PRED_R, OBJ_A, OBJ_B, 1));
    pddl_fm_when_t *when = pddlFmNewWhen(wpre, weff);
    fm = mkAnd(2, atomO(PRED_Q, OBJ_A, 0), &when->fm);
    st = run(pddlFmDeconflictEff, &fm, &pddl, NULL, &err);
    assert(st == 1);
    pddlFmDel(fm);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_bools)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("TRUE removed from and:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0), &pddlFmNewBool(1)->fm),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("FALSE collapses and:\n");
    pddl_fm_t *fm = mkAnd(2, atomO(PRED_P, OBJ_A, 0), &pddlFmNewBool(0)->fm);
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("TRUE collapses or:\n");
    fm = mkOr(2, atomO(PRED_P, OBJ_A, 0), &pddlFmNewBool(1)->fm);
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsTrue(fm));
    pddlFmDel(fm);

    printf("FALSE removed from or:\n");
    st = runDel(pddlFmSimplify,
                mkOr(2, atomO(PRED_P, OBJ_A, 0), &pddlFmNewBool(0)->fm),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("singleton and unwrapped:\n");
    st = runDel(pddlFmSimplify, mkAnd(1, atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("nested and flattened:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0),
                      mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                            atomO(PRED_Q, OBJ_B, 0))),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("no change:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_Q, OBJ_B, 0)),
                &pddl, NULL, &err);
    assert(st == 0);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_conflict)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("conflict in and:\n");
    pddl_fm_t *fm = mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_A, 1));
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("conflict in or:\n");
    fm = mkOr(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_A, 1));
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsTrue(fm));
    pddlFmDel(fm);

    printf("neg_of pair conflict in and:\n");
    fm = mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_NP, OBJ_A, 0));
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("different args are not in conflict:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_P, OBJ_B, 1)),
                &pddl, NULL, &err);
    assert(st == 0);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_eq)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("(= a a) is true:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_C, 0), eqOO(OBJ_A, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(= a b) is false:\n");
    pddl_fm_t *fm = mkAnd(2, atomO(PRED_P, OBJ_C, 0), eqOO(OBJ_A, OBJ_B, 0));
    st = run(pddlFmSimplify, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("(not (= a b)) is true:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_C, 0), eqOO(OBJ_A, OBJ_B, 1)),
                &pddl, NULL, &err);
    assert(st == 1);

    pddl_params_t params;
    pddlParamsInit(&params);
    pddlParamsAdd(&params)->type = TYPE_T;

    printf("conflicting (= ?x a) and (= ?x b):\n");
    fm = mkAnd(3, atomP(PRED_P, 0, 0), eqPO(0, OBJ_A), eqPO(0, OBJ_B));
    st = run(pddlFmSimplify, &fm, &pddl, &params, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("redundant duplicate (= ?x a):\n");
    st = runDel(pddlFmSimplify,
                mkAnd(3, atomP(PRED_P, 0, 0), eqPO(0, OBJ_A), eqPO(0, OBJ_A)),
                &pddl, &params, &err);
    assert(st == 1);

    printf("(= a ?x) reordered to (= ?x a):\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomP(PRED_P, 0, 0), eqOP(OBJ_A, 0)),
                &pddl, &params, &err);
    assert(st == 1);

    pddlParamsFree(&params);
    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_entail)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("entailed disjunction removed:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0),
                      mkOr(2, atomO(PRED_P, OBJ_A, 0),
                           atomO(PRED_Q, OBJ_A, 0))),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("negated conjunct removed from disjunction:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0),
                      mkOr(2, atomO(PRED_P, OBJ_A, 1),
                           atomO(PRED_Q, OBJ_A, 0))),
                &pddl, NULL, &err);
    assert(st == 1);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_num)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("constant folding:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(
                    pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(1),
                                        pddlFmNewNumExpNumInt(2)),
                    pddlFmNewNumExpMult(pddlFmNewNumExpNumInt(2),
                                        pddlFmNewNumExpNumInt(3)))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(* 1 (f)) -> (f):\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(
                    pddlFmNewNumExpMult(pddlFmNewNumExpNumInt(1), fluentF()),
                    pddlFmNewNumExpNumInt(2))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(+ 0 (f)) and (- (f) 0) -> (f):\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(
                    pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(0), fluentF()),
                    pddlFmNewNumExpMinus(fluentF(),
                                         pddlFmNewNumExpNumInt(0)))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(* 0 (f)) -> 0:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(
                    pddlFmNewNumExpMult(pddlFmNewNumExpNumInt(0), fluentF()),
                    pddlFmNewNumExpNumInt(1))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("multiplication constants folded:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(
                    pddlFmNewNumExpMult(
                        pddlFmNewNumExpNumInt(2),
                        pddlFmNewNumExpMult(pddlFmNewNumExpNumInt(3),
                                            fluentF())),
                    pddlFmNewNumExpNumInt(1))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("integral float recast to int:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumCmpGE(pddlFmNewNumExpNumFlt(2.),
                                   pddlFmNewNumExpNumInt(1))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_status)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("no change:\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, atomO(PRED_P, OBJ_A, 0), atomO(PRED_Q, OBJ_B, 0)),
                &pddl, NULL, &err);
    assert(st == 0);

    printf("division by zero:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                                    pddlFmNewNumExpNumInt(0))->fm,
                &pddl, NULL, &err);
    assert(st == -1);

    printf("integer overflow:\n");
    st = runDel(pddlFmSimplify,
                &pddlFmNewNumExpMult(pddlFmNewNumExpNumInt(INT64_MAX),
                                     pddlFmNewNumExpNumInt(2))->fm,
                &pddl, NULL, &err);
    assert(st == -1);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_degenerate)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("(and (and)):\n");
    st = runDel(pddlFmSimplify, mkAnd(1, pddlFmNewEmptyAnd()),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(and (or) (p a)):\n");
    st = runDel(pddlFmSimplify,
                mkAnd(2, pddlFmNewEmptyOr(), atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(or (and) (p a)):\n");
    st = runDel(pddlFmSimplify,
                mkOr(2, pddlFmNewEmptyAnd(), atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(or (and (and)) (p a)):\n");
    st = runDel(pddlFmSimplify,
                mkOr(2, mkAnd(1, pddlFmNewEmptyAnd()),
                     atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("(or (and (and) (p a)) (p a)):\n");
    st = runDel(pddlFmSimplify,
                mkOr(2, mkAnd(2, pddlFmNewEmptyAnd(),
                              atomO(PRED_P, OBJ_A, 0)),
                     atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    freePddl(&pddl);
}

TEST_ONCE(fm_normalize_quant)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("forall over t:\n");
    st = runDel(pddlFmNormalize,
                mkQuant(PDDL_FM_FORALL, TYPE_T, atomP(PRED_Q, 0, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("exists over t:\n");
    st = runDel(pddlFmNormalize,
                mkQuant(PDDL_FM_EXIST, TYPE_T, atomP(PRED_Q, 0, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("forall over the empty type:\n");
    pddl_fm_t *fm = mkQuant(PDDL_FM_FORALL, TYPE_EMPTY, atomP(PRED_Q, 0, 0));
    st = run(pddlFmNormalize, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsTrue(fm));
    pddlFmDel(fm);

    printf("exists over the empty type:\n");
    fm = mkQuant(PDDL_FM_EXIST, TYPE_EMPTY, atomP(PRED_Q, 0, 0));
    st = run(pddlFmNormalize, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    freePddl(&pddl);
}

TEST_ONCE(fm_normalize_imply)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    // Implication over a non-static predicate is rewritten as a
    // disjunction
    printf("non-static imply:\n");
    st = runDel(pddlFmNormalize,
                &pddlFmNewImply(atomO(PRED_Q, OBJ_A, 0),
                                atomO(PRED_P, OBJ_B, 0))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    // Implication over a static predicate is removed by exhaustive
    // instantiation of the parameter
    printf("static imply:\n");
    pddl_params_t params;
    pddlParamsInit(&params);
    pddlParamsAdd(&params)->type = TYPE_T;
    st = runDel(pddlFmNormalize,
                &pddlFmNewImply(atomP(PRED_PS, 0, 0),
                                atomP(PRED_Q, 0, 0))->fm,
                &pddl, &params, &err);
    assert(st == 1);
    pddlParamsFree(&params);

    freePddl(&pddl);
}

TEST_ONCE(fm_normalize_bool)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("static atom that holds in init:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_PS, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("static atom that does not hold in init:\n");
    pddl_fm_t *fm = mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                          atomO(PRED_PS, OBJ_B, 0));
    st = run(pddlFmNormalize, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(fm));
    pddlFmDel(fm);

    printf("negated static atom that does not hold in init:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_PS, OBJ_B, 1)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("ground equality:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(3, atomO(PRED_Q, OBJ_C, 0), eqOO(OBJ_A, OBJ_A, 0),
                      eqOO(OBJ_A, OBJ_B, 1)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("when with a statically true condition:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                      &pddlFmNewWhen(atomO(PRED_PS, OBJ_A, 0),
                                     atomO(PRED_Q, OBJ_B, 0))->fm),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("when with a statically false condition:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                      &pddlFmNewWhen(atomO(PRED_PS, OBJ_B, 0),
                                     atomO(PRED_Q, OBJ_B, 0))->fm),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("empty and is true:\n");
    pddl_fm_t *empty = pddlFmNewEmptyAnd();
    st = run(pddlFmNormalize, &empty, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsTrue(empty));
    pddlFmDel(empty);

    printf("empty or is false:\n");
    empty = pddlFmNewEmptyOr();
    st = run(pddlFmNormalize, &empty, &pddl, NULL, &err);
    assert(st == 1);
    assert(pddlFmIsFalse(empty));
    pddlFmDel(empty);

    pddl_params_t params;
    pddlParamsInit(&params);
    pddlParamsAdd(&params)->type = TYPE_T;

    printf("negated partially instantiated static atom, no match:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                      atomPO(PRED_PS2, 0, OBJ_A, 1)),
                &pddl, &params, &err);
    assert(st == 1);

    printf("negated partially instantiated static atom, with a match:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0),
                      atomPO(PRED_PS2, 0, OBJ_B, 1)),
                &pddl, &params, &err);
    assert(st == 0);

    pddlParamsFree(&params);

    printf("flattening + instantiated static atom, with a match:\n");
    st = runDel(pddlFmNormalize,
                mkOr(2, mkAnd(2, mkAnd(1, pddlFmNewEmptyAnd()),
                              atomO(PRED_PS, OBJ_A, 0)),
                     atomO(PRED_P, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);
    freePddl(&pddl);
}

TEST_ONCE(fm_normalize_when_dnf)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    printf("when with a disjunctive condition is split:\n");
    st = runDel(pddlFmNormalize,
                &pddlFmNewWhen(mkOr(2, atomO(PRED_Q, OBJ_A, 0),
                                    atomO(PRED_Q, OBJ_B, 0)),
                               atomO(PRED_P, OBJ_C, 0))->fm,
                &pddl, NULL, &err);
    assert(st == 1);

    printf("disjunctions are moved up:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, mkOr(2, atomO(PRED_Q, OBJ_A, 0),
                              atomO(PRED_Q, OBJ_B, 0)),
                      atomO(PRED_Q, OBJ_C, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    printf("duplicate atoms are removed:\n");
    st = runDel(pddlFmNormalize,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_Q, OBJ_A, 0)),
                &pddl, NULL, &err);
    assert(st == 1);

    freePddl(&pddl);
}

TEST_ONCE(fm_normalize_status)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;

    printf("already normalized formula:\n");
    int st = runDel(pddlFmNormalize,
                    mkAnd(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_Q, OBJ_B, 0)),
                    &pddl, NULL, &err);
    assert(st == 0);

    freePddl(&pddl);
}

TEST_ONCE(fm_simplify_eff)
{
    pddl_t pddl;
    mkPddl(&pddl);
    pddl_err_t err = PDDL_ERR_INIT;
    int st;

    // Numeric no-op effects are removed or rewritten
    printf("no-op numeric effects:\n");
    pddl_fm_t *fm = mkAnd(6,
            atomO(PRED_Q, OBJ_A, 0),
            &pddlFmNewNumOpIncrease(fluentAtomF(),
                                    pddlFmNewNumExpNumInt(0))->fm,
            &pddlFmNewNumOpScaleUp(fluentAtomF(),
                                   pddlFmNewNumExpNumInt(1))->fm,
            &pddlFmNewNumOpScaleUp(fluentAtomF(),
                                   pddlFmNewNumExpNumInt(0))->fm,
            &pddlFmNewNumOpDecrease(fluentAtomF(),
                                    pddlFmNewNumExpMinus(
                                        pddlFmNewNumExpNumInt(2),
                                        pddlFmNewNumExpNumInt(2)))->fm,
            &pddlFmNewNumOpIncrease(fluentAtomF(),
                                    pddlFmNewNumExpPlus(
                                        pddlFmNewNumExpNumInt(1),
                                        pddlFmNewNumExpNumInt(2)))->fm);
    st = run(pddlFmSimplifyEff, &fm, &pddl, NULL, &err);
    assert(st == 1);

    // Second call changes nothing
    st = run(pddlFmSimplifyEff, &fm, &pddl, NULL, &err);
    assert(st == 0);
    pddlFmDel(fm);

    // A no-op numeric effect at the root leaves NULL behind
    printf("no-op numeric effect at the root:\n");
    fm = &pddlFmNewNumOpIncrease(fluentAtomF(),
                                 pddlFmNewNumExpNumInt(0))->fm;
    st = run(pddlFmSimplifyEff, &fm, &pddl, NULL, &err);
    assert(st == 1);
    assert(fm == NULL);

    // Numeric error in the right-hand side is reported
    printf("division by zero in the right-hand side:\n");
    fm = &pddlFmNewNumOpIncrease(fluentAtomF(),
                                 pddlFmNewNumExpDiv(
                                     pddlFmNewNumExpNumInt(1),
                                     pddlFmNewNumExpNumInt(0)))->fm;
    st = run(pddlFmSimplifyEff, &fm, &pddl, NULL, &err);
    assert(st == -1);
    pddlFmDel(fm);

    // No numeric operations: nothing to do
    printf("no numeric operations:\n");
    st = runDel(pddlFmSimplifyEff,
                mkAnd(2, atomO(PRED_Q, OBJ_A, 0), atomO(PRED_Q, OBJ_B, 0)),
                &pddl, NULL, &err);
    assert(st == 0);

    freePddl(&pddl);
}
