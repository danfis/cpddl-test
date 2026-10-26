#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdarg.h>

/** Maximal arity of the predicates used in the tests */
#define MAX_ARITY 4
/** Maximal number of facts of a single predicate */
#define MAX_FACTS 64
/** Size of the buffers for string representations */
#define STR_SIZE 2048

/** Specification of an atom: predicate and its arguments (constants or
 *  variables) */
struct atom_spec {
    int pred;
    int arity;
    int arg[MAX_ARITY];
};
typedef struct atom_spec atom_spec_t;

#define ARGC(...) \
    ((int)(sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)))
#define ATOM(P, ...) \
    ((atom_spec_t){ .pred = (P), .arity = ARGC(__VA_ARGS__), \
                    .arg = { __VA_ARGS__ } })
#define ATOM0(P) ((atom_spec_t){ .pred = (P), .arity = 0 })

static int addPred(pddl_datalog_t *dl, int arity, const char *name)
{
    int pred = pddlDatalogAddPred(dl, arity, name);
    pddlDatalogSetPredUserId(dl, pred, name[0]);
    return pred;
}

static int addGoalPred(pddl_datalog_t *dl, const char *name)
{
    int pred = pddlDatalogAddGoalPred(dl, name);
    pddlDatalogSetPredUserId(dl, pred, name[0]);
    return pred;
}

static int addConst(pddl_datalog_t *dl, const char *name)
{
    int c = pddlDatalogAddConst(dl, name);
    pddlDatalogSetConstUserId(dl, c, name[0]);
    return c;
}

static void atomInit(pddl_datalog_t *dl,
                     pddl_datalog_atom_t *atom,
                     const atom_spec_t *spec)
{
    pddlDatalogAtomInit(dl, atom, spec->pred);
    for (int i = 0; i < spec->arity; ++i)
        pddlDatalogAtomSetArg(dl, atom, i, spec->arg[i]);
}

