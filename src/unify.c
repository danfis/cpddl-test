/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/unify.h"
#include "test.h"
#include <assert.h>
#include <string.h>

/* Predicate IDs used in hand-constructed atoms */
enum {
    PRED_EQ = 0,
    PRED_P,
    PRED_Q,
};

/* Object IDs used in hand-constructed atoms */
enum {
    OBJ_T0 = 0, /* type t */
    OBJ_T1,     /* type t */
    OBJ_S0,     /* type s */
};

/* A small type system:
 * object (id 0) = {OBJ_T0, OBJ_T1, OBJ_S0}
 * t = {OBJ_T0, OBJ_T1}
 * s = {OBJ_S0}
 * t and s are disjoint, both are subsets of object. */
struct fixture {
    pddl_types_t type;
    int obj;
    int t;
    int s;
};
typedef struct fixture fixture_t;

static void fixtureInit(fixture_t *f)
{
    pddlTypesInit(&f->type);
    f->obj = 0;
    f->t = pddlTypesAdd(&f->type, "t", f->obj);
    f->s = pddlTypesAdd(&f->type, "s", f->obj);
    pddlTypesAddObj(&f->type, OBJ_T0, f->t);
    pddlTypesAddObj(&f->type, OBJ_T1, f->t);
    pddlTypesAddObj(&f->type, OBJ_S0, f->s);
}

static void fixtureFree(fixture_t *f)
{
    pddlTypesFree(&f->type);
}

/* Object IDs used with the deeper type hierarchy (hier_t) */
enum {
    HOBJ_CAR0 = 0,
    HOBJ_CAR1,
    HOBJ_TRUCK0,
    HOBJ_LOC0,
};

/* A deeper type hierarchy:
 * object (id 0) = {CAR0, CAR1, TRUCK0, LOC0}
 * +- vehicle = {CAR0, CAR1, TRUCK0}
 * |  +- car = {CAR0, CAR1}
 * |  +- truck = {TRUCK0}
 * |  +- bike = {} (empty)
 * +- location = {LOC0}
 * +- red = {CAR1, TRUCK0} -- declared directly under object; it overlaps
 *    both car and truck without being comparable to either, but it is a
 *    subset of vehicle by its objects */
struct hier {
    pddl_types_t type;
    int obj;
    int vehicle;
    int car;
    int truck;
    int bike;
    int location;
    int red;
};
typedef struct hier hier_t;

static void hierInit(hier_t *h)
{
    pddlTypesInit(&h->type);
    h->obj = 0;
    h->vehicle = pddlTypesAdd(&h->type, "vehicle", h->obj);
    h->car = pddlTypesAdd(&h->type, "car", h->vehicle);
    h->truck = pddlTypesAdd(&h->type, "truck", h->vehicle);
    h->bike = pddlTypesAdd(&h->type, "bike", h->vehicle);
    h->location = pddlTypesAdd(&h->type, "location", h->obj);
    h->red = pddlTypesAdd(&h->type, "red", h->obj);
    pddlTypesAddObj(&h->type, HOBJ_CAR0, h->car);
    pddlTypesAddObj(&h->type, HOBJ_CAR1, h->car);
    pddlTypesAddObj(&h->type, HOBJ_CAR1, h->red);
    pddlTypesAddObj(&h->type, HOBJ_TRUCK0, h->truck);
    pddlTypesAddObj(&h->type, HOBJ_TRUCK0, h->red);
    pddlTypesAddObj(&h->type, HOBJ_LOC0, h->location);
}

static void hierFree(hier_t *h)
{
    pddlTypesFree(&h->type);
}

static void addParam(pddl_params_t *params, int type)
{
    pddl_param_t *p = pddlParamsAdd(params);
    p->type = type;
}

static void addCountedParam(pddl_params_t *params, int type)
{
    pddl_param_t *p = pddlParamsAdd(params);
    p->type = type;
    p->is_counted_var = pddl_true;
}

/* Create atom pred(?param) */
static pddl_fm_atom_t *newAtomP(int pred, int param)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].param = param;
    return a;
}

/* Create atom pred(obj) */
static pddl_fm_atom_t *newAtomO(int pred, int obj)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].obj = obj;
    return a;
}

/* Create atom pred(?param1, ?param2) */
static pddl_fm_atom_t *newAtomPP(int pred, int param1, int param2)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(2);
    a->pred = pred;
    a->arg[0].param = param1;
    a->arg[1].param = param2;
    return a;
}

/* Create formula (and (= ?param1 ?param2)), or the negated atom if NEG */
static pddl_fm_t *newEqCond(int param1, int param2, pddl_bool_t neg)
{
    pddl_fm_t *and = pddlFmNewEmptyAnd();
    pddl_fm_atom_t *eq = pddlFmNewEmptyAtom(2);
    eq->pred = PRED_EQ;
    eq->arg[0].param = param1;
    eq->arg[1].param = param2;
    eq->neg = neg;
    pddlFmJuncAdd(pddlFmToJunc(and), &eq->fm);
    return and;
}

static void assertMapsInSync(const pddl_unify_t *u)
{
    assert(u->param[0] == u->param[1]);
    assert(memcmp(u->map[0], u->map[1],
                  sizeof(pddl_unify_val_t) * u->param[0]->param_size) == 0);
}


TEST_ONCE(unify_distinct_params_vars)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);
    // Identity unifier: each side gets its own variable scope
    assert(u.map[0][0].var == 0);
    assert(u.map[1][0].var == 1);
    assert(u.map[0][0].obj == -1);
    assert(u.map[1][0].obj == -1);

    pddl_fm_atom_t *a1 = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *a2 = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, a1, a2) == 0);
    // Both sides map to the same variable of type t
    assert(u.map[0][0].var == u.map[1][0].var);
    assert(u.map[0][0].obj == -1);
    assert(u.map[1][0].obj == -1);
    assert(u.map[0][0].var_type == f.t);
    assert(u.map[1][0].var_type == f.t);

    pddlFmDel(&a1->fm);
    pddlFmDel(&a2->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_var_obj)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    pddl_fm_atom_t *ao1 = newAtomO(PRED_P, OBJ_T1);
    assert(pddlUnify(&u, ax, ao0) == 0);
    // x is bound to OBJ_T0, y on the other side is untouched
    assert(u.map[0][0].obj == OBJ_T0);
    assert(u.map[0][0].var == -1);
    assert(u.map[1][0].var == 1);
    assert(u.map[1][0].obj == -1);
    // x is already bound to OBJ_T0, so it cannot unify with OBJ_T1
    assert(pddlUnify(&u, ax, ao1) != 0);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlFmDel(&ao1->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_fail)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.s);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    // Disjoint types t and s cannot be unified
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) != 0);

    // Predicate mismatch
    pddl_fm_atom_t *aq = newAtomP(PRED_Q, 0);
    assert(pddlUnify(&u, ax, aq) != 0);

    // Two different objects cannot be unified
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    pddl_fm_atom_t *ao1 = newAtomO(PRED_P, OBJ_T1);
    assert(pddlUnify(&u, ao0, ao1) != 0);

    // Variable of type t cannot be bound to an object of type s
    pddl_fm_atom_t *aos = newAtomO(PRED_P, OBJ_S0);
    assert(pddlUnify(&u, ax, aos) != 0);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlFmDel(&aq->fm);
    pddlFmDel(&ao0->fm);
    pddlFmDel(&ao1->fm);
    pddlFmDel(&aos->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_type_subset)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.obj);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) == 0);
    // The merged variable is narrowed to the more specific type t
    assert(u.map[0][0].var == u.map[1][0].var);
    assert(u.map[0][0].var_type == f.t);
    assert(u.map[1][0].var_type == f.t);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_equality_inequality)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t); // x
    addParam(&p1, f.t); // z
    addParam(&p2, f.t); // y
    addParam(&p2, f.t); // w

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    // On the identity unifier (!= x z) holds
    pddl_fm_t *ineq = newEqCond(0, 1, pddl_true);
    assert(pddlUnifyCheckInequality(&u, &p1, PRED_EQ, ineq));

    // Apply (= x z) on side 0
    pddl_fm_t *eq = newEqCond(0, 1, pddl_false);
    assert(pddlUnifyApplyEquality(&u, &p1, PRED_EQ, eq) == 0);
    assert(u.map[0][0].var == u.map[0][1].var);
    // ... and now (!= x z) is violated
    assert(!pddlUnifyCheckInequality(&u, &p1, PRED_EQ, ineq));

    // Apply (= y w) on side 1
    assert(pddlUnifyApplyEquality(&u, &p2, PRED_EQ, eq) == 0);
    assert(u.map[1][0].var == u.map[1][1].var);
    // Sides were merged independently
    assert(u.map[0][0].var != u.map[1][0].var);

    pddlFmDel(ineq);
    pddlFmDel(eq);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_atoms_differ)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *aq = newAtomP(PRED_Q, 0);
    // On the identity unifier x and y are different variables
    assert(pddlUnifyAtomsDiffer(&u, &p1, ax, &p2, ay));
    assert(pddlUnify(&u, ax, ay) == 0);
    // After unification p(x) and p(y) are the same atom
    assert(!pddlUnifyAtomsDiffer(&u, &p1, ax, &p2, ay));
    // Predicates differ
    assert(pddlUnifyAtomsDiffer(&u, &p1, aq, &p2, ay));

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlFmDel(&aq->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_to_cond)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    // The identity unifier corresponds to the constant true
    pddl_fm_t *cond = pddlUnifyToCond(&u, PRED_EQ, &p1);
    assert(pddlFmIsTrue(cond));
    pddlFmDel(cond);

    // Binding x to OBJ_T0 corresponds to (and (= x OBJ_T0))
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    assert(pddlUnify(&u, ax, ao0) == 0);
    cond = pddlUnifyToCond(&u, PRED_EQ, &p1);
    assert(!pddlFmIsTrue(cond));
    int num_atoms = 0;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(cond, &it, eq){
        assert(eq->pred == PRED_EQ);
        assert(eq->arg[0].param == 0);
        assert(eq->arg[1].obj == OBJ_T0);
        ++num_atoms;
    }
    assert(num_atoms == 1);
    pddlFmDel(cond);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_reset_counted_vars)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addCountedParam(&p1, f.t); // x
    addParam(&p2, f.t);        // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    assert(pddlUnify(&u, ax, ao0) == 0);
    assert(u.map[0][0].obj == OBJ_T0);

    pddlUnifyResetCountedVars(&u);
    // x is a fresh unbound variable again
    assert(u.map[0][0].obj == -1);
    assert(u.map[0][0].var == 0);
    assert(u.map[0][0].var_type == f.t);
    // y is not counted, so it keeps its (positional) variable
    assert(u.map[1][0].var == 1);

    // The reset variable can be unified again, with a different object
    pddl_fm_atom_t *ao1 = newAtomO(PRED_P, OBJ_T1);
    assert(pddlUnify(&u, ax, ao1) == 0);
    assert(u.map[0][0].obj == OBJ_T1);
    // ... and with the other side's variable after another reset
    pddlUnifyResetCountedVars(&u);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) == 0);
    assert(u.map[0][0].var == u.map[1][0].var);
    assert(u.map[0][0].obj == -1);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlFmDel(&ao1->fm);
    pddlFmDel(&ay->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}