static void ruleInit(pddl_datalog_t *dl,
                     pddl_datalog_rule_t *rule,
                     atom_spec_t head)
{
    pddl_datalog_atom_t atom;
    pddlDatalogRuleInit(dl, rule);
    atomInit(dl, &atom, &head);
    pddlDatalogRuleSetHead(dl, rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
}

static void ruleAddBody(pddl_datalog_t *dl,
                        pddl_datalog_rule_t *rule,
                        atom_spec_t body)
{
    pddl_datalog_atom_t atom;
    atomInit(dl, &atom, &body);
    pddlDatalogRuleAddBody(dl, rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
}

static void ruleAddNeg(pddl_datalog_t *dl,
                       pddl_datalog_rule_t *rule,
                       atom_spec_t neg)
{
    pddl_datalog_atom_t atom;
    atomInit(dl, &atom, &neg);
    pddlDatalogRuleAddNegStaticBody(dl, rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
}

/** Adds the rule to the program and frees it */
static int ruleAdd(pddl_datalog_t *dl, pddl_datalog_rule_t *rule)
{
    int id = pddlDatalogAddRule(dl, rule);
    pddlDatalogRuleFree(dl, rule);
    return id;
}

static int addRuleV(pddl_datalog_t *dl,
                    int weight,
                    pddl_datalog_annotation_fn ann_fn,
                    void *ann_data,
                    atom_spec_t head,
                    int body_size,
                    va_list ap)
{
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, head);
    for (int i = 0; i < body_size; ++i)
        ruleAddBody(dl, &rule, va_arg(ap, atom_spec_t));
    if (weight != 0){
        pddl_cost_t w = pddl_cost_zero;
        w.cost = weight;
        pddlDatalogRuleSetWeight(dl, &rule, &w);
    }
    if (ann_fn != NULL)
        pddlDatalogRuleAddAnnotation(dl, &rule, ann_fn, ann_data);
    return ruleAdd(dl, &rule);
}

/** Adds rule HEAD :- BODY_1, ..., BODY_n where n = BODY_SIZE */
static int rule(pddl_datalog_t *dl, atom_spec_t head, int body_size, ...)
{
    va_list ap;
    va_start(ap, body_size);
    int id = addRuleV(dl, 0, NULL, NULL, head, body_size, ap);
    va_end(ap);
    return id;
}

/** Same as rule(), but with the weight WEIGHT */
static int wrule(pddl_datalog_t *dl,
                 int weight,
                 atom_spec_t head,
                 int body_size, ...)
{
    va_list ap;
    va_start(ap, body_size);
    int id = addRuleV(dl, weight, NULL, NULL, head, body_size, ap);
    va_end(ap);
    return id;
}

/** Same as rule(), but with the annotation ANN_FN(ANN_DATA) */
static int arule(pddl_datalog_t *dl,
                 pddl_datalog_annotation_fn ann_fn,
                 void *ann_data,
                 atom_spec_t head,
                 int body_size, ...)
{
    va_list ap;
    va_start(ap, body_size);
    int id = addRuleV(dl, 0, ann_fn, ann_data, head, body_size, ap);
    va_end(ap);
    return id;
}

/** Adds body-less rule ATOM */
static int fact(pddl_datalog_t *dl, atom_spec_t atom)
{
    return rule(dl, atom, 0);
}

static void addFactToDB(pddl_datalog_t *dl,
                        atom_spec_t atom,
                        const pddl_cost_t *weight)
{
    pddlDatalogAddFactToDB(dl, atom.pred, atom.arg, weight);
}

static void toNormalForm(pddl_datalog_t *dl)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
}

/** Set of facts represented as strings */
struct facts {
    int size;
    char fact[MAX_FACTS][32];
};
typedef struct facts facts_t;

/** Adds string representation of the fact: user IDs of the predicate and
 *  the arguments (which are characters in the tests) followed by the weight
 *  if WEIGHT is non-NULL */
static void factsAdd(facts_t *fs,
                     int pred,
                     int arity,
                     const int *arg,
                     const pddl_cost_t *weight)
{
    assert(fs->size < MAX_FACTS);
    char *s = fs->fact[fs->size++];
    int len = 0;
    s[len++] = pred;
    for (int i = 0; i < arity; ++i)
        s[len++] = arg[i];
    s[len] = '\x0';
    if (weight != NULL){
        len += snprintf(s + len, 32 - len, ":%d", weight->cost);
        if (weight->zero_cost != 0)
            snprintf(s + len, 32 - len, "~%d", weight->zero_cost);
    }
}

static int cmpStr(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/** Writes the sorted facts separated by spaces to OUT */
static void factsJoin(facts_t *fs, char *out)
{
    qsort(fs->fact, fs->size, sizeof(fs->fact[0]), cmpStr);
    int len = 0;
    out[0] = '\x0';
    for (int i = 0; i < fs->size; ++i){
        len += snprintf(out + len, STR_SIZE - len, "%s%s",
                        (i > 0 ? " " : ""), fs->fact[i]);
    }
}

static void assertStrEq(const char *got, const char *expected)
{
    if (strcmp(got, expected) != 0)
        fprintf(stderr, "Expected:\n'%s'\nGot:\n'%s'\n", expected, got);
    fflush(stderr);
    assert(strcmp(got, expected) == 0);
}

static void factCB(int pred, int arity, const int *arg, void *ud)
{
    factsAdd(ud, pred, arity, arg, NULL);
}

static void weightedFactCB(int pred,
                           int arity,
                           const int *arg,
                           const pddl_cost_t *weight,
                           void *ud)
{
    factsAdd(ud, pred, arity, arg, weight);
}

/** Asserts that the facts of PRED are EXPECTED */
static void assertFacts(pddl_datalog_t *dl, int pred, const char *expected)
{
    facts_t fs;
    fs.size = 0;
    pddlDatalogFactsFromCanonicalModel(dl, pred, factCB, &fs);
    char out[STR_SIZE];
    factsJoin(&fs, out);
    assertStrEq(out, expected);
}

/** Asserts that the facts of PRED with their weights are EXPECTED */
static void assertWeightedFacts(pddl_datalog_t *dl,
                                int pred,
                                const char *expected)
{
    facts_t fs;
    fs.size = 0;
    pddlDatalogFactsFromWeightedCanonicalModel(dl, pred, weightedFactCB, &fs);
    char out[STR_SIZE];
    factsJoin(&fs, out);
    assertStrEq(out, expected);
}

/** Number of facts in the database */
static int dbSize(pddl_datalog_t *dl)
{
    int pred, arity, arg[MAX_ARITY];
    int size = 0;
    while (pddlDatalogFact(dl, size, &pred, &arity, arg, NULL) == 0)
        ++size;
    return size;
}

/** Asserts that all facts in the database are EXPECTED */
static void assertDB(pddl_datalog_t *dl, const char *expected)
{
    facts_t fs;
    fs.size = 0;
    int size = dbSize(dl);
    for (int id = 0; id < size; ++id){
        int pred, arity, arg[MAX_ARITY];
        int ret = pddlDatalogFact(dl, id, &pred, &arity, arg, NULL);
        assert(ret == 0);
        factsAdd(&fs, pred, arity, arg, NULL);
    }
    char out[STR_SIZE];
    factsJoin(&fs, out);
    assertStrEq(out, expected);
}

/** Asserts that the program printed by pddlDatalogPrint() is EXPECTED */
static void assertProgram(const pddl_datalog_t *dl, const char *expected)
{
    FILE *fout = tmpfile();
    assert(fout != NULL);
    pddlDatalogPrint(dl, fout);
    long size = ftell(fout);
    assert(size >= 0 && size < STR_SIZE);
    rewind(fout);
    char buf[STR_SIZE];
    size_t read = fread(buf, 1, size, fout);
    assert(read == (size_t)size);
    buf[size] = '\x0';
    fclose(fout);
    assertStrEq(buf, expected);
}

/** Buffer for the output of annotations */
struct ann_out {
    int len;
    char buf[STR_SIZE];
};
typedef struct ann_out ann_out_t;

/** Annotation data: label and the output buffer */
struct ann_data {
    const char *label;
    ann_out_t *out;
};
typedef struct ann_data ann_data_t;

/** Appends "label | (H a b) :- (B1 c) (B2 d).\n" to the output buffer,
 *  the body facts are sorted */
static void annotation(pddl_datalog_t *dl,
                       int head_fact_id,
                       const pddl_iset_t *body_fact_ids,
                       void *userdata)
{
    ann_data_t *d = userdata;
    ann_out_t *out = d->out;
    int pred, arity, arg[MAX_ARITY];

    int st = pddlDatalogFact(dl, head_fact_id, &pred, &arity, arg, NULL);
    assert(st == 0);
    out->len += snprintf(out->buf + out->len, STR_SIZE - out->len,
                         "%s | (%c", d->label, pred);
    for (int i = 0; i < arity; ++i){
        out->len += snprintf(out->buf + out->len, STR_SIZE - out->len,
                             " %c", arg[i]);
    }
    out->len += snprintf(out->buf + out->len, STR_SIZE - out->len, ") :-");

    facts_t fs;
    fs.size = 0;
    PDDL_ISET_FOR_EACH(body_fact_ids, fact_id){
        int st = pddlDatalogFact(dl, fact_id, &pred, &arity, arg, NULL);
        assert(st == 0);
        factsAdd(&fs, pred, arity, arg, NULL);
    }
    qsort(fs.fact, fs.size, sizeof(fs.fact[0]), cmpStr);
    for (int i = 0; i < fs.size; ++i){
        out->len += snprintf(out->buf + out->len, STR_SIZE - out->len,
                             " (%c", fs.fact[i][0]);
        for (int j = 1; fs.fact[i][j] != '\x0'; ++j){
            out->len += snprintf(out->buf + out->len, STR_SIZE - out->len,
                                 " %c", fs.fact[i][j]);
        }
        out->len += snprintf(out->buf + out->len, STR_SIZE - out->len, ")");
    }
    out->len += snprintf(out->buf + out->len, STR_SIZE - out->len, ".\n");
}

/*
 * Normal form and canonical model of a small program with a rule with three
 * body atoms.
 */
TEST_ONCE(datalog)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 2, "P");
    int Q = addPred(dl, 2, "Q");
    int R = addPred(dl, 3, "R");
    int S = addPred(dl, 2, "S");
    int c1 = addConst(dl, "1");
    int c2 = addConst(dl, "2");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(P, c1, c2));
    fact(dl, ATOM(Q, c2, c1));
    rule(dl, ATOM(Q, X, Y), 1, ATOM(P, X, Y));
    fact(dl, ATOM(R, c2, c1, c1));
    rule(dl, ATOM(R, X, Y, W), 3,
         ATOM(P, X, Y), ATOM(Q, Y, Z), ATOM(S, Z, W));

    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "Q(X, Y) :- P(X, Y).\n"
                  "R(2, 1, 1).\n"
                  "R(X, Y, W) :- P(X, Y), Q(Y, Z), S(Z, W).\n");

    toNormalForm(dl);
    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "Q(X, Y) :- P(X, Y).\n"
                  "R(2, 1, 1).\n"
                  "R(X, Y, Z) :- P(X, Y), _p4(Y, Z).\n"
                  "_p4(X, Z) :- Q(X, Y), S(Y, Z).\n");
    assertFacts(dl, P, "P12");
    assertFacts(dl, Q, "Q12 Q21");
    assertFacts(dl, R, "R211");
    assertFacts(dl, S, "");
    pddlDatalogDel(dl);
}