TEST_ONCE(unify_distinct_params_init_copy)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, f.t);
    addParam(&p2, f.t);

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p1, &p2);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) == 0);

    pddl_unify_t c;
    pddlUnifyInitCopy(&c, &u);
    assert(pddlUnifyEq(&c, &u));

    // Unification on the copy does not modify the original
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    assert(pddlUnify(&c, ax, ao0) == 0);
    assert(c.map[0][0].obj == OBJ_T0);
    assert(u.map[0][0].obj == -1);
    assert(!pddlUnifyEq(&c, &u));

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlFmDel(&ao0->fm);
    pddlUnifyFree(&c);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    fixtureFree(&f);
}


TEST_ONCE(unify_distinct_params_type_chain)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, h.obj);     // x
    addParam(&p2, h.vehicle); // y
    addParam(&p2, h.car);     // z

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p1, &p2);

    // Unifying x:object with y:vehicle narrows the merged variable to
    // vehicle ...
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *az = newAtomP(PRED_P, 1);
    assert(pddlUnify(&u, ax, ay) == 0);
    assert(u.map[0][0].var_type == h.vehicle);
    assert(u.map[1][0].var_type == h.vehicle);

    // ... and unifying x with z:car narrows all three variables to car
    assert(pddlUnify(&u, ax, az) == 0);
    assert(u.map[0][0].var_type == h.car);
    assert(u.map[1][0].var_type == h.car);
    assert(u.map[1][1].var_type == h.car);
    assert(u.map[0][0].var == u.map[1][0].var);
    assert(u.map[0][0].var == u.map[1][1].var);

    // The merged variable cannot be bound outside of car ...
    pddl_fm_atom_t *aloc = newAtomO(PRED_P, HOBJ_LOC0);
    pddl_fm_atom_t *atruck = newAtomO(PRED_P, HOBJ_TRUCK0);
    assert(pddlUnify(&u, ax, aloc) != 0);
    assert(pddlUnify(&u, ax, atruck) != 0);
    // ... but binding to a car binds x, y, and z at once
    pddl_fm_atom_t *acar = newAtomO(PRED_P, HOBJ_CAR0);
    assert(pddlUnify(&u, ax, acar) == 0);
    assert(u.map[0][0].obj == HOBJ_CAR0);
    assert(u.map[1][0].obj == HOBJ_CAR0);
    assert(u.map[1][1].obj == HOBJ_CAR0);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlFmDel(&az->fm);
    pddlFmDel(&aloc->fm);
    pddlFmDel(&atruck->fm);
    pddlFmDel(&acar->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    hierFree(&h);
}