/*
 * Annotations executed over the derivation tree of the goal of a program
 * with a rule with three body atoms.
 */
TEST_ONCE(datalog_annotated)
{
    ann_out_t out;
    out.len = 0;
    out.buf[0] = '\x0';
    ann_data_t ann_q = { "Q(X,Y)", &out };
    ann_data_t ann_r211 = { "R(2,1,1)", &out };
    ann_data_t ann_r = { "R(X,Y,W)", &out };
    ann_data_t ann_g = { "G", &out };

    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 2, "P");
    int Q = addPred(dl, 2, "Q");
    int R = addPred(dl, 3, "R");
    int S = addPred(dl, 2, "S");
    int G = addGoalPred(dl, "G");
    int c1 = addConst(dl, "1");
    int c2 = addConst(dl, "2");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(P, c1, c2));
    fact(dl, ATOM(Q, c2, c1));
    fact(dl, ATOM(S, c1, c2));
    arule(dl, annotation, &ann_q, ATOM(Q, X, Y), 1, ATOM(P, X, Y));
    arule(dl, annotation, &ann_r211, ATOM(R, c2, c1, c1), 0);
    arule(dl, annotation, &ann_r, ATOM(R, X, Y, W), 3,
          ATOM(P, X, Y), ATOM(Q, Y, Z), ATOM(S, Z, W));
    arule(dl, annotation, &ann_g, ATOM0(G), 1, ATOM(R, c1, c2, c2));

    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "S(1, 2).\n"
                  "Q(X, Y) :- P(X, Y).; ann_size = 1\n"
                  "R(2, 1, 1).; ann_size = 1\n"
                  "R(X, Y, W) :- P(X, Y), Q(Y, Z), S(Z, W).; ann_size = 1\n"
                  "G() :- R(1, 2, 2).; ann_size = 1\n");

    toNormalForm(dl);
    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "S(1, 2).\n"
                  "Q(X, Y) :- P(X, Y).; ann_size = 1\n"
                  "R(2, 1, 1).; ann_size = 1\n"
                  "R(X, Y, Z) :- P(X, Y), _p5(Y, Z).; ann_size = 1\n"
                  "G() :- R(1, 2, 2).; ann_size = 1\n"
                  "_p5(X, Z) :- Q(X, Y), S(Y, Z).\n");

    assertFacts(dl, P, "P12");
    assertFacts(dl, Q, "Q12 Q21");
    assertFacts(dl, R, "R122 R211");
    assertFacts(dl, S, "S12");
    assertFacts(dl, G, "G");

    pddlDatalogExecuteAnnotations(dl, G);
    assertStrEq(out.buf,
                "R(X,Y,W) | (R 1 2 2) :- (P 1 2) (Q 2 1) (S 1 2).\n"
                "G | (G) :- (R 1 2 2).\n");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by graphProgram() */
struct graph {
    int E, T, L, K, M, Z, W;
};
typedef struct graph graph_t;

/** Creates a program over the graph a -> b -> c -> a, d -> d computing
 *  transitive closure (T), self-loops (L), nodes reachable from a (K), a
 *  constant in the head (M), existence of a cycle (Z), and nodes on
 *  triangles (W) */
static void graphProgram(pddl_datalog_t *dl, graph_t *g)
{
    g->E = addPred(dl, 2, "E");
    g->T = addPred(dl, 2, "T");
    g->L = addPred(dl, 1, "L");
    g->K = addPred(dl, 1, "K");
    g->M = addPred(dl, 2, "M");
    g->Z = addPred(dl, 0, "Z");
    g->W = addPred(dl, 1, "W");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(g->E, a, b));
    fact(dl, ATOM(g->E, b, c));
    fact(dl, ATOM(g->E, c, a));
    fact(dl, ATOM(g->E, d, d));
    rule(dl, ATOM(g->T, X, Y), 1, ATOM(g->E, X, Y));
    rule(dl, ATOM(g->T, X, Z), 2, ATOM(g->T, X, Y), ATOM(g->E, Y, Z));
    rule(dl, ATOM(g->L, X), 1, ATOM(g->E, X, X));
    rule(dl, ATOM(g->K, X), 1, ATOM(g->T, a, X));
    rule(dl, ATOM(g->M, X, a), 1, ATOM(g->E, X, Y));
    rule(dl, ATOM0(g->Z), 1, ATOM(g->T, X, X));
    rule(dl, ATOM(g->W, X), 3,
         ATOM(g->E, X, Y), ATOM(g->E, Y, Z), ATOM(g->E, Z, X));
}

static void assertGraphModel(pddl_datalog_t *dl, const graph_t *g)
{
    assertFacts(dl, g->E, "Eab Ebc Eca Edd");
    assertFacts(dl, g->T, "Taa Tab Tac Tba Tbb Tbc Tca Tcb Tcc Tdd");
    assertFacts(dl, g->L, "Ld");
    assertFacts(dl, g->K, "Ka Kb Kc");
    assertFacts(dl, g->M, "Maa Mba Mca Mda");
    assertFacts(dl, g->Z, "Z");
    assertFacts(dl, g->W, "Wa Wb Wc Wd");
}

/*
 * Canonical model with recursion, constants in heads and bodies, repeated
 * variables, a zero-arity predicate, and a rule with three body atoms.
 */
TEST_ONCE(datalog_canonical_model)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    graph_t g;
    graphProgram(dl, &g);
    toNormalForm(dl);
    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertGraphModel(dl, &g);
    pddlDatalogDel(dl);
}

/*
 * Facts added to the database after the canonical model are processed by
 * pddlDatalogCanonicalModelCont(); continuing without new facts or with
 * already known facts changes nothing.
 */