TEST_ONCE(unify_distinct_params_type_siblings)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, h.car);     // x
    addParam(&p1, h.vehicle); // z
    addParam(&p2, h.truck);   // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p1, &p2);

    // Sibling types car and truck are disjoint
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *az = newAtomP(PRED_P, 1);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) != 0);
    // ... but each of them unifies with the parent type vehicle
    assert(pddlUnify(&u, az, ay) == 0);
    assert(u.map[0][1].var_type == h.truck);
    assert(u.map[1][0].var_type == h.truck);

    pddlFmDel(&ax->fm);
    pddlFmDel(&az->fm);
    pddlFmDel(&ay->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    hierFree(&h);
}

TEST_ONCE(unify_distinct_params_type_overlap)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, h.car);     // x
    addParam(&p1, h.vehicle); // z
    addParam(&p2, h.red);     // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p1, &p2);

    // car and red overlap in CAR1, but neither is a subset of the other,
    // so they cannot be unified
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *az = newAtomP(PRED_P, 1);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    assert(pddlUnify(&u, ax, ay) != 0);
    // Types are compared by their object sets, not by the declared
    // hierarchy: red was declared under object, but its objects make it
    // a subset of vehicle
    assert(pddlUnify(&u, az, ay) == 0);
    assert(u.map[0][1].var_type == h.red);
    assert(u.map[1][0].var_type == h.red);

    pddlFmDel(&ax->fm);
    pddlFmDel(&az->fm);
    pddlFmDel(&ay->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    hierFree(&h);
}

TEST_ONCE(unify_distinct_params_type_empty)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p1, p2;
    pddlParamsInit(&p1);
    pddlParamsInit(&p2);
    addParam(&p1, h.bike); // x
    addParam(&p2, h.car);  // y
    addParam(&p2, h.bike); // z

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p1, &p2);

    // The empty type bike is a subset of everything, but variables of an
    // empty type are not unifiable
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *az = newAtomP(PRED_P, 1);
    assert(pddlUnify(&u, ax, ay) != 0);
    assert(pddlUnify(&u, ax, az) != 0);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ay->fm);
    pddlFmDel(&az->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p1);
    pddlParamsFree(&p2);
    hierFree(&h);
}