TEST_ONCE(datalog_canonical_model_cont)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = addPred(dl, 2, "E");
    int T = addPred(dl, 2, "T");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(E, a, b));
    rule(dl, ATOM(T, X, Y), 1, ATOM(E, X, Y));
    rule(dl, ATOM(T, X, Z), 2, ATOM(T, X, Y), ATOM(E, Y, Z));
    toNormalForm(dl);

    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, T, "Tab");
    assert(dbSize(dl) == 2);

    pddlDatalogCanonicalModelCont(dl, &err);
    assertFacts(dl, T, "Tab");
    assert(dbSize(dl) == 2);

    addFactToDB(dl, ATOM(E, b, c), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertFacts(dl, E, "Eab Ebc");
    assertFacts(dl, T, "Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    addFactToDB(dl, ATOM(E, a, b), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertFacts(dl, T, "Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    addFactToDB(dl, ATOM(E, c, d), NULL);
    addFactToDB(dl, ATOM(E, d, a), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertFacts(dl, E, "Eab Ebc Ecd Eda");
    assertFacts(dl, T, "Taa Tab Tac Tad Tba Tbb Tbc Tbd"
                       " Tca Tcb Tcc Tcd Tda Tdb Tdc Tdd");
    pddlDatalogDel(dl);
}

/*
 * Normal form: rules with more than two body atoms are split, auxiliary
 * predicates with identical definitions are merged, and an unsafe program
 * cannot be normalized.
 */
TEST_ONCE(datalog_normal_form)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int C = addPred(dl, 2, "C");
    int D = addPred(dl, 1, "D");
    int H = addPred(dl, 2, "H");
    int I = addPred(dl, 1, "I");
    int J = addPred(dl, 1, "J");
    int M = addPred(dl, 1, "M");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(A, b, b));
    fact(dl, ATOM(B, b, c));
    fact(dl, ATOM(C, c, d));
    fact(dl, ATOM(D, d));
    fact(dl, ATOM(D, c));
    rule(dl, ATOM(H, X, W), 4,
         ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(C, Z, W), ATOM(D, W));
    rule(dl, ATOM(I, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    rule(dl, ATOM(J, Z), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    rule(dl, ATOM(M, Z), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    assert(pddlDatalogIsSafe(dl));

    toNormalForm(dl);
    assert(pddlDatalogIsSafe(dl));
    assertProgram(dl,
                  "A(a, b).\n"
                  "A(b, b).\n"
                  "B(b, c).\n"
                  "C(c, d).\n"
                  "D(d).\n"
                  "D(c).\n"
                  "H(X, Y) :- D(Y), _p9(X, Y).\n"
                  "I(X) :- A(X, Y), _p10(Y).\n"
                  "J(X) :- D(X), _p11(X).\n"
                  "M(X) :- D(X), _p11(X).\n"
                  "_p8(X, Z) :- A(X, Y), B(Y, Z).\n"
                  "_p9(X, Z) :- C(Y, Z), _p8(X, Y).\n"
                  "_p10(X) :- B(X, Y), D(Y).\n"
                  "_p11(Z) :- A(X, Y), B(Y, Z).\n");

    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, H, "Had Hbd");
    assertFacts(dl, I, "Ia Ib");
    assertFacts(dl, J, "Jc");
    assertFacts(dl, M, "Mc");
    pddlDatalogDel(dl);

    // Unsafe program: the variable Y of the head is not in the body
    dl = pddlDatalogNew();
    A = addPred(dl, 2, "A");
    H = addPred(dl, 2, "H");
    a = addConst(dl, "a");
    X = pddlDatalogAddVar(dl, "X");
    Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(A, a, a));
    rule(dl, ATOM(H, X, Y), 2, ATOM(A, X, X), ATOM(A, X, a));
    assert(!pddlDatalogIsSafe(dl));
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);
    pddlDatalogDel(dl);
}

/** Predicates of the program created by weightedProgram() */
struct weighted {
    int At, L, K, G;
    int a, b, c, d;
};
typedef struct weighted weighted_t;

/** Creates a weighted program: At(a) (unless AT_A_IN_DB is true, then it
 *  must be added to the database), links a -> b -> c, a -> c, moving along
 *  a link costs 1, a key at a reached node costs 2, and the goal requires
 *  At(b), At(c), and K(c) (or At(d) if REACHABLE_GOAL is false) */
static void weightedProgram(pddl_datalog_t *dl,
                            weighted_t *w,
                            pddl_bool_t at_a_in_db,
                            pddl_bool_t reachable_goal)
{
    w->At = addPred(dl, 1, "A");
    w->L = addPred(dl, 2, "L");
    w->K = addPred(dl, 1, "K");
    w->G = addGoalPred(dl, "G");
    w->a = addConst(dl, "a");
    w->b = addConst(dl, "b");
    w->c = addConst(dl, "c");
    w->d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");

    if (!at_a_in_db)
        fact(dl, ATOM(w->At, w->a));
    fact(dl, ATOM(w->L, w->a, w->b));
    fact(dl, ATOM(w->L, w->b, w->c));
    fact(dl, ATOM(w->L, w->a, w->c));
    wrule(dl, 1, ATOM(w->At, Y), 2, ATOM(w->At, X), ATOM(w->L, X, Y));
    wrule(dl, 2, ATOM(w->K, X), 1, ATOM(w->At, X));
    if (reachable_goal){
        rule(dl, ATOM0(w->G), 3,
             ATOM(w->At, w->b), ATOM(w->At, w->c), ATOM(w->K, w->c));
    }else{
        rule(dl, ATOM0(w->G), 1, ATOM(w->At, w->d));
    }
    toNormalForm(dl);
}

/*
 * Weighted canonical model with the sum of weights: the goal weight is the
 * h^add value; an unreachable goal is reported and all reachable facts get
 * their final weights.
 */
TEST_ONCE(datalog_weighted_add)
{
    pddl_err_t err = PDDL_ERR_INIT;
    weighted_t w;
    pddl_cost_t weight;

    pddl_datalog_t *dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_false, pddl_true);
    int ret = pddlDatalogWeightedCanonicalModelAdd(dl, &weight, 0, &err);
    assert(ret == 0);
    assert(weight.cost == 5 && weight.zero_cost == 0);
    pddlDatalogDel(dl);

    // At(a) added to the database with zero weight
    dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_true, pddl_true);
    pddlDatalogResetDB(dl);
    addFactToDB(dl, ATOM(w.At, w.a), &pddl_cost_zero);
    ret = pddlDatalogWeightedCanonicalModelAdd(dl, &weight, 0, &err);
    assert(ret == 0);
    assert(weight.cost == 5 && weight.zero_cost == 0);
    pddlDatalogDel(dl);

    dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_false, pddl_false);
    ret = pddlDatalogWeightedCanonicalModelAdd(dl, &weight, 0, &err);
    assert(ret == -1);
    assertWeightedFacts(dl, w.At, "Aa:0 Ab:1 Ac:1");
    assertWeightedFacts(dl, w.L, "Lab:0 Lac:0 Lbc:0");
    assertWeightedFacts(dl, w.K, "Ka:2 Kb:3 Kc:3");
    assertWeightedFacts(dl, w.G, "");
    pddlDatalogDel(dl);
}

/*
 * Weighted canonical model with the maximum of weights: the goal weight is
 * the h^max value; an unreachable goal is reported and all reachable facts
 * get their final weights.
 */
TEST_ONCE(datalog_weighted_max)
{
    pddl_err_t err = PDDL_ERR_INIT;
    weighted_t w;
    pddl_cost_t weight;

    pddl_datalog_t *dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_false, pddl_true);
    int ret = pddlDatalogWeightedCanonicalModelMax(dl, &weight, 0, &err);
    assert(ret == 0);
    assert(weight.cost == 3 && weight.zero_cost == 0);
    pddlDatalogDel(dl);

    // At(a) added to the database with zero weight
    dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_true, pddl_true);
    pddlDatalogResetDB(dl);
    addFactToDB(dl, ATOM(w.At, w.a), &pddl_cost_zero);
    ret = pddlDatalogWeightedCanonicalModelMax(dl, &weight, 0, &err);
    assert(ret == 0);
    assert(weight.cost == 3 && weight.zero_cost == 0);
    pddlDatalogDel(dl);

    dl = pddlDatalogNew();
    weightedProgram(dl, &w, pddl_false, pddl_false);
    ret = pddlDatalogWeightedCanonicalModelMax(dl, &weight, 0, &err);
    assert(ret == -1);
    assertWeightedFacts(dl, w.At, "Aa:0 Ab:1 Ac:1");
    assertWeightedFacts(dl, w.L, "Lab:0 Lac:0 Lbc:0");
    assertWeightedFacts(dl, w.K, "Ka:2 Kb:3 Kc:3");
    assertWeightedFacts(dl, w.G, "");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by negProgram() */
struct neg {
    int T, S, R, E, H, N, J;
};
typedef struct neg neg_t;

/** Creates a program with negative static atoms in the bodies */
static void negProgram(pddl_datalog_t *dl, neg_t *n)
{
    n->T = addPred(dl, 1, "T");
    n->S = addPred(dl, 1, "S");
    n->R = addPred(dl, 2, "R");
    n->E = addPred(dl, 2, "E");
    n->H = addPred(dl, 1, "H");
    n->N = addPred(dl, 1, "N");
    n->J = addPred(dl, 1, "J");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");

    fact(dl, ATOM(n->T, a));
    fact(dl, ATOM(n->T, b));
    fact(dl, ATOM(n->T, c));
    fact(dl, ATOM(n->S, b));
    fact(dl, ATOM(n->R, c, a));
    fact(dl, ATOM(n->E, a, b));
    fact(dl, ATOM(n->E, a, c));
    fact(dl, ATOM(n->E, b, c));

    pddl_datalog_rule_t r;
    // H(X) :- T(X), !S(X).
    ruleInit(dl, &r, ATOM(n->H, X));
    ruleAddBody(dl, &r, ATOM(n->T, X));
    ruleAddNeg(dl, &r, ATOM(n->S, X));
    ruleAdd(dl, &r);

    // N(X) :- T(X), !R(X, a).
    ruleInit(dl, &r, ATOM(n->N, X));
    ruleAddBody(dl, &r, ATOM(n->T, X));
    ruleAddNeg(dl, &r, ATOM(n->R, X, a));
    ruleAdd(dl, &r);

    // J(X) :- T(X), T(Y), E(X, Y), !S(Y).
    ruleInit(dl, &r, ATOM(n->J, X));
    ruleAddBody(dl, &r, ATOM(n->T, X));
    ruleAddBody(dl, &r, ATOM(n->T, Y));
    ruleAddBody(dl, &r, ATOM(n->E, X, Y));
    ruleAddNeg(dl, &r, ATOM(n->S, Y));
    ruleAdd(dl, &r);
}

static void assertNegModel(pddl_datalog_t *dl, const neg_t *n)
{
    assertFacts(dl, n->H, "Ha Hc");
    assertFacts(dl, n->N, "Na Nb");
    assertFacts(dl, n->J, "Ja Jb");
}

/*
 * Negative static atoms in the bodies of rules, including a rule that is
 * split by the normalization.
 */
TEST_ONCE(datalog_neg_static)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    neg_t n;
    negProgram(dl, &n);
    toNormalForm(dl);
    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    assertNegModel(dl, &n);
    pddlDatalogDel(dl);
}

/*
 * Removing rules with pddlDatalogRmLastRules() and pddlDatalogRmRules()
 * changes the canonical model computed afterwards.
 */
TEST_ONCE(datalog_rm_rules)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int Q = addPred(dl, 1, "Q");
    int R = addPred(dl, 1, "R");
    int S = addPred(dl, 1, "S");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    assert(fact(dl, ATOM(P, a)) == 0);
    assert(fact(dl, ATOM(P, b)) == 1);
    assert(rule(dl, ATOM(Q, X), 1, ATOM(P, X)) == 2);
    assert(rule(dl, ATOM(R, X), 1, ATOM(Q, X)) == 3);
    assert(rule(dl, ATOM(S, X), 1, ATOM(P, X)) == 4);

    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, P, "Pa Pb");
    assertFacts(dl, Q, "Qa Qb");
    assertFacts(dl, R, "Ra Rb");
    assertFacts(dl, S, "Sa Sb");

    // Removes S(X) :- P(X).
    pddlDatalogRmLastRules(dl, 1);
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, P, "Pa Pb");
    assertFacts(dl, Q, "Qa Qb");
    assertFacts(dl, R, "Ra Rb");
    assertFacts(dl, S, "");

    // Removes P(a) and Q(X) :- P(X).
    PDDL_ISET(rm);
    pddlISetAdd(&rm, 0);
    pddlISetAdd(&rm, 2);
    pddlDatalogRmRules(dl, &rm);
    pddlISetFree(&rm);
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, P, "Pb");
    assertFacts(dl, Q, "");
    assertFacts(dl, R, "");

    // The remaining rules are P(b) and R(X) :- Q(X)
    assert(rule(dl, ATOM(Q, X), 1, ATOM(P, X)) == 2);
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, P, "Pb");
    assertFacts(dl, Q, "Qb");
    assertFacts(dl, R, "Rb");

    // Removing more rules than there are removes all of them
    pddlDatalogRmLastRules(dl, 10);
    pddlDatalogCanonicalModel(dl, &err);
    assert(dbSize(dl) == 0);
    pddlDatalogDel(dl);
}