TEST_ONCE(unify_same_params_vars)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x
    addParam(&p, f.t); // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);
    // Aliased params share one variable scope: side 1 gets the same
    // variable IDs as side 0
    assert(u.map[1][0].var == 0);
    assert(u.map[1][1].var == 1);
    assertMapsInSync(&u);

    // Unifying q(x,y) with q(y,x) forces x = y
    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    pddl_fm_atom_t *ayx = newAtomPP(PRED_Q, 1, 0);
    assert(pddlUnify(&u, axy, ayx) == 0);
    assert(u.map[0][0].var == u.map[0][1].var);
    assertMapsInSync(&u);

    // The unifier corresponds to (and (= x y))
    pddl_fm_t *cond = pddlUnifyToCond(&u, PRED_EQ, &p);
    assert(!pddlFmIsTrue(cond));
    int num_atoms = 0;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(cond, &it, eq){
        assert(eq->pred == PRED_EQ);
        assert(eq->arg[0].param == 0);
        assert(eq->arg[1].param == 1);
        ++num_atoms;
    }
    assert(num_atoms == 1);
    pddlFmDel(cond);

    pddlFmDel(&axy->fm);
    pddlFmDel(&ayx->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_identity)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x
    addParam(&p, f.t); // y

    pddl_unify_t u, u2;
    pddlUnifyInit(&u, &f.type, &p, &p);
    pddlUnifyInit(&u2, &f.type, &p, &p);

    // Unifying q(x,y) with itself is a no-op
    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    assert(pddlUnify(&u, axy, axy) == 0);
    assert(pddlUnifyEq(&u, &u2));
    assertMapsInSync(&u);

    pddlFmDel(&axy->fm);
    pddlUnifyFree(&u);
    pddlUnifyFree(&u2);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_var_obj)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    pddl_fm_atom_t *ao1 = newAtomO(PRED_P, OBJ_T1);
    assert(pddlUnify(&u, ax, ao0) == 0);
    // x is bound to OBJ_T0 on both sides
    assert(u.map[0][0].obj == OBJ_T0);
    assert(u.map[0][0].var == -1);
    assertMapsInSync(&u);
    // x is already bound to OBJ_T0, so it cannot unify with OBJ_T1
    assert(pddlUnify(&u, ax, ao1) != 0);
    pddlUnifyFree(&u);

    // Variable of type t cannot be bound to an object of type s
    pddlUnifyInit(&u, &f.type, &p, &p);
    pddl_fm_atom_t *aos = newAtomO(PRED_P, OBJ_S0);
    assert(pddlUnify(&u, ax, aos) != 0);
    pddlUnifyFree(&u);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlFmDel(&ao1->fm);
    pddlFmDel(&aos->fm);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_equality_inequality)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x
    addParam(&p, f.t); // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);

    // On the identity unifier (!= x y) holds
    pddl_fm_t *ineq = newEqCond(0, 1, pddl_true);
    assert(pddlUnifyCheckInequality(&u, &p, PRED_EQ, ineq));

    // Apply (= x y)
    pddl_fm_t *eq = newEqCond(0, 1, pddl_false);
    assert(pddlUnifyApplyEquality(&u, &p, PRED_EQ, eq) == 0);
    assert(u.map[0][0].var == u.map[0][1].var);
    assertMapsInSync(&u);
    // ... and now (!= x y) is violated
    assert(!pddlUnifyCheckInequality(&u, &p, PRED_EQ, ineq));

    pddlFmDel(ineq);
    pddlFmDel(eq);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_atoms_differ)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x
    addParam(&p, f.t); // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);

    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    pddl_fm_atom_t *ayx = newAtomPP(PRED_Q, 1, 0);
    pddl_fm_atom_t *rxy = newAtomPP(PRED_P, 0, 1);
    // On the identity unifier q(x,y) and q(y,x) differ
    assert(pddlUnifyAtomsDiffer(&u, &p, axy, &p, ayx));
    assert(pddlUnify(&u, axy, ayx) == 0);
    // After forcing x = y they are the same atom
    assert(!pddlUnifyAtomsDiffer(&u, &p, axy, &p, ayx));
    // Predicates differ
    assert(pddlUnifyAtomsDiffer(&u, &p, rxy, &p, ayx));

    pddlFmDel(&axy->fm);
    pddlFmDel(&ayx->fm);
    pddlFmDel(&rxy->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_reset_counted_vars)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addCountedParam(&p, f.t); // x
    addParam(&p, f.t);        // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);

    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    assert(pddlUnify(&u, ax, ao0) == 0);
    assert(u.map[0][0].obj == OBJ_T0);
    assertMapsInSync(&u);

    pddlUnifyResetCountedVars(&u);
    // x is a fresh unbound variable again with the same ID on both sides
    assert(u.map[0][0].obj == -1);
    assert(u.map[0][0].var == 0);
    assert(u.map[0][0].var_type == f.t);
    assert(u.map[0][1].var == 1);
    assertMapsInSync(&u);

    // The reset variable can be unified again, with a different object
    pddl_fm_atom_t *ao1 = newAtomO(PRED_P, OBJ_T1);
    assert(pddlUnify(&u, ax, ao1) == 0);
    assert(u.map[0][0].obj == OBJ_T1);
    assertMapsInSync(&u);
    // ... and with the other variable y after another reset
    pddlUnifyResetCountedVars(&u);
    pddl_fm_atom_t *ay = newAtomP(PRED_P, 1);
    assert(pddlUnify(&u, ax, ay) == 0);
    assert(u.map[0][0].var == u.map[0][1].var);
    assert(u.map[0][0].obj == -1);
    assertMapsInSync(&u);

    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlFmDel(&ao1->fm);
    pddlFmDel(&ay->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_init_copy)
{
    fixture_t f;
    fixtureInit(&f);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, f.t); // x
    addParam(&p, f.t); // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &f.type, &p, &p);

    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    pddl_fm_atom_t *ayx = newAtomPP(PRED_Q, 1, 0);
    assert(pddlUnify(&u, axy, ayx) == 0);

    pddl_unify_t c;
    pddlUnifyInitCopy(&c, &u);
    assert(pddlUnifyEq(&c, &u));
    assertMapsInSync(&c);

    // Unification on the copy keeps its sides in sync and does not
    // modify the original
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *ao0 = newAtomO(PRED_P, OBJ_T0);
    assert(pddlUnify(&c, ax, ao0) == 0);
    assert(c.map[0][0].obj == OBJ_T0);
    assert(c.map[0][1].obj == OBJ_T0);
    assertMapsInSync(&c);
    assert(u.map[0][0].obj == -1);
    assert(!pddlUnifyEq(&c, &u));

    pddlFmDel(&axy->fm);
    pddlFmDel(&ayx->fm);
    pddlFmDel(&ax->fm);
    pddlFmDel(&ao0->fm);
    pddlUnifyFree(&c);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    fixtureFree(&f);
}