/*
 * Facts added to the database with pddlDatalogAddFactToDB(), reading facts
 * with pddlDatalogFact(), and clearing the database with
 * pddlDatalogResetDB().
 */
TEST_ONCE(datalog_db)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = addPred(dl, 2, "E");
    int T = addPred(dl, 2, "T");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(E, a, b));
    rule(dl, ATOM(T, X, Y), 1, ATOM(E, X, Y));
    rule(dl, ATOM(T, X, Z), 2, ATOM(T, X, Y), ATOM(E, Y, Z));
    toNormalForm(dl);

    pddlDatalogResetDB(dl);
    assert(dbSize(dl) == 0);
    addFactToDB(dl, ATOM(E, b, c), NULL);
    assertDB(dl, "Ebc");
    pddlDatalogCanonicalModel(dl, &err);
    assertDB(dl, "Eab Ebc Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    int pred, arity, arg[MAX_ARITY];
    pddl_cost_t weight;
    assert(pddlDatalogFact(dl, -1, &pred, &arity, arg, NULL) == -1);
    assert(pddlDatalogFact(dl, 5, &pred, &arity, arg, NULL) == -1);
    assert(pddlDatalogFact(dl, 0, &pred, &arity, arg, &weight) == 0);
    assert(pddlDatalogFact(dl, 4, &pred, &arity, arg, NULL) == 0);
    assert(arity == 2);

    pddlDatalogResetDB(dl);
    assert(dbSize(dl) == 0);
    assert(pddlDatalogFact(dl, 0, &pred, &arity, arg, NULL) == -1);
    assertFacts(dl, E, "");
    assertFacts(dl, T, "");

    pddlDatalogCanonicalModel(dl, &err);
    assertDB(dl, "Eab Tab");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by rollbackProgram() */
struct rollback {
    int A, C, D, H, B, K;
    int a, b, c, c1, c2, c5, c7;
};
typedef struct rollback rollback_t;

/** Creates a program with H(X, Z) :- A(X, Y), C(Y, Z), static C(1, 1) and
 *  C(2, 2), computes its canonical model and saves the state of the
 *  database */
static void rollbackProgram(pddl_datalog_t *dl, rollback_t *r)
{
    r->A = addPred(dl, 2, "A");
    r->C = addPred(dl, 2, "C");
    r->D = addPred(dl, 2, "D");
    r->H = addPred(dl, 2, "H");
    r->B = addPred(dl, 1, "B");
    r->K = addPred(dl, 1, "K");
    r->a = addConst(dl, "a");
    r->b = addConst(dl, "b");
    r->c = addConst(dl, "c");
    r->c1 = addConst(dl, "1");
    r->c2 = addConst(dl, "2");
    r->c5 = addConst(dl, "5");
    r->c7 = addConst(dl, "7");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(r->C, r->c1, r->c1));
    fact(dl, ATOM(r->C, r->c2, r->c2));
    rule(dl, ATOM(r->H, X, Z), 2, ATOM(r->A, X, Y), ATOM(r->C, Y, Z));
    rule(dl, ATOM(r->B, X), 1, ATOM(r->A, X, Y));
    rule(dl, ATOM(r->K, Z), 1, ATOM(r->H, X, Z));
    toNormalForm(dl);

    pddl_err_t err = PDDL_ERR_INIT;
    pddlDatalogCanonicalModel(dl, &err);
    pddlDatalogSaveStateOfDB(dl);
}

static void assertRollbackSaved(pddl_datalog_t *dl, const rollback_t *r)
{
    assertDB(dl, "C11 C22");
    assertFacts(dl, r->A, "");
    assertFacts(dl, r->H, "");
    assertFacts(dl, r->B, "");
    assertFacts(dl, r->K, "");
}

/*
 * Saving and rolling back the database, including repeated rollbacks to
 * the same saved state.
 */
TEST_ONCE(datalog_rollback)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    assertRollbackSaved(dl, &r);

    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(r.A, r.a, r.c1), NULL);
        pddlDatalogCanonicalModelCont(dl, &err);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");
        pddlDatalogRollbackDB(dl);
        assertRollbackSaved(dl, &r);

        addFactToDB(dl, ATOM(r.A, r.b, r.c2), NULL);
        addFactToDB(dl, ATOM(r.A, r.c, r.c1), NULL);
        addFactToDB(dl, ATOM(r.A, r.c, r.c5), NULL);
        pddlDatalogCanonicalModelCont(dl, &err);
        assertDB(dl, "Ab2 Ac1 Ac5 Bb Bc C11 C22 Hb2 Hc1 K1 K2");
        pddlDatalogRollbackDB(dl);
        assertRollbackSaved(dl, &r);

        // Rollback without any change
        pddlDatalogRollbackDB(dl);
        assertRollbackSaved(dl, &r);

        // Facts added without the computation of the canonical model
        addFactToDB(dl, ATOM(r.A, r.a, r.c2), NULL);
        assertDB(dl, "Aa2 C11 C22");
        pddlDatalogRollbackDB(dl);
        assertRollbackSaved(dl, &r);
    }
    pddlDatalogDel(dl);
}

/*
 * Rollback removes the IDs of the facts added after the save from the join
 * index, because these IDs are re-used by the facts added after the
 * rollback: C(1, 2) added after the save joins the static C(1, 1) in the
 * index of H(X, Z) :- A(X, Y), C(Y, Z), and after the rollback its ID is
 * re-used by D(5, 7), which must not be joined with A(b, 1).
 */
TEST_ONCE(datalog_rollback_reused_ids)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.C, r.c1, r.c2), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertDB(dl, "C11 C12 C22");
    pddlDatalogRollbackDB(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
    addFactToDB(dl, ATOM(r.A, r.b, r.c1), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertDB(dl, "Ab1 Bb C11 C22 D57 Hb1 K1");
    pddlDatalogRollbackDB(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.A, r.a, r.c1), NULL);
    addFactToDB(dl, ATOM(r.C, r.c1, r.c7), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertDB(dl, "Aa1 Ba C11 C17 C22 Ha1 Ha7 K1 K7");
    pddlDatalogRollbackDB(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
    addFactToDB(dl, ATOM(r.D, r.c7, r.c5), NULL);
    addFactToDB(dl, ATOM(r.A, r.b, r.c1), NULL);
    pddlDatalogCanonicalModelCont(dl, &err);
    assertDB(dl, "Ab1 Bb C11 C22 D57 D75 Hb1 K1");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by chainProgram() */
struct chain {
    int At, L, G;
    int a, b, c, d;
};
typedef struct chain chain_t;

/** Creates a program with At(a), links a -> b -> c, d -> a, moving
 *  At(Y) :- At(X), L(X, Y) annotated with ANN_MOVE, and the goal
 *  G :- At(c), At(b) annotated with ANN_GOAL */
static void chainProgram(pddl_datalog_t *dl,
                         chain_t *ch,
                         ann_data_t *ann_move,
                         ann_data_t *ann_goal)
{
    ch->At = addPred(dl, 1, "A");
    ch->L = addPred(dl, 2, "L");
    ch->G = addGoalPred(dl, "G");
    ch->a = addConst(dl, "a");
    ch->b = addConst(dl, "b");
    ch->c = addConst(dl, "c");
    ch->d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");

    fact(dl, ATOM(ch->At, ch->a));
    fact(dl, ATOM(ch->L, ch->a, ch->b));
    fact(dl, ATOM(ch->L, ch->b, ch->c));
    fact(dl, ATOM(ch->L, ch->d, ch->a));
    arule(dl, annotation, ann_move, ATOM(ch->At, Y), 2,
          ATOM(ch->At, X), ATOM(ch->L, X, Y));
    arule(dl, annotation, ann_goal, ATOM0(ch->G), 2,
          ATOM(ch->At, ch->c), ATOM(ch->At, ch->b));
    toNormalForm(dl);
}

/*
 * Annotations are executed in the order of the derivation tree of the goal,
 * once for each derived fact, even if the fact is used multiple times.
 */
TEST_ONCE(datalog_annotations)
{
    ann_out_t out;
    out.len = 0;
    out.buf[0] = '\x0';
    ann_data_t ann_move = { "move", &out };
    ann_data_t ann_goal = { "goal", &out };
    pddl_err_t err = PDDL_ERR_INIT;

    pddl_datalog_t *dl = pddlDatalogNew();
    chain_t ch;
    chainProgram(dl, &ch, &ann_move, &ann_goal);
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, ch.At, "Aa Ab Ac");
    assertFacts(dl, ch.G, "G");
    pddlDatalogExecuteAnnotations(dl, ch.G);
    assertStrEq(out.buf,
                "move | (A b) :- (A a) (L a b).\n"
                "move | (A c) :- (A b) (L b c).\n"
                "goal | (G) :- (A b) (A c).\n");
    pddlDatalogDel(dl);

    // At(d) added to the database derives At(a) again, but the derivation
    // tree keeps the first derivation of At(a) (the body-less rule)
    out.len = 0;
    out.buf[0] = '\x0';
    dl = pddlDatalogNew();
    chainProgram(dl, &ch, &ann_move, &ann_goal);
    pddlDatalogResetDB(dl);
    addFactToDB(dl, ATOM(ch.At, ch.d), NULL);
    pddlDatalogCanonicalModel(dl, &err);
    assertFacts(dl, ch.At, "Aa Ab Ac Ad");
    pddlDatalogExecuteAnnotations(dl, ch.G);
    assertStrEq(out.buf,
                "move | (A b) :- (A a) (L a b).\n"
                "move | (A c) :- (A b) (L b c).\n"
                "goal | (G) :- (A b) (A c).\n");
    pddlDatalogDel(dl);
}

/*
 * Canonical model computed by Gringo (without normalization) is the same as
 * the one computed by pddlDatalogCanonicalModel().
 */
TEST_ONCE(datalog_gringo)
{
#ifdef PDDL_CLINGO
    pddl_err_t err = PDDL_ERR_INIT;

    pddl_datalog_t *dl = pddlDatalogNew();
    graph_t g;
    graphProgram(dl, &g);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assertGraphModel(dl, &g);
    pddlDatalogDel(dl);

    dl = pddlDatalogNew();
    neg_t n;
    negProgram(dl, &n);
    ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assertNegModel(dl, &n);
    pddlDatalogDel(dl);
#endif /* PDDL_CLINGO */
}