TEST_ONCE(unify_same_params_type_hierarchy)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, h.vehicle); // x
    addParam(&p, h.car);     // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p, &p);

    // Forcing x = y narrows the merged variable to car on both sides
    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    pddl_fm_atom_t *ayx = newAtomPP(PRED_Q, 1, 0);
    assert(pddlUnify(&u, axy, ayx) == 0);
    assert(u.map[0][0].var == u.map[0][1].var);
    assert(u.map[0][0].var_type == h.car);
    assert(u.map[0][1].var_type == h.car);
    assertMapsInSync(&u);

    // The merged variable cannot be bound to a truck ...
    pddl_fm_atom_t *ax = newAtomP(PRED_P, 0);
    pddl_fm_atom_t *atruck = newAtomO(PRED_P, HOBJ_TRUCK0);
    assert(pddlUnify(&u, ax, atruck) != 0);
    // ... but binding to a car binds both x and y
    pddl_fm_atom_t *acar = newAtomO(PRED_P, HOBJ_CAR1);
    assert(pddlUnify(&u, ax, acar) == 0);
    assert(u.map[0][0].obj == HOBJ_CAR1);
    assert(u.map[0][1].obj == HOBJ_CAR1);
    assertMapsInSync(&u);

    pddlFmDel(&axy->fm);
    pddlFmDel(&ayx->fm);
    pddlFmDel(&ax->fm);
    pddlFmDel(&atruck->fm);
    pddlFmDel(&acar->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    hierFree(&h);
}

TEST_ONCE(unify_same_params_type_empty)
{
    hier_t h;
    hierInit(&h);
    pddl_params_t p;
    pddlParamsInit(&p);
    addParam(&p, h.car);  // x
    addParam(&p, h.bike); // y

    pddl_unify_t u;
    pddlUnifyInit(&u, &h.type, &p, &p);

    // Forcing x = y fails, because the empty type bike is not unifiable
    pddl_fm_atom_t *axy = newAtomPP(PRED_Q, 0, 1);
    pddl_fm_atom_t *ayx = newAtomPP(PRED_Q, 1, 0);
    assert(pddlUnify(&u, axy, ayx) != 0);

    pddlFmDel(&axy->fm);
    pddlFmDel(&ayx->fm);
    pddlUnifyFree(&u);
    pddlParamsFree(&p);
    hierFree(&h);
}
