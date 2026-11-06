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

/** Creates a weighted program failing on missing values of fluents */
static pddl_datalog_t *newWeighted(pddl_datalog_weight_type_t type)
{
    return pddlDatalogNewWeighted(type, PDDL_DATALOG_MISSING_FLUENT_ERR);
}

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
    pddlDatalogRuleAddNegBody(dl, rule, &atom);
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
        pddl_num_t w;
        pddlNumSetInt(&w, weight);
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

/** Same as rule(), but with the weight WEIGHT and the annotation
 *  ANN_FN(ANN_DATA) */
static int awrule(pddl_datalog_t *dl,
                  int weight,
                  pddl_datalog_annotation_fn ann_fn,
                  void *ann_data,
                  atom_spec_t head,
                  int body_size, ...)
{
    va_list ap;
    va_start(ap, body_size);
    int id = addRuleV(dl, weight, ann_fn, ann_data, head, body_size, ap);
    va_end(ap);
    return id;
}

/** Same as rule(), but with the weight W given as a number (e.g., with
 *  delta != 0 or a float) */
static int cwrule(pddl_datalog_t *dl,
                  const pddl_num_t *w,
                  atom_spec_t head,
                  int body_size, ...)
{
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, head);
    va_list ap;
    va_start(ap, body_size);
    for (int i = 0; i < body_size; ++i)
        ruleAddBody(dl, &rule, va_arg(ap, atom_spec_t));
    va_end(ap);
    pddlDatalogRuleSetWeight(dl, &rule, w);
    return ruleAdd(dl, &rule);
}

/** Weight atoms of a rule (see frule()) */
struct watoms {
    int size;
    atom_spec_t atom[6];
};
typedef struct watoms watoms_t;

#define WATOMS(...) \
    ((watoms_t){ .size = (int)(sizeof((atom_spec_t[]){ __VA_ARGS__ }) \
                               / sizeof(atom_spec_t)), \
                 .atom = { __VA_ARGS__ } })

static void ruleAddWeightAtom(pddl_datalog_t *dl,
                              pddl_datalog_rule_t *rule,
                              atom_spec_t watom)
{
    pddl_datalog_atom_t atom;
    atomInit(dl, &atom, &watom);
    pddlDatalogRuleAddWeightAtom(dl, rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
}

/** Same as rule(), but with the weight WEIGHT, the weight atoms WATOMS,
 *  and the annotation ANN_FN(ANN_DATA) if ANN_FN is non-NULL */
static int frule(pddl_datalog_t *dl,
                 int weight,
                 watoms_t watoms,
                 pddl_datalog_annotation_fn ann_fn,
                 void *ann_data,
                 atom_spec_t head,
                 int body_size, ...)
{
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, head);
    va_list ap;
    va_start(ap, body_size);
    for (int i = 0; i < body_size; ++i)
        ruleAddBody(dl, &rule, va_arg(ap, atom_spec_t));
    va_end(ap);
    pddl_num_t w;
    pddlNumSetInt(&w, weight);
    pddlDatalogRuleSetWeight(dl, &rule, &w);
    for (int i = 0; i < watoms.size; ++i)
        ruleAddWeightAtom(dl, &rule, watoms.atom[i]);
    if (ann_fn != NULL)
        pddlDatalogRuleAddAnnotation(dl, &rule, ann_fn, ann_data);
    return ruleAdd(dl, &rule);
}

/** Sets the value of the ground numeric fluent ATOM to VALUE and asserts
 *  that it succeeds */
static void fluent(pddl_datalog_t *dl, atom_spec_t atom, int value)
{
    pddl_num_t v;
    pddlNumSetInt(&v, value);
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogAddNumericFluent(dl, atom.pred, atom.arg, &v, &err);
    assert(ret == 0);
    assert(!pddlErrIsSet(&err));
}

/** Asserts that setting the value of the numeric fluent ATOM to VALUE fails
 *  with an error message containing MSG */
static void fluentFail(pddl_datalog_t *dl,
                       atom_spec_t atom,
                       int value,
                       const char *msg)
{
    pddl_num_t v;
    pddlNumSetInt(&v, value);
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogAddNumericFluent(dl, atom.pred, atom.arg, &v, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    assert(strstr(pddlErrMsg(&err), msg) != NULL);
}

/** Adds body-less rule ATOM */
static int fact(pddl_datalog_t *dl, atom_spec_t atom)
{
    return rule(dl, atom, 0);
}

/** Adds the fact ATOM with the weight WEIGHT to the database and asserts
 *  that it succeeds */
static void addFactToDB(pddl_datalog_t *dl,
                        atom_spec_t atom,
                        const pddl_num_t *weight)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogDBAddFact(dl, atom.pred, atom.arg, weight, &err);
    assert(ret == 0);
    assert(!pddlErrIsSet(&err));
}

/** Finalizes the program with TO_NORMAL_FORM and asserts that it
 *  succeeds */
static void finalizeProgram(pddl_datalog_t *dl, pddl_bool_t to_normal_form)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogFinalizeProgram(dl, to_normal_form, &err);
    assert(ret == 0);
    assert(!pddlErrIsSet(&err));
}

/** Finalizes the program and transforms it to the normal form */
static void finalize(pddl_datalog_t *dl)
{
    finalizeProgram(dl, pddl_true);
}

/** Asserts that the status ST returned by a computation of the canonical
 *  model is EXPECTED and that ERR is set only on errors */
static void assertCanonicalModelStatus(
                    pddl_datalog_canonical_model_status_t st,
                    pddl_datalog_canonical_model_status_t expected,
                    const pddl_err_t *err)
{
    switch (st){
    case PDDL_DATALOG_CANONICAL_MODEL_ERR:
        assert(st == expected);
        assert(pddlErrIsSet(err));
        break;
    case PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED:
    case PDDL_DATALOG_CANONICAL_MODEL_OK:
        assert(st == expected);
        assert(!pddlErrIsSet(err));
        break;
    }
}

/** Computes (or continues computing) the canonical model and asserts that
 *  it returns EXPECTED */
static void canonicalModel(pddl_datalog_t *dl,
                           pddl_datalog_canonical_model_status_t expected)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_canonical_model_status_t st;
    st = pddlDatalogCanonicalModel(dl, &err);
    assertCanonicalModelStatus(st, expected, &err);
}

/** Computes (or continues computing) the weighted canonical model up to
 *  the weight LIMIT and asserts that it returns EXPECTED */
static void canonicalModelWithLimit(
                    pddl_datalog_t *dl,
                    const pddl_num_t *limit,
                    pddl_datalog_canonical_model_status_t expected)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_canonical_model_status_t st;
    st = pddlDatalogWeightedCanonicalModelWithLimit(dl, limit, &err);
    assertCanonicalModelStatus(st, expected, &err);
}

/** Same as canonicalModelWithLimit() with an integer limit */
static void canonicalModelWithIntLimit(
                    pddl_datalog_t *dl,
                    int limit,
                    pddl_datalog_canonical_model_status_t expected)
{
    pddl_num_t l;
    pddlNumSetInt(&l, limit);
    canonicalModelWithLimit(dl, &l, expected);
}

/** Computes the canonical model and asserts that it returns OK */
static void canonicalModelOK(pddl_datalog_t *dl)
{
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_OK);
}

/** Commits the database with the input predicates INPUT_PREDS (can be
 *  NULL) and asserts that it succeeds */
static void dbCommit(pddl_datalog_t *dl, const pddl_ivec_t *input_preds)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogDBCommit(dl, input_preds, &err);
    assert(ret == 0);
    assert(!pddlErrIsSet(&err));
}

/** Asserts that the commit with the input predicates INPUT_PREDS fails */
static void dbCommitFail(pddl_datalog_t *dl, const pddl_ivec_t *input_preds)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogDBCommit(dl, input_preds, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
}

/** Asserts that the weight of the fact of the goal predicate GOAL is
 *  EXPECTED */
static void assertGoalWeight(pddl_datalog_t *dl, int goal, int expected)
{
    pddl_num_t weight;
    int ret = pddlDatalogDBGoalWeight(dl, goal, &weight);
    assert(ret == 0);
    pddl_num_t exp;
    pddlNumSetInt(&exp, expected);
    assert(pddlNumExactEq(&weight, &exp));
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
                     const pddl_num_t *weight)
{
    assert(fs->size < MAX_FACTS);
    char *s = fs->fact[fs->size++];
    int len = 0;
    s[len++] = pred;
    for (int i = 0; i < arity; ++i)
        s[len++] = arg[i];
    s[len] = '\x0';
    if (weight != NULL){
        char w[32];
        snprintf(s + len, 32 - len, ":%s", pddlNumFmt(weight, w, 32));
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

static void factCB(int pred,
                   int arity,
                   const int *arg,
                   const pddl_num_t *weight,
                   void *ud)
{
    factsAdd(ud, pred, arity, arg, weight);
}

/** Asserts that the facts of PRED (with their weights in weighted
 *  programs) are EXPECTED */
static void assertFacts(pddl_datalog_t *dl, int pred, const char *expected)
{
    facts_t fs;
    fs.size = 0;
    pddlDatalogDBForEachFact(dl, pred, factCB, &fs);
    char out[STR_SIZE];
    factsJoin(&fs, out);
    assertStrEq(out, expected);
}

/** Number of facts in the database */
static int dbSize(pddl_datalog_t *dl)
{
    int pred, arity, arg[MAX_ARITY];
    int size = 0;
    while (pddlDatalogDBFact(dl, size, &pred, &arity, arg, NULL) == 0)
        ++size;
    return size;
}

/** Asserts that adding the fact ATOM with the weight WEIGHT to the
 *  database fails and changes nothing */
static void addFactToDBFail(pddl_datalog_t *dl,
                            atom_spec_t atom,
                            const pddl_num_t *weight)
{
    int size = dbSize(dl);
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogDBAddFact(dl, atom.pred, atom.arg, weight, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    assert(dbSize(dl) == size);
}

/** Writes all facts in the database sorted and separated by spaces to OUT */
static void dbStr(pddl_datalog_t *dl, char *out)
{
    facts_t fs;
    fs.size = 0;
    int size = dbSize(dl);
    for (int id = 0; id < size; ++id){
        int pred, arity, arg[MAX_ARITY];
        int ret = pddlDatalogDBFact(dl, id, &pred, &arity, arg, NULL);
        assert(ret == 0);
        factsAdd(&fs, pred, arity, arg, NULL);
    }
    factsJoin(&fs, out);
}

/** Asserts that all facts in the database are EXPECTED */
static void assertDB(pddl_datalog_t *dl, const char *expected)
{
    char out[STR_SIZE];
    dbStr(dl, out);
    assertStrEq(out, expected);
}

/** Writes the program printed by pddlDatalogPrint() to OUT */
static void programStr(const pddl_datalog_t *dl, char *out)
{
    FILE *fout = tmpfile();
    assert(fout != NULL);
    pddlDatalogPrint(dl, fout);
    long size = ftell(fout);
    assert(size >= 0 && size < STR_SIZE);
    rewind(fout);
    size_t read = fread(out, 1, size, fout);
    assert(read == (size_t)size);
    out[size] = '\x0';
    fclose(fout);
}

/** Asserts that the program printed by pddlDatalogPrint() is EXPECTED */
static void assertProgram(const pddl_datalog_t *dl, const char *expected)
{
    char buf[STR_SIZE];
    programStr(dl, buf);
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
 *  the body facts are sorted; in weighted programs, the ground weight of
 *  the rule follows the dot, e.g., "label | (H a) :- (B b). w=3\n" */
static void annotation(pddl_datalog_t *dl,
                       int head_fact_id,
                       const pddl_iset_t *body_fact_ids,
                       const pddl_num_t *weight,
                       void *userdata)
{
    ann_data_t *d = userdata;
    ann_out_t *out = d->out;
    int pred, arity, arg[MAX_ARITY];

    int st = pddlDatalogDBFact(dl, head_fact_id, &pred, &arity, arg, NULL);
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
        int st = pddlDatalogDBFact(dl, fact_id, &pred, &arity, arg, NULL);
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
    out->len += snprintf(out->buf + out->len, STR_SIZE - out->len, ".");
    if (weight != NULL){
        char w[32];
        out->len += snprintf(out->buf + out->len, STR_SIZE - out->len,
                             " w=%s", pddlNumFmt(weight, w, 32));
    }
    out->len += snprintf(out->buf + out->len, STR_SIZE - out->len, "\n");
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

    finalize(dl);
    canonicalModelOK(dl);
    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "Q(X, Y) :- P(X, Y).\n"
                  "R(2, 1, 1).\n"
                  "R(X, Y, Z) :- P(X, Y), _p4(Y, Z).\n"
                  "_p4(X, Y) :- Q(X, Z), S(Z, Y).\n");
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

    finalize(dl);
    canonicalModelOK(dl);
    assertProgram(dl,
                  "P(1, 2).\n"
                  "Q(2, 1).\n"
                  "S(1, 2).\n"
                  "Q(X, Y) :- P(X, Y).; ann_size = 1\n"
                  "R(2, 1, 1).; ann_size = 1\n"
                  "R(X, Y, Z) :- P(X, Y), _p5(Y, Z).; ann_size = 1\n"
                  "G() :- R(1, 2, 2).; ann_size = 1\n"
                  "_p5(X, Y) :- Q(X, Z), S(Z, Y).\n");

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
    finalize(dl);
    canonicalModelOK(dl);
    assertGraphModel(dl, &g);
    pddlDatalogDel(dl);
}

/*
 * Facts added to the database after the canonical model are processed by
 * the next call of pddlDatalogCanonicalModel(); continuing without new
 * facts or with already known facts changes nothing.
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
    finalize(dl);

    canonicalModelOK(dl);
    assertFacts(dl, T, "Tab");
    assert(dbSize(dl) == 2);

    canonicalModelOK(dl);
    assertFacts(dl, T, "Tab");
    assert(dbSize(dl) == 2);

    addFactToDB(dl, ATOM(E, b, c), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, E, "Eab Ebc");
    assertFacts(dl, T, "Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    addFactToDB(dl, ATOM(E, a, b), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, T, "Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    addFactToDB(dl, ATOM(E, c, d), NULL);
    addFactToDB(dl, ATOM(E, d, a), NULL);
    canonicalModelOK(dl);
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

    finalizeProgram(dl, pddl_false);
    assert(!pddlDatalogIsNormalForm(dl));
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
    assert(pddlDatalogIsSafe(dl));
    assert(pddlDatalogIsNormalForm(dl));
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
                  "_p8(X, Y) :- A(X, Z), B(Z, Y).\n"
                  "_p9(X, Y) :- C(Z, Y), _p8(X, Z).\n"
                  "_p10(X) :- B(X, Y), D(Y).\n"
                  "_p11(X) :- A(Y, Z), B(Z, X).\n");

    canonicalModelOK(dl);
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
    ret = pddlDatalogFinalizeProgram(dl, pddl_true, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);
    pddlDatalogDel(dl);
}

/*
 * Normalization merges the auxiliary predicates of rules that differ only
 * in the order of their body atoms.
 */
TEST_ONCE(datalog_normal_form_body_order)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int D = addPred(dl, 1, "D");
    int H = addPred(dl, 1, "H");
    int I = addPred(dl, 1, "I");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(B, b, c));
    fact(dl, ATOM(D, c));
    rule(dl, ATOM(H, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    rule(dl, ATOM(I, X), 3, ATOM(D, Z), ATOM(B, Y, Z), ATOM(A, X, Y));

    finalizeProgram(dl, pddl_true);
    assertProgram(dl,
                  "A(a, b).\n"
                  "B(b, c).\n"
                  "D(c).\n"
                  "H(X) :- A(X, Y), _p5(Y).\n"
                  "I(X) :- A(X, Y), _p5(Y).\n"
                  "_p5(X) :- B(X, Y), D(Y).\n");

    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    assertFacts(dl, I, "Ia");
    pddlDatalogDel(dl);
}

/*
 * Normalization merges the auxiliary predicates of rules that differ only
 * in the naming of their variables.
 */
TEST_ONCE(datalog_normal_form_var_naming)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int D = addPred(dl, 1, "D");
    int H = addPred(dl, 1, "H");
    int I = addPred(dl, 1, "I");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(B, b, c));
    fact(dl, ATOM(D, c));
    rule(dl, ATOM(H, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    rule(dl, ATOM(I, W), 3, ATOM(A, W, Z), ATOM(B, Z, Y), ATOM(D, Y));

    finalizeProgram(dl, pddl_true);
    assertProgram(dl,
                  "A(a, b).\n"
                  "B(b, c).\n"
                  "D(c).\n"
                  "H(X) :- A(X, Y), _p5(Y).\n"
                  "I(X) :- A(X, Y), _p5(Y).\n"
                  "_p5(X) :- B(X, Y), D(Y).\n");

    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    assertFacts(dl, I, "Ia");
    pddlDatalogDel(dl);
}

/*
 * Normalization merges the auxiliary predicates of rules whose bodies
 * consist of atoms of the same predicate in different orders and with
 * differently named variables.
 */
TEST_ONCE(datalog_normal_form_same_pred)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = addPred(dl, 2, "E");
    int J = addPred(dl, 2, "J");
    int K = addPred(dl, 2, "K");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(E, a, b));
    fact(dl, ATOM(E, b, c));
    fact(dl, ATOM(E, c, d));
    rule(dl, ATOM(J, X, W), 3, ATOM(E, X, Y), ATOM(E, Y, Z), ATOM(E, Z, W));
    rule(dl, ATOM(K, X, W), 3, ATOM(E, Z, W), ATOM(E, Y, Z), ATOM(E, X, Y));

    finalizeProgram(dl, pddl_true);
    assertProgram(dl,
                  "E(a, b).\n"
                  "E(b, c).\n"
                  "E(c, d).\n"
                  "J(X, Y) :- E(Z, Y), _p3(X, Z).\n"
                  "K(X, Y) :- E(X, Z), _p3(Z, Y).\n"
                  "_p3(X, Y) :- E(X, Z), E(Z, Y).\n");

    canonicalModelOK(dl);
    assertFacts(dl, J, "Jad");
    assertFacts(dl, K, "Kad");
    pddlDatalogDel(dl);
}

/*
 * Normalization merges auxiliary predicates with equal definitions even
 * if they are interleaved with an auxiliary predicate with the same body
 * and a head of a different arity.
 */
TEST_ONCE(datalog_normal_form_interleaved_arity)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int D = addPred(dl, 1, "D");
    int H = addPred(dl, 1, "H");
    int I = addPred(dl, 2, "I");
    int J = addPred(dl, 1, "J");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(B, b, c));
    fact(dl, ATOM(D, a));
    // Each rule is binarized to aux :- A(X, Y), B(Y, Z) with the head (X)
    // for H and J, and (X, Y) for I
    rule(dl, ATOM(H, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, X));
    rule(dl, ATOM(I, X, Y), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, X));
    rule(dl, ATOM(J, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, X));

    finalizeProgram(dl, pddl_true);
    assertProgram(dl,
                  "A(a, b).\n"
                  "B(b, c).\n"
                  "D(a).\n"
                  "H(X) :- D(X), _p6(X).\n"
                  "I(X, Y) :- D(X), _p7(X, Y).\n"
                  "J(X) :- D(X), _p6(X).\n"
                  "_p6(X) :- A(X, Y), B(Y, Z).\n"
                  "_p7(X, Y) :- A(X, Y), B(Y, Z).\n");

    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    assertFacts(dl, I, "Iab");
    assertFacts(dl, J, "Ja");
    pddlDatalogDel(dl);
}

/*
 * A second normalization of a normalized program does not change it, i.e.,
 * canonization of the rules is idempotent.
 */
TEST_ONCE(datalog_normal_form_twice)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int D = addPred(dl, 1, "D");
    int E = addPred(dl, 2, "E");
    int H = addPred(dl, 1, "H");
    int I = addPred(dl, 1, "I");
    int J = addPred(dl, 2, "J");
    int K = addPred(dl, 2, "K");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");

    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(B, b, c));
    fact(dl, ATOM(D, c));
    fact(dl, ATOM(E, a, b));
    fact(dl, ATOM(E, b, a));
    rule(dl, ATOM(H, X), 3, ATOM(A, X, Y), ATOM(B, Y, Z), ATOM(D, Z));
    rule(dl, ATOM(I, W), 3, ATOM(D, Y), ATOM(B, Z, Y), ATOM(A, W, Z));
    rule(dl, ATOM(J, X, W), 4,
         ATOM(E, X, Y), ATOM(E, Y, Z), ATOM(E, Z, W), ATOM(D, Z));
    rule(dl, ATOM(K, X, Y), 2, ATOM(E, Y, X), ATOM(E, X, Y));

    finalizeProgram(dl, pddl_true);
    char before[STR_SIZE];
    programStr(dl, before);

    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
    assert(pddlDatalogIsNormalForm(dl));
    char after[STR_SIZE];
    programStr(dl, after);
    assertStrEq(after, before);

    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    assertFacts(dl, I, "Ia");
    assertFacts(dl, J, "");
    assertFacts(dl, K, "Kab Kba");
    pddlDatalogDel(dl);
}

/*
 * The normal form required by the canonical model (safe rules with at most
 * two positive body atoms) is recognized also without the normalization,
 * and the canonical model of a program that is not in the normal form is
 * refused without touching the database.
 */
TEST_ONCE(datalog_is_normal_form)
{
    pddl_err_t err = PDDL_ERR_INIT;

    // Safe rules with at most two body atoms
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 1, "B");
    int H = addPred(dl, 1, "H");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(A, b, c));
    fact(dl, ATOM(B, c));
    rule(dl, ATOM(H, X), 2, ATOM(A, X, Y), ATOM(B, Y));
    finalizeProgram(dl, pddl_false);
    assert(pddlDatalogIsNormalForm(dl));
    canonicalModelOK(dl);
    assertFacts(dl, H, "Hb");
    pddlDatalogDel(dl);

    // A rule with three body atoms is refused until the normalization
    dl = pddlDatalogNew();
    A = addPred(dl, 2, "A");
    B = addPred(dl, 1, "B");
    H = addPred(dl, 1, "H");
    a = addConst(dl, "a");
    b = addConst(dl, "b");
    c = addConst(dl, "c");
    X = pddlDatalogAddVar(dl, "X");
    Y = pddlDatalogAddVar(dl, "Y");
    Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(A, b, c));
    fact(dl, ATOM(B, c));
    rule(dl, ATOM(H, X), 3, ATOM(A, X, Y), ATOM(A, Y, Z), ATOM(B, Z));
    finalizeProgram(dl, pddl_false);
    assert(!pddlDatalogIsNormalForm(dl));
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
    assert(pddlDatalogIsNormalForm(dl));
    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    pddlDatalogDel(dl);

    // A variable of the head is not in the body
    dl = pddlDatalogNew();
    A = addPred(dl, 1, "A");
    H = addPred(dl, 2, "H");
    a = addConst(dl, "a");
    X = pddlDatalogAddVar(dl, "X");
    Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(A, a));
    rule(dl, ATOM(H, X, Y), 1, ATOM(A, X));
    finalizeProgram(dl, pddl_false);
    assert(!pddlDatalogIsSafe(dl));
    assert(!pddlDatalogIsNormalForm(dl));
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    pddlDatalogDel(dl);

    // A body-less rule with a variable
    dl = pddlDatalogNew();
    A = addPred(dl, 1, "A");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(A, X));
    finalizeProgram(dl, pddl_false);
    assert(!pddlDatalogIsNormalForm(dl));
    ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);
    assert(!pddlDatalogIsNormalForm(dl));
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    pddlDatalogDel(dl);

    // A variable of a negative atom is not in the positive body
    dl = pddlDatalogNew();
    A = addPred(dl, 1, "A");
    B = addPred(dl, 1, "B");
    H = addPred(dl, 1, "H");
    a = addConst(dl, "a");
    X = pddlDatalogAddVar(dl, "X");
    Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(A, a));
    pddl_datalog_rule_t r;
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(A, X));
    ruleAddNeg(dl, &r, ATOM(B, Y));
    ruleAdd(dl, &r);
    assert(!pddlDatalogIsSafe(dl));
    ret = pddlDatalogFinalizeProgram(dl, pddl_true, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);
    assert(!pddlDatalogIsNormalForm(dl));
    pddlDatalogDel(dl);
}

/** Predicates of the program created by weightedProgram() */
struct weighted {
    int At, L, K, P, Q, G;
    int a, b, c, d;
};
typedef struct weighted weighted_t;

/** Creates a weighted program: At(a) (unless AT_A_IN_DB is true, then it
 *  must be added to the database), links a -> b -> c, a -> c, moving along
 *  a link costs 1, a key at a reached node costs 2, P costs 10 more than
 *  K(c), Q follows from P for free, and the goal requires At(b), At(c), and
 *  K(c) (or At(d) if REACHABLE_GOAL is false) */
static void weightedProgram(pddl_datalog_t *dl,
                            weighted_t *w,
                            pddl_bool_t at_a_in_db,
                            pddl_bool_t reachable_goal)
{
    w->At = addPred(dl, 1, "A");
    w->L = addPred(dl, 2, "L");
    w->K = addPred(dl, 1, "K");
    w->P = addPred(dl, 0, "P");
    w->Q = addPred(dl, 0, "Q");
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
    wrule(dl, 10, ATOM0(w->P), 1, ATOM(w->K, w->c));
    rule(dl, ATOM0(w->Q), 1, ATOM0(w->P));
    if (reachable_goal){
        rule(dl, ATOM0(w->G), 3,
             ATOM(w->At, w->b), ATOM(w->At, w->c), ATOM(w->K, w->c));
    }else{
        rule(dl, ATOM0(w->G), 1, ATOM(w->At, w->d));
    }
    finalize(dl);
}

/** Weighted canonical model of the program from weightedProgram() with
 *  the aggregation TYPE, GOAL is the expected weight of the goal */
static void weightedTest(pddl_datalog_weight_type_t type, int goal)
{
    weighted_t w;
    pddl_num_t weight;

    pddl_datalog_t *dl = newWeighted(type);
    weightedProgram(dl, &w, pddl_false, pddl_true);
    int ret = pddlDatalogDBGoalWeight(dl, w.G, &weight);
    assert(ret == -1);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, goal);
    // The computation stopped at the goal: P is queued, but Q (derived
    // from P) is not
    assertFacts(dl, w.P, "P:13");
    assertFacts(dl, w.Q, "");
    // The goal was already reached, so nothing more is computed
    int size = dbSize(dl);
    canonicalModelOK(dl);
    assert(dbSize(dl) == size);
    assertFacts(dl, w.Q, "");
    assertGoalWeight(dl, w.G, goal);
    pddlDatalogDel(dl);

    // At(a) added to the database with zero weight, given explicitly or
    // as NULL, with or without resetting the database first
    for (int i = 0; i < 2; ++i){
        dl = newWeighted(type);
        weightedProgram(dl, &w, pddl_true, pddl_true);
        if (i == 0){
            pddlDatalogDBReset(dl);
            addFactToDB(dl, ATOM(w.At, w.a), &pddl_num_zero);
        }else{
            addFactToDB(dl, ATOM(w.At, w.a), NULL);
        }
        canonicalModelOK(dl);
        assertGoalWeight(dl, w.G, goal);
        pddlDatalogDel(dl);
    }

    // Unreachable goal: all reachable facts get their final weights
    dl = newWeighted(type);
    weightedProgram(dl, &w, pddl_false, pddl_false);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    ret = pddlDatalogDBGoalWeight(dl, w.G, &weight);
    assert(ret == -1);
    assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1");
    assertFacts(dl, w.L, "Lab:0 Lac:0 Lbc:0");
    assertFacts(dl, w.K, "Ka:2 Kb:3 Kc:3");
    assertFacts(dl, w.P, "P:13");
    assertFacts(dl, w.Q, "Q:13");
    assertFacts(dl, w.G, "");
    pddlDatalogDel(dl);
}

/*
 * Weighted canonical model with the sum of weights: the goal weight is the
 * h^add value, the computation stops at the goal and does not continue
 * when called again; an unreachable goal is reported and all reachable
 * facts get their final weights.
 */
TEST_ONCE(datalog_weighted_add)
{
    weightedTest(PDDL_DATALOG_WEIGHT_ADD, 5);
}

/*
 * Weighted canonical model with the maximum of weights: the goal weight is
 * the h^max value, the computation stops at the goal and does not continue
 * when called again; an unreachable goal is reported and all reachable
 * facts get their final weights.
 */
TEST_ONCE(datalog_weighted_max)
{
    weightedTest(PDDL_DATALOG_WEIGHT_MAX, 3);
}

/** The pattern of src/lifted_heur_relaxed.c on the program from
 *  weightedProgram() with the aggregation TYPE: GOAL_A and GOAL_B are the
 *  expected weights of the goal when starting at a and b, respectively */
static void weightedResetTest(pddl_datalog_weight_type_t type,
                              int goal_a,
                              int goal_b)
{
    weighted_t w;
    pddl_datalog_t *dl = newWeighted(type);
    weightedProgram(dl, &w, pddl_true, pddl_true);
    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBReset(dl);
        addFactToDB(dl, ATOM(w.At, w.a), NULL);
        canonicalModelOK(dl);
        assertGoalWeight(dl, w.G, goal_a);

        pddlDatalogDBReset(dl);
        addFactToDB(dl, ATOM(w.At, w.b), NULL);
        canonicalModelOK(dl);
        assertGoalWeight(dl, w.G, goal_b);

        pddlDatalogDBReset(dl);
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    }

    // Rollback without a computation since the commit: At(a) is queued and
    // then dropped, At(b) re-uses its fact ID and weight record
    pddlDatalogDBReset(dl);
    dbCommit(dl, NULL);
    addFactToDB(dl, ATOM(w.At, w.a), NULL);
    pddlDatalogDBRollback(dl);
    assert(dbSize(dl) == 0);
    addFactToDB(dl, ATOM(w.At, w.b), NULL);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, goal_b);
    assertFacts(dl, w.At, "Ab:0 Ac:1");

    // Rollback to a commit with a queued fact: At(a) waits in the queue at
    // the commit, so it is processed after the rollback
    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBReset(dl);
        addFactToDB(dl, ATOM(w.At, w.a), NULL);
        dbCommit(dl, NULL);
        addFactToDB(dl, ATOM(w.At, w.b), NULL);
        pddlDatalogDBRollback(dl);
        canonicalModelOK(dl);
        assertGoalWeight(dl, w.G, goal_a);
        assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1");
    }
    pddlDatalogDel(dl);
}

/*
 * Weighted canonical model with the sum of weights computed repeatedly from
 * different initial states after pddlDatalogDBReset() (as in the lifted
 * relaxed heuristics), and a rollback dropping the queued facts added
 * after the commit.
 */
TEST_ONCE(datalog_weighted_reset_add)
{
    weightedResetTest(PDDL_DATALOG_WEIGHT_ADD, 5, 4);
}

/*
 * Weighted canonical model with the maximum of weights computed repeatedly
 * from different initial states after pddlDatalogDBReset() (as in the
 * lifted relaxed heuristics), and a rollback dropping the queued facts
 * added after the commit.
 */
TEST_ONCE(datalog_weighted_reset_max)
{
    weightedResetTest(PDDL_DATALOG_WEIGHT_MAX, 3, 3);
}

/** Weighted body-less rules with the aggregation TYPE: GOAL is the expected
 *  weight of the goal */
static void weightedInitialFactsTest(pddl_datalog_weight_type_t type,
                                     int goal)
{
    pddl_datalog_t *dl = newWeighted(type);
    int A = addPred(dl, 1, "A");
    int B = addPred(dl, 1, "B");
    int G = addGoalPred(dl, "G");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");

    // A(a) is an initial fact with the weight 5, but it is also derived
    // from B(b) with the weight 2
    wrule(dl, 5, ATOM(A, a), 0);
    fact(dl, ATOM(B, b));
    // B(a) has two body-less rules, the lower weight wins
    wrule(dl, 4, ATOM(B, a), 0);
    wrule(dl, 2, ATOM(B, a), 0);
    wrule(dl, 2, ATOM(A, a), 1, ATOM(B, b));
    wrule(dl, 1, ATOM(A, X), 1, ATOM(B, X));
    rule(dl, ATOM0(G), 2, ATOM(A, a), ATOM(A, b));
    finalize(dl);

    canonicalModelOK(dl);
    assertGoalWeight(dl, G, goal);
    assertFacts(dl, A, "Aa:2 Ab:1");
    assertFacts(dl, B, "Ba:2 Bb:0");
    pddlDatalogDel(dl);
}

/*
 * Weighted body-less rules with the sum of weights: an initial fact gets
 * the weight of its rule (the lowest one if it has more rules), and the
 * weight decreases if the fact is derived more cheaply.
 */
TEST_ONCE(datalog_weighted_initial_facts_add)
{
    weightedInitialFactsTest(PDDL_DATALOG_WEIGHT_ADD, 3);
}

/*
 * Weighted body-less rules with the maximum of weights: an initial fact
 * gets the weight of its rule (the lowest one if it has more rules), and
 * the weight decreases if the fact is derived more cheaply.
 */
TEST_ONCE(datalog_weighted_initial_facts_max)
{
    weightedInitialFactsTest(PDDL_DATALOG_WEIGHT_MAX, 2);
}

/** Weights with delta != 0 (zero-cost actions weighted by eps) with the
 *  aggregation TYPE: T is the expected representation of the fact T */
static void weightedDeltaTest(pddl_datalog_weight_type_t type, const char *t)
{
    pddl_num_t z = pddl_num_eps;
    pddl_num_t one;
    pddlNumSetInt(&one, 1);

    pddl_datalog_t *dl = newWeighted(type);
    int S = addPred(dl, 0, "S");
    int P = addPred(dl, 0, "P");
    int Q = addPred(dl, 0, "Q");
    int R = addPred(dl, 0, "R");
    int G = addPred(dl, 0, "G");
    int T = addPred(dl, 0, "T");
    fact(dl, ATOM0(S));
    cwrule(dl, &z, ATOM0(P), 1, ATOM0(S));
    cwrule(dl, &z, ATOM0(Q), 1, ATOM0(P));
    cwrule(dl, &one, ATOM0(R), 1, ATOM0(S));
    // Three zero-cost actions are cheaper than one action with cost 1
    cwrule(dl, &z, ATOM0(G), 1, ATOM0(Q));
    rule(dl, ATOM0(G), 1, ATOM0(R));
    // Both zero-cost body facts are aggregated
    cwrule(dl, &z, ATOM0(T), 2, ATOM0(Q), ATOM0(P));
    finalize(dl);

    // No goal predicate, so the whole model is computed
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertFacts(dl, S, "S:0");
    assertFacts(dl, P, "P:0+1e");
    assertFacts(dl, Q, "Q:0+2e");
    assertFacts(dl, R, "R:1");
    assertFacts(dl, G, "G:0+3e");
    assertFacts(dl, T, t);
    pddlDatalogDel(dl);
}

/*
 * Weights with delta != 0 and the sum of weights: the deltas are summed,
 * and they are compared only after the values.
 */
TEST_ONCE(datalog_weighted_delta_add)
{
    weightedDeltaTest(PDDL_DATALOG_WEIGHT_ADD, "T:0+4e");
}

/*
 * Weights with delta != 0 and the maximum of weights: the maximum takes
 * the deltas into account, and they are compared only after the values.
 */
TEST_ONCE(datalog_weighted_delta_max)
{
    weightedDeltaTest(PDDL_DATALOG_WEIGHT_MAX, "T:0+3e");
}

/** Float weights and integer weights above the array of the queue with the
 *  aggregation TYPE: D is the expected representation of the fact D */
static void weightedFloatTest(pddl_datalog_weight_type_t type, const char *d)
{
    pddl_num_t half, quarter;
    pddlNumSetFlt(&half, .5f);
    pddlNumSetFlt(&quarter, .25f);

    pddl_datalog_t *dl = newWeighted(type);
    int S = addPred(dl, 0, "S");
    int P = addPred(dl, 0, "P");
    int Q = addPred(dl, 0, "Q");
    int R = addPred(dl, 0, "R");
    int G = addPred(dl, 0, "G");
    int B = addPred(dl, 0, "B");
    int C = addPred(dl, 0, "C");
    int D = addPred(dl, 0, "D");
    fact(dl, ATOM0(S));
    cwrule(dl, &half, ATOM0(P), 1, ATOM0(S));
    cwrule(dl, &quarter, ATOM0(Q), 1, ATOM0(P));
    wrule(dl, 1, ATOM0(R), 1, ATOM0(S));
    // 0.5 + 0.25 is cheaper than 1
    rule(dl, ATOM0(G), 1, ATOM0(Q));
    rule(dl, ATOM0(G), 1, ATOM0(R));
    wrule(dl, 5000, ATOM0(B), 1, ATOM0(S));
    wrule(dl, 2000, ATOM0(C), 1, ATOM0(B));
    rule(dl, ATOM0(D), 2, ATOM0(C), ATOM0(Q));
    finalize(dl);

    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertFacts(dl, S, "S:0");
    assertFacts(dl, P, "P:0.5");
    assertFacts(dl, Q, "Q:0.75");
    assertFacts(dl, R, "R:1");
    assertFacts(dl, G, "G:0.75");
    assertFacts(dl, B, "B:5000");
    assertFacts(dl, C, "C:7000");
    assertFacts(dl, D, d);
    pddlDatalogDel(dl);
}

/*
 * Float weights and integer weights above PDDL_IPQ_ARRAY_SIZE with the
 * sum of weights: a float is summed with an integer into a float.
 */
TEST_ONCE(datalog_weighted_float_add)
{
    weightedFloatTest(PDDL_DATALOG_WEIGHT_ADD, "D:7000.75");
}

/*
 * Float weights and integer weights above PDDL_IPQ_ARRAY_SIZE with the
 * maximum of weights.
 */
TEST_ONCE(datalog_weighted_float_max)
{
    weightedFloatTest(PDDL_DATALOG_WEIGHT_MAX, "D:7000");
}

/** An overflow of the weight with the aggregation TYPE is an error; the
 *  database can be rolled back and reset afterwards */
static void weightedOverflowTest(pddl_datalog_weight_type_t type)
{
    pddl_num_t big;
    pddlNumSetInt(&big, INT_MAX - 1);

    pddl_datalog_t *dl = newWeighted(type);
    int A = addPred(dl, 1, "A");
    int P = addPred(dl, 1, "P");
    int Q = addPred(dl, 1, "Q");
    int a = addConst(dl, "a");
    int X = pddlDatalogAddVar(dl, "X");
    cwrule(dl, &big, ATOM(P, X), 1, ATOM(A, X));
    // The weight of P(a) plus 2 (aggregated with A(a) of weight 0)
    wrule(dl, 2, ATOM(Q, X), 2, ATOM(P, X), ATOM(A, X));
    finalize(dl);

    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBReset(dl);
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        dbCommit(dl, NULL);
        addFactToDB(dl, ATOM(A, a), NULL);
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
        assertFacts(dl, Q, "");
        // The rollback drops A(a) and P(a), the database is usable again
        pddlDatalogDBRollback(dl);
        assertFacts(dl, A, "");
        assertFacts(dl, P, "");
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    }
    pddlDatalogDel(dl);
}

/*
 * An overflow of the sum of weights is an error.
 */
TEST_ONCE(datalog_weighted_overflow_add)
{
    weightedOverflowTest(PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * An overflow of the maximum of weights plus the weight of the rule is an
 * error.
 */
TEST_ONCE(datalog_weighted_overflow_max)
{
    weightedOverflowTest(PDDL_DATALOG_WEIGHT_MAX);
}

/** Predicates of the program created by limitProgram() */
struct limit_prog {
    int S, A, B, G;
};
typedef struct limit_prog limit_prog_t;

/** Creates and finalizes the chain S, A :- S (w=1), B :- A (w=2), and the
 *  goal G :- B (w=3), i.e., the weights are S:0, A:1, B:3, G:6 with both
 *  aggregations */
static void limitProgram(pddl_datalog_t *dl, limit_prog_t *p)
{
    p->S = addPred(dl, 0, "S");
    p->A = addPred(dl, 0, "A");
    p->B = addPred(dl, 0, "B");
    p->G = addGoalPred(dl, "G");
    fact(dl, ATOM0(p->S));
    wrule(dl, 1, ATOM0(p->A), 1, ATOM0(p->S));
    wrule(dl, 2, ATOM0(p->B), 1, ATOM0(p->A));
    wrule(dl, 3, ATOM0(p->G), 1, ATOM0(p->B));
    finalize(dl);
}

static void weightedWithLimitTest(pddl_datalog_weight_type_t type)
{
    const pddl_datalog_canonical_model_status_t unreached
        = PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED;
    pddl_datalog_t *dl = newWeighted(type);
    limit_prog_t p;
    limitProgram(dl, &p);

    // A is derived by S, but it is not popped
    canonicalModelWithIntLimit(dl, 0, unreached);
    assertFacts(dl, p.S, "S:0");
    assertFacts(dl, p.A, "A:1");
    assertFacts(dl, p.B, "");

    pddl_num_t limit;
    pddlNumSetFlt(&limit, 2.5f);
    canonicalModelWithLimit(dl, &limit, unreached);
    assertFacts(dl, p.B, "B:3");
    assertFacts(dl, p.G, "");

    // The facts with the weights equal to the limit are processed
    canonicalModelWithIntLimit(dl, 3, unreached);
    assertFacts(dl, p.G, "G:6");

    canonicalModelWithIntLimit(dl, 6, PDDL_DATALOG_CANONICAL_MODEL_OK);
    assertGoalWeight(dl, p.G, 6);

    // The goal was already reached
    canonicalModelWithIntLimit(dl, 0, PDDL_DATALOG_CANONICAL_MODEL_OK);
    assertGoalWeight(dl, p.G, 6);

    // Continuing without a limit
    pddlDatalogDBReset(dl);
    canonicalModelWithIntLimit(dl, 1, unreached);
    assertFacts(dl, p.B, "B:3");
    canonicalModelOK(dl);
    assertGoalWeight(dl, p.G, 6);

    // Rollback to a commit made before the limited computation
    pddlDatalogDBReset(dl);
    dbCommit(dl, NULL);
    canonicalModelWithIntLimit(dl, 1, unreached);
    pddlDatalogDBRollback(dl);
    assert(dbSize(dl) == 0);
    canonicalModelOK(dl);
    assertGoalWeight(dl, p.G, 6);
    pddlDatalogDel(dl);

    // Unreachable goal: all facts are derived
    dl = newWeighted(type);
    int S = addPred(dl, 0, "S");
    int A = addPred(dl, 0, "A");
    int B = addPred(dl, 0, "B");
    int G = addGoalPred(dl, "G");
    fact(dl, ATOM0(S));
    wrule(dl, 1, ATOM0(A), 1, ATOM0(S));
    wrule(dl, 1, ATOM0(G), 1, ATOM0(B));
    finalize(dl);
    canonicalModelWithIntLimit(dl, 100, unreached);
    assertDB(dl, "A S");
    pddl_num_t w;
    int ret = pddlDatalogDBGoalWeight(dl, G, &w);
    assert(ret == -1);
    pddlDatalogDel(dl);

    // The program must be in the normal form
    dl = newWeighted(type);
    S = addPred(dl, 0, "S");
    A = addPred(dl, 0, "A");
    G = addGoalPred(dl, "G");
    fact(dl, ATOM0(S));
    fact(dl, ATOM0(A));
    rule(dl, ATOM0(G), 3, ATOM0(S), ATOM0(A), ATOM0(S));
    finalizeProgram(dl, pddl_false);
    canonicalModelWithIntLimit(dl, 100, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    pddlDatalogDel(dl);
}

/*
 * The weighted canonical model computed up to a limit stops before the
 * first fact with a greater weight is popped (sum of the weights); the
 * following calls continue the computation.
 */
TEST_ONCE(datalog_weighted_with_limit_add)
{
    weightedWithLimitTest(PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * The weighted canonical model computed up to a limit stops before the
 * first fact with a greater weight is popped (maximum of the weights); the
 * following calls continue the computation.
 */
TEST_ONCE(datalog_weighted_with_limit_max)
{
    weightedWithLimitTest(PDDL_DATALOG_WEIGHT_MAX);
}

/** Predicates of the program created by weightedCommitProgram() */
struct weighted_commit {
    int At, L, G;
    int a, b, c, d;
};
typedef struct weighted_commit weighted_commit_t;

/** Creates and finalizes a program with the links a -> b, a -> c, b -> d,
 *  c -> d (two equally expensive paths to d), moving along a link costs 1
 *  and it is annotated with ANN_MOVE, and the goal G :- At(d) is annotated
 *  with ANN_GOAL */
static void weightedCommitProgram(pddl_datalog_t *dl,
                                  weighted_commit_t *w,
                                  ann_data_t *ann_move,
                                  ann_data_t *ann_goal)
{
    w->At = addPred(dl, 1, "A");
    w->L = addPred(dl, 2, "L");
    w->G = addGoalPred(dl, "G");
    w->a = addConst(dl, "a");
    w->b = addConst(dl, "b");
    w->c = addConst(dl, "c");
    w->d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(w->L, w->a, w->b));
    fact(dl, ATOM(w->L, w->a, w->c));
    fact(dl, ATOM(w->L, w->b, w->d));
    fact(dl, ATOM(w->L, w->c, w->d));
    awrule(dl, 1, annotation, ann_move, ATOM(w->At, Y), 2,
           ATOM(w->At, X), ATOM(w->L, X, Y));
    arule(dl, annotation, ann_goal, ATOM0(w->G), 1, ATOM(w->At, w->d));
    finalize(dl);
}

/** Writes to OUT the facts of At with their weights followed by the output
 *  of the annotations executed from the goal (written to ANN_OUT) */
static void weightedCommitResult(pddl_datalog_t *dl,
                                 const weighted_commit_t *w,
                                 ann_out_t *ann_out,
                                 char *out)
{
    facts_t fs;
    fs.size = 0;
    pddlDatalogDBForEachFact(dl, w->At, factCB, &fs);
    factsJoin(&fs, out);
    ann_out->len = 0;
    ann_out->buf[0] = '\x0';
    pddlDatalogExecuteAnnotations(dl, w->G);
    int len = strlen(out);
    snprintf(out + len, STR_SIZE - len, "\n%s", ann_out->buf);
}

static void weightedCommitTest(pddl_datalog_weight_type_t type)
{
    ann_out_t ann_out;
    ann_out.len = 0;
    ann_data_t ann_move = { "move", &ann_out };
    ann_data_t ann_goal = { "goal", &ann_out };
    pddl_datalog_t *dl = newWeighted(type);
    weighted_commit_t w;
    weightedCommitProgram(dl, &w, &ann_move, &ann_goal);
    char ref[STR_SIZE], res[STR_SIZE];
    pddl_num_t one;
    pddlNumSetInt(&one, 1);

    // The reference computation from At(a)
    addFactToDB(dl, ATOM(w.At, w.a), NULL);
    dbCommit(dl, NULL);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 2);
    weightedCommitResult(dl, &w, &ann_out, ref);

    // A different computation after a rollback
    pddlDatalogDBRollback(dl);
    addFactToDB(dl, ATOM(w.At, w.b), NULL);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 1);
    weightedCommitResult(dl, &w, &ann_out, res);
    assert(strcmp(res, ref) != 0);

    // After another rollback, the same computation as the reference
    pddlDatalogDBRollback(dl);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 2);
    weightedCommitResult(dl, &w, &ann_out, res);
    assertStrEq(res, ref);

    // A commit in the middle of the computation: the facts with the
    // weights up to 1 are processed and At(d) waits in the queue
    pddlDatalogDBReset(dl);
    addFactToDB(dl, ATOM(w.At, w.a), NULL);
    canonicalModelWithIntLimit(dl, 1,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommit(dl, NULL);
    assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1 Ad:2");
    canonicalModelOK(dl);
    weightedCommitResult(dl, &w, &ann_out, res);
    assertStrEq(res, ref);

    // The processed facts, the weight of At(d), and its derivation change
    // after the rollback and they are restored by the next rollback
    pddlDatalogDBRollback(dl);
    assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1 Ad:2");
    addFactToDB(dl, ATOM(w.At, w.d), &one);
    assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1 Ad:1");
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 1);
    pddlDatalogDBRollback(dl);
    assertFacts(dl, w.At, "Aa:0 Ab:1 Ac:1 Ad:2");
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 2);
    weightedCommitResult(dl, &w, &ann_out, res);
    assertStrEq(res, ref);

    // A rollback to a commit made before the initial facts were inserted
    // and after a computation not reaching the goal
    pddlDatalogDBReset(dl);
    dbCommit(dl, NULL);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertDB(dl, "Lab Lac Lbd Lcd");
    pddlDatalogDBRollback(dl);
    assert(dbSize(dl) == 0);
    addFactToDB(dl, ATOM(w.At, w.c), NULL);
    canonicalModelOK(dl);
    assertGoalWeight(dl, w.G, 1);
    pddlDatalogDel(dl);
}

/*
 * Commit and rollback of a weighted database (sum of the weights) restore
 * exactly the state at the commit, including the weights, the states, and
 * the derivations of the facts and the order of the queue, also when the
 * commit is made in the middle of the computation.
 */
TEST_ONCE(datalog_weighted_commit_add)
{
    weightedCommitTest(PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * Commit and rollback of a weighted database (maximum of the weights)
 * restore exactly the state at the commit, including the weights, the
 * states, and the derivations of the facts and the order of the queue,
 * also when the commit is made in the middle of the computation.
 */
TEST_ONCE(datalog_weighted_commit_max)
{
    weightedCommitTest(PDDL_DATALOG_WEIGHT_MAX);
}

/** Predicates and constants of the program created by
 *  weightedFrozenProgram() */
struct weighted_frozen {
    int E, T, At, G;
    int c[4];
};
typedef struct weighted_frozen weighted_frozen_t;

/** Creates and finalizes a program with the static links E: 0 -> 1 -> 2 ->
 *  3 and 0 -> 2, their transitive closure T (weight T_WEIGHT), the moves
 *  At(Y) :- At(X), T(X, Y) (weight 1), and the goal G :- At(3) */
static void weightedFrozenProgram(pddl_datalog_t *dl,
                                  weighted_frozen_t *w,
                                  int t_weight)
{
    w->E = addPred(dl, 2, "E");
    w->T = addPred(dl, 2, "T");
    w->At = addPred(dl, 1, "A");
    w->G = addGoalPred(dl, "G");
    for (int i = 0; i < 4; ++i){
        char name[2] = { '0' + i, '\x0' };
        w->c[i] = addConst(dl, name);
    }
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(w->E, w->c[0], w->c[1]));
    fact(dl, ATOM(w->E, w->c[1], w->c[2]));
    fact(dl, ATOM(w->E, w->c[2], w->c[3]));
    fact(dl, ATOM(w->E, w->c[0], w->c[2]));
    wrule(dl, t_weight, ATOM(w->T, X, Y), 1, ATOM(w->E, X, Y));
    wrule(dl, t_weight, ATOM(w->T, X, Z), 2,
          ATOM(w->T, X, Y), ATOM(w->E, Y, Z));
    wrule(dl, 1, ATOM(w->At, Y), 2, ATOM(w->At, X), ATOM(w->T, X, Y));
    rule(dl, ATOM0(w->G), 1, ATOM(w->At, w->c[3]));
    finalize(dl);
}

/** Writes the facts of At with their weights and the status and the weight
 *  of the goal (computed from At(START)) to OUT */
static void weightedFrozenResult(pddl_datalog_t *dl,
                                 const weighted_frozen_t *w,
                                 pddl_datalog_canonical_model_status_t st,
                                 char *out)
{
    facts_t fs;
    fs.size = 0;
    pddlDatalogDBForEachFact(dl, w->At, factCB, &fs);
    factsJoin(&fs, out);
    pddl_num_t g;
    int len = strlen(out);
    if (st == PDDL_DATALOG_CANONICAL_MODEL_OK
            && pddlDatalogDBGoalWeight(dl, w->G, &g) == 0){
        char buf[32];
        snprintf(out + len, STR_SIZE - len, " | G:%s",
                 pddlNumFmt(&g, buf, 32));
    }else{
        snprintf(out + len, STR_SIZE - len, " | unreached");
    }
}

/** Computes the canonical model of a fresh (not frozen) program from
 *  At(START) and writes the result to OUT */
static void weightedFrozenReference(pddl_datalog_weight_type_t type,
                                    int start,
                                    char *out)
{
    pddl_datalog_t *dl = newWeighted(type);
    weighted_frozen_t w;
    weightedFrozenProgram(dl, &w, 0);
    addFactToDB(dl, ATOM(w.At, w.c[start]), NULL);
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_canonical_model_status_t st;
    st = pddlDatalogCanonicalModel(dl, &err);
    assert(st != PDDL_DATALOG_CANONICAL_MODEL_ERR);
    weightedFrozenResult(dl, &w, st, out);
    pddlDatalogDel(dl);
}

static void weightedFrozenTest(pddl_datalog_weight_type_t type)
{
    // The static part (E, T) is processed with the limit zero and frozen
    pddl_datalog_t *dl = newWeighted(type);
    weighted_frozen_t w;
    weightedFrozenProgram(dl, &w, 0);
    canonicalModelWithIntLimit(dl, 0,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    PDDL_IVEC(input);
    pddlIVecPush(&input, w.At);
    dbCommit(dl, &input);
    addFactToDBFail(dl, ATOM(w.E, w.c[3], w.c[0]), NULL);

    // Every evaluation from the commit gives the same results as a fresh
    // program
    for (int rep = 0; rep < 2; ++rep){
        for (int start = 0; start < 4; ++start){
            pddlDatalogDBRollback(dl);
            addFactToDB(dl, ATOM(w.At, w.c[start]), NULL);
            pddl_err_t err = PDDL_ERR_INIT;
            pddl_datalog_canonical_model_status_t st;
            st = pddlDatalogCanonicalModel(dl, &err);
            assert(st != PDDL_DATALOG_CANONICAL_MODEL_ERR);
            char res[STR_SIZE], ref[STR_SIZE];
            weightedFrozenResult(dl, &w, st, res);
            weightedFrozenReference(type, start, ref);
            assertStrEq(res, ref);
        }
    }
    pddlDatalogDel(dl);

    // The facts of T have the weight 1, so they wait in the queue after
    // the computation with the limit zero and T cannot be frozen
    dl = newWeighted(type);
    weightedFrozenProgram(dl, &w, 1);
    dbCommitFail(dl, &input);
    canonicalModelWithIntLimit(dl, 0,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommitFail(dl, &input);
    // The highest weight of a fact of T is 2 (T(0, 3) and T(1, 3))
    canonicalModelWithIntLimit(dl, 2,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommit(dl, &input);
    assertFacts(dl, w.At, "");
    pddlDatalogDel(dl);
    pddlIVecFree(&input);

    // A goal fact reached with the limit zero (the goal has an empty body)
    // stops the computation before the static facts are processed, but
    // nothing is processed anymore, so the predicates can be frozen
    dl = newWeighted(type);
    int E = addPred(dl, 2, "E");
    int T = addPred(dl, 2, "T");
    int At = addPred(dl, 1, "A");
    int G = addGoalPred(dl, "G");
    int c0 = addConst(dl, "0");
    int c1 = addConst(dl, "1");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(E, c0, c1));
    wrule(dl, 1, ATOM(T, X, Y), 1, ATOM(E, X, Y));
    wrule(dl, 1, ATOM(At, Y), 2, ATOM(At, X), ATOM(T, X, Y));
    fact(dl, ATOM0(G));
    finalize(dl);
    canonicalModelWithIntLimit(dl, 0, PDDL_DATALOG_CANONICAL_MODEL_OK);
    PDDL_IVEC(input_at);
    pddlIVecPush(&input_at, At);
    dbCommit(dl, &input_at);
    pddlIVecFree(&input_at);
    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBRollback(dl);
        addFactToDB(dl, ATOM(At, c0), NULL);
        canonicalModelOK(dl);
        assertGoalWeight(dl, G, 0);
        assertFacts(dl, At, "A0:0");
    }
    pddlDatalogDel(dl);
}

/*
 * Freezing the predicates of a weighted program (sum of the weights) whose
 * facts are all processed: the evaluations from the commit give the same
 * results as fresh programs, and freezing fails while a fact of a
 * predicate to be frozen waits in the queue (unless a goal fact was
 * reached).
 */
TEST_ONCE(datalog_weighted_commit_frozen_add)
{
    weightedFrozenTest(PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * Freezing the predicates of a weighted program (maximum of the weights)
 * whose facts are all processed: the evaluations from the commit give the
 * same results as fresh programs, and freezing fails while a fact of a
 * predicate to be frozen waits in the queue (unless a goal fact was
 * reached).
 */
TEST_ONCE(datalog_weighted_commit_frozen_max)
{
    weightedFrozenTest(PDDL_DATALOG_WEIGHT_MAX);
}

/*
 * With two goal predicates, the computation stops at the first goal fact
 * popped from the queue: pddlDatalogDBGoalWeight() returns its weight and
 * -1 for the other goal, which was not derived yet, and a further call
 * returns immediately.
 */
TEST_ONCE(datalog_weighted_two_goals)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int S = addPred(dl, 0, "S");
    int P = addPred(dl, 0, "P");
    int G = addGoalPred(dl, "G");
    int H = addGoalPred(dl, "H");
    fact(dl, ATOM0(S));
    wrule(dl, 1, ATOM0(G), 1, ATOM0(S));
    wrule(dl, 5, ATOM0(P), 1, ATOM0(S));
    wrule(dl, 1, ATOM0(H), 1, ATOM0(P));
    finalize(dl);

    for (int rep = 0; rep < 2; ++rep){
        canonicalModelOK(dl);
        assertGoalWeight(dl, G, 1);
        pddl_num_t weight;
        int ret = pddlDatalogDBGoalWeight(dl, H, &weight);
        assert(ret == -1);
        assertFacts(dl, P, "P:5");
        assertFacts(dl, H, "");
    }
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
    finalize(dl);
    canonicalModelOK(dl);
    assertNegModel(dl, &n);
    pddlDatalogDel(dl);
}

/** Negative static atoms in a weighted program with the aggregation TYPE */
static void weightedNegStaticTest(pddl_datalog_weight_type_t type)
{
    // The model of negProgram() is the same as in the unweighted program
    pddl_datalog_t *dl = newWeighted(type);
    neg_t n;
    negProgram(dl, &n);
    finalize(dl);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertFacts(dl, n.H, "Ha:0 Hc:0");
    assertFacts(dl, n.N, "Na:0 Nb:0");
    assertFacts(dl, n.J, "Ja:0 Jb:0");
    pddlDatalogDel(dl);

    // H(a) is blocked by the negative atom, so the goal is reached only via
    // the more expensive rule from H(b)
    dl = newWeighted(type);
    int T = addPred(dl, 1, "T");
    int S = addPred(dl, 1, "S");
    int H = addPred(dl, 1, "H");
    int G = addGoalPred(dl, "G");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(T, a));
    fact(dl, ATOM(T, b));
    fact(dl, ATOM(S, a));

    // H(X) :- T(X), !S(X).  with the weight 1
    pddl_datalog_rule_t r;
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    pddl_num_t w;
    pddlNumSetInt(&w, 1);
    pddlDatalogRuleSetWeight(dl, &r, &w);
    ruleAdd(dl, &r);

    rule(dl, ATOM0(G), 1, ATOM(H, a));
    wrule(dl, 3, ATOM0(G), 1, ATOM(H, b));
    finalize(dl);

    canonicalModelOK(dl);
    assertGoalWeight(dl, G, 4);
    assertFacts(dl, H, "Hb:1");
    pddlDatalogDel(dl);
}

/*
 * Negative static atoms in a weighted program with the sum of weights: the
 * model is the same as in the unweighted program, and a rule blocked by a
 * negative atom does not contribute to the weights.
 */
TEST_ONCE(datalog_weighted_neg_static_add)
{
    weightedNegStaticTest(PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * Negative static atoms in a weighted program with the maximum of weights:
 * the model is the same as in the unweighted program, and a rule blocked by
 * a negative atom does not contribute to the weights.
 */
TEST_ONCE(datalog_weighted_neg_static_max)
{
    weightedNegStaticTest(PDDL_DATALOG_WEIGHT_MAX);
}

/** Body-less rules with negative atoms in a program that is unweighted
 *  (WEIGHTED is false) or weighted with the sum of weights */
static void negBodylessTest(pddl_bool_t weighted)
{
    pddl_datalog_t *dl;
    if (weighted){
        dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    }else{
        dl = pddlDatalogNew();
    }
    int S = addPred(dl, 1, "S");
    int A = addPred(dl, 0, "A");
    int B = addPred(dl, 0, "B");
    int C = addPred(dl, 1, "C");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");

    // The rules come before the fact S(b) they depend on
    pddl_datalog_rule_t r;
    // A() :- !S(b).  with the weight 1
    ruleInit(dl, &r, ATOM0(A));
    ruleAddNeg(dl, &r, ATOM(S, b));
    if (weighted){
        pddl_num_t w;
        pddlNumSetInt(&w, 1);
        pddlDatalogRuleSetWeight(dl, &r, &w);
    }
    ruleAdd(dl, &r);
    // B() :- !S(a).  with the weight 2
    ruleInit(dl, &r, ATOM0(B));
    ruleAddNeg(dl, &r, ATOM(S, a));
    if (weighted){
        pddl_num_t w;
        pddlNumSetInt(&w, 2);
        pddlDatalogRuleSetWeight(dl, &r, &w);
    }
    ruleAdd(dl, &r);
    // C(a) :- !S(a), !S(b).
    ruleInit(dl, &r, ATOM(C, a));
    ruleAddNeg(dl, &r, ATOM(S, a));
    ruleAddNeg(dl, &r, ATOM(S, b));
    ruleAdd(dl, &r);
    // C(b) :- !S(a).
    ruleInit(dl, &r, ATOM(C, b));
    ruleAddNeg(dl, &r, ATOM(S, a));
    ruleAdd(dl, &r);
    fact(dl, ATOM(S, b));
    finalize(dl);

    if (weighted){
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        assertFacts(dl, A, "");
        assertFacts(dl, B, "B:2");
        assertFacts(dl, C, "Cb:0");
    }else{
        canonicalModelOK(dl);
        assertFacts(dl, A, "");
        assertFacts(dl, B, "B");
        assertFacts(dl, C, "Cb");
    }
    pddlDatalogDel(dl);
}

/*
 * Body-less rules with negative atoms: their heads are inserted only if the
 * negative atoms hold, also if the facts the negative atoms refer to are
 * added after the rules.
 */
TEST_ONCE(datalog_neg_bodyless)
{
    negBodylessTest(pddl_false);
}

/*
 * Body-less rules with negative atoms in a weighted program: their heads
 * are inserted (with the weights of the rules) only if the negative atoms
 * hold.
 */
TEST_ONCE(datalog_weighted_neg_bodyless)
{
    negBodylessTest(pddl_true);
}

/** Predicates of the program of negJoinProgram() */
struct neg_join {
    int P, Q, S, R, H;
    int a, b, c;
};
typedef struct neg_join neg_join_t;

/** Creates the program with the rule
 *    H(X, Y, Z) :- P(X, Y), Q(Y, Z), !S(X), !S(Z), !S(Y), !R(X, Z).
 *  whose negative atoms are covered by the first body atom, by the second
 *  one, by both, and by neither. The rule has the weight 1 in weighted
 *  programs. The facts of P are added only if ADD_P is true. */
static void negJoinProgram(pddl_datalog_t *dl,
                           neg_join_t *n,
                           pddl_bool_t weighted,
                           pddl_bool_t add_p)
{
    n->P = addPred(dl, 2, "P");
    n->Q = addPred(dl, 2, "Q");
    n->S = addPred(dl, 1, "S");
    n->R = addPred(dl, 2, "R");
    n->H = addPred(dl, 3, "H");
    n->a = addConst(dl, "a");
    n->b = addConst(dl, "b");
    n->c = addConst(dl, "c");
    int a = n->a, b = n->b, c = n->c;
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    if (add_p){
        fact(dl, ATOM(n->P, a, a));
        fact(dl, ATOM(n->P, a, b));
        fact(dl, ATOM(n->P, b, a));
        fact(dl, ATOM(n->P, b, c));
        fact(dl, ATOM(n->P, c, a));
    }
    fact(dl, ATOM(n->Q, a, a));
    fact(dl, ATOM(n->Q, a, b));
    fact(dl, ATOM(n->Q, b, a));
    fact(dl, ATOM(n->Q, b, c));
    fact(dl, ATOM(n->Q, c, c));
    fact(dl, ATOM(n->S, c));
    fact(dl, ATOM(n->R, a, a));

    pddl_datalog_rule_t r;
    ruleInit(dl, &r, ATOM(n->H, X, Y, Z));
    ruleAddBody(dl, &r, ATOM(n->P, X, Y));
    ruleAddBody(dl, &r, ATOM(n->Q, Y, Z));
    ruleAddNeg(dl, &r, ATOM(n->S, X));
    ruleAddNeg(dl, &r, ATOM(n->S, Z));
    ruleAddNeg(dl, &r, ATOM(n->S, Y));
    ruleAddNeg(dl, &r, ATOM(n->R, X, Z));
    if (weighted){
        pddl_num_t w;
        pddlNumSetInt(&w, 1);
        pddlDatalogRuleSetWeight(dl, &r, &w);
    }
    ruleAdd(dl, &r);
}

/** Negative atoms of a two-body rule checked at different points, in an
 *  unweighted program (WEIGHTED is false) or a weighted program with the
 *  aggregation TYPE */
static void negJoinTest(pddl_bool_t weighted, pddl_datalog_weight_type_t type)
{
    // P(b, c) and P(c, a) are filtered by !S(Y) and !S(X) on the first
    // side, Q(b, c) and Q(c, c) by !S(Z) and !S(Y) on the second side, and
    // the joined H(a, a, a) and H(a, b, a) by !R(X, Z)
    pddl_datalog_t *dl;
    if (weighted){
        dl = newWeighted(type);
    }else{
        dl = pddlDatalogNew();
    }
    neg_join_t n;
    negJoinProgram(dl, &n, weighted, pddl_true);
    finalize(dl);
    if (weighted){
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        assertFacts(dl, n.H, "Haab:1 Hbaa:1 Hbab:1");
    }else{
        canonicalModelOK(dl);
        assertFacts(dl, n.H, "Haab Hbaa Hbab");
    }
    pddlDatalogDel(dl);

    // The same program with the facts of P added after a commit freezing
    // all other predicates (the side of Q is kept, the side of P is
    // dropped), over a series of rollbacks
    if (weighted){
        dl = newWeighted(type);
    }else{
        dl = pddlDatalogNew();
    }
    negJoinProgram(dl, &n, weighted, pddl_false);
    finalize(dl);
    if (weighted){
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    }else{
        canonicalModelOK(dl);
    }
    pddl_ivec_t input = PDDL_IVEC_INIT;
    pddlIVecPush(&input, n.P);
    dbCommit(dl, &input);
    pddlIVecFree(&input);

    int a = n.a, b = n.b, c = n.c;
    const atom_spec_t steps[3][3] = {
        { ATOM(n.P, a, a), ATOM(n.P, b, c), ATOM(n.P, c, a) },
        { ATOM(n.P, b, a), ATOM(n.P, a, b), ATOM(n.P, c, a) },
        { ATOM(n.P, a, a), ATOM(n.P, a, b), ATOM(n.P, b, a) },
    };
    const char *expected[2][3] = {
        { "Haab", "Hbaa Hbab", "Haab Hbaa Hbab" },
        { "Haab:1", "Hbaa:1 Hbab:1", "Haab:1 Hbaa:1 Hbab:1" },
    };
    for (int rep = 0; rep < 2; ++rep){
        for (int si = 0; si < 3; ++si){
            pddlDatalogDBRollback(dl);
            for (int fi = 0; fi < 3; ++fi)
                addFactToDB(dl, steps[si][fi], NULL);
            if (weighted){
                canonicalModel(dl,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
                assertFacts(dl, n.H, expected[1][si]);
            }else{
                canonicalModelOK(dl);
                assertFacts(dl, n.H, expected[0][si]);
            }
        }
    }
    pddlDatalogDel(dl);
}

/*
 * Negative atoms of a rule with two body atoms covered by the first body
 * atom, by the second one, by both, and by neither give the expected
 * canonical model, also with a commit freezing predicates and rollbacks.
 */
TEST_ONCE(datalog_neg_join)
{
    negJoinTest(pddl_false, PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * Negative atoms of a rule with two body atoms checked at different points
 * in a weighted program with the sum of weights.
 */
TEST_ONCE(datalog_weighted_neg_join_add)
{
    negJoinTest(pddl_true, PDDL_DATALOG_WEIGHT_ADD);
}

/*
 * Negative atoms of a rule with two body atoms checked at different points
 * in a weighted program with the maximum of weights.
 */
TEST_ONCE(datalog_weighted_neg_join_max)
{
    negJoinTest(pddl_true, PDDL_DATALOG_WEIGHT_MAX);
}

/** Asserts that the finalization of DL fails */
static void finalizeFail(pddl_datalog_t *dl)
{
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogFinalizeProgram(dl, pddl_true, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
}

/*
 * The predicates of negative atoms can be derived by rules with non-empty
 * bodies or with negative atoms as long as the program is stratifiable
 * (the finalization fails otherwise), and the facts of the predicates
 * affecting negative atoms cannot be added to the database once the
 * computation of the canonical model has started.
 */
TEST_ONCE(datalog_neg_pred_errors)
{
    // S(X) :- T(X), K(X).  H(X) :- T(X), !S(X).  with S in the stratum 0
    // and H in the stratum 1
    pddl_datalog_t *dl = pddlDatalogNew();
    int T = addPred(dl, 1, "T");
    int S = addPred(dl, 1, "S");
    int H = addPred(dl, 1, "H");
    int K = addPred(dl, 1, "K");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(T, a));
    fact(dl, ATOM(T, b));
    fact(dl, ATOM(K, a));
    rule(dl, ATOM(S, X), 2, ATOM(T, X), ATOM(K, X));
    pddl_datalog_rule_t r;
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    ruleAdd(dl, &r);
    assert(pddlDatalogIsStratifiable(dl));
    finalize(dl);
    assert(pddlDatalogPredStratum(dl, S) == 0);
    assert(pddlDatalogPredStratum(dl, H) == 1);
    canonicalModelOK(dl);
    assertFacts(dl, S, "Sa");
    assertFacts(dl, H, "Hb");
    pddlDatalogDel(dl);

    // S(a) :- !T(b).  H(X) :- T(X), !S(X).  where T is static, so S is in
    // the stratum 0 and H in the stratum 1
    dl = pddlDatalogNew();
    T = addPred(dl, 1, "T");
    S = addPred(dl, 1, "S");
    H = addPred(dl, 1, "H");
    a = addConst(dl, "a");
    b = addConst(dl, "b");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(T, a));
    ruleInit(dl, &r, ATOM(S, a));
    ruleAddNeg(dl, &r, ATOM(T, b));
    ruleAdd(dl, &r);
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    ruleAdd(dl, &r);
    finalize(dl);
    assert(pddlDatalogPredStratum(dl, T) == 0);
    assert(pddlDatalogPredStratum(dl, S) == 0);
    assert(pddlDatalogPredStratum(dl, H) == 1);
    canonicalModelOK(dl);
    assertFacts(dl, S, "Sa");
    assertFacts(dl, H, "");
    pddlDatalogDel(dl);

    // S(X) :- T(X), !H(X).  H(X) :- T(X), !S(X).  is not stratifiable
    dl = pddlDatalogNew();
    T = addPred(dl, 1, "T");
    S = addPred(dl, 1, "S");
    H = addPred(dl, 1, "H");
    a = addConst(dl, "a");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(T, a));
    ruleInit(dl, &r, ATOM(S, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(H, X));
    ruleAdd(dl, &r);
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    ruleAdd(dl, &r);
    assert(!pddlDatalogIsStratifiable(dl));
    finalizeFail(dl);
    pddlDatalogDel(dl);

    // Facts of S can be added only before the computation starts
    dl = pddlDatalogNew();
    T = addPred(dl, 1, "T");
    S = addPred(dl, 1, "S");
    H = addPred(dl, 1, "H");
    a = addConst(dl, "a");
    b = addConst(dl, "b");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(T, a));
    fact(dl, ATOM(T, b));
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(T, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    ruleAdd(dl, &r);
    finalize(dl);

    addFactToDB(dl, ATOM(S, a), NULL);
    dbCommit(dl, NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "Hb");
    addFactToDBFail(dl, ATOM(S, b), NULL);
    // T is not a predicate of a negative atom
    addFactToDB(dl, ATOM(T, a), NULL);

    // The rollback returns before the start of the computation
    pddlDatalogDBRollback(dl);
    addFactToDB(dl, ATOM(S, b), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "");
    addFactToDBFail(dl, ATOM(S, b), NULL);

    // So does the reset
    pddlDatalogDBReset(dl);
    addFactToDB(dl, ATOM(S, b), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    pddlDatalogDel(dl);

    // Facts of T affect the negative atoms of S through the rule deriving
    // S, so they can be added only before the computation starts; facts of
    // K do not affect any negative atom
    dl = pddlDatalogNew();
    T = addPred(dl, 1, "T");
    S = addPred(dl, 1, "S");
    H = addPred(dl, 1, "H");
    K = addPred(dl, 1, "K");
    a = addConst(dl, "a");
    b = addConst(dl, "b");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(K, a));
    fact(dl, ATOM(K, b));
    rule(dl, ATOM(S, X), 1, ATOM(T, X));
    ruleInit(dl, &r, ATOM(H, X));
    ruleAddBody(dl, &r, ATOM(K, X));
    ruleAddNeg(dl, &r, ATOM(S, X));
    ruleAdd(dl, &r);
    finalize(dl);

    addFactToDB(dl, ATOM(T, a), NULL);
    dbCommit(dl, NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "Hb");
    addFactToDBFail(dl, ATOM(T, b), NULL);
    addFactToDBFail(dl, ATOM(S, b), NULL);
    addFactToDB(dl, ATOM(K, a), NULL);

    pddlDatalogDBRollback(dl);
    addFactToDB(dl, ATOM(T, b), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "");
    pddlDatalogDel(dl);
}

/** Adds the rule HEAD :- B_1, ..., B_n, !N_1, ..., !N_m with the weight
 *  WEIGHT (not set if zero), where the variadic arguments are the
 *  n = BODY_SIZE positive atoms followed by the m = NEG_SIZE negative
 *  atoms; returns the value of pddlDatalogAddRule() */
static int nrule(pddl_datalog_t *dl,
                 int weight,
                 atom_spec_t head,
                 int body_size,
                 int neg_size,
                 ...)
{
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, head);
    va_list ap;
    va_start(ap, neg_size);
    for (int i = 0; i < body_size; ++i)
        ruleAddBody(dl, &rule, va_arg(ap, atom_spec_t));
    for (int i = 0; i < neg_size; ++i)
        ruleAddNeg(dl, &rule, va_arg(ap, atom_spec_t));
    va_end(ap);
    if (weight != 0){
        pddl_num_t w;
        pddlNumSetInt(&w, weight);
        pddlDatalogRuleSetWeight(dl, &rule, &w);
    }
    return ruleAdd(dl, &rule);
}

/*
 * pddlDatalogAddRule() eliminates the positive atoms of the built-in
 * equality predicate by unifying their arguments in the whole rule, removes
 * the negative atoms of two different constants, and does not add the rules
 * that can never fire. The rule passed by the caller is not changed.
 */
TEST_ONCE(datalog_eq_simplify)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 2, "P");
    int S = addPred(dl, 1, "S");
    int H = addPred(dl, 2, "H");
    int E = pddlDatalogAddEqPred(dl, "=");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    // Positive atoms
    int ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, X, Y));
    assert(ret == 0);
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, X, a));
    assert(ret == 1);
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, a, Y));
    assert(ret == 2);
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, a, a));
    assert(ret == 3);
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, X, X));
    assert(ret == 4);
    // A chain of =-atoms
    ret = nrule(dl, 0, ATOM(H, X, Y), 4, 0, ATOM(P, X, Z), ATOM(E, X, Y),
                ATOM(E, Y, Z), ATOM(E, Z, c));
    assert(ret == 5);
    // The replacement applies also to the negative atoms
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 1, ATOM(P, X, Z), ATOM(E, Y, Z),
                ATOM(S, Z));
    assert(ret == 6);
    // Negative atoms: !=(a, b) is removed, the others are kept
    ret = nrule(dl, 0, ATOM(H, X, Y), 1, 3, ATOM(P, X, Y), ATOM(E, X, Y),
                ATOM(E, X, a), ATOM(E, a, b));
    assert(ret == 7);
    ret = nrule(dl, 0, ATOM(H, a, b), 0, 1, ATOM(E, a, b));
    assert(ret == 8);

    // Rules that can never fire
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 0, ATOM(P, X, Y), ATOM(E, a, b));
    assert(ret == -1);
    ret = nrule(dl, 0, ATOM(H, X, Y), 3, 0, ATOM(P, X, Y), ATOM(E, X, a),
                ATOM(E, X, b));
    assert(ret == -1);
    ret = nrule(dl, 0, ATOM(H, X, Y), 1, 1, ATOM(P, X, Y), ATOM(E, X, X));
    assert(ret == -1);
    ret = nrule(dl, 0, ATOM(H, X, Y), 1, 1, ATOM(P, X, Y), ATOM(E, a, a));
    assert(ret == -1);
    ret = nrule(dl, 0, ATOM(H, X, Y), 2, 1, ATOM(P, X, Y), ATOM(E, X, Y),
                ATOM(E, Y, X));
    assert(ret == -1);
    ret = nrule(dl, 0, ATOM(H, a, b), 0, 1, ATOM(E, b, b));
    assert(ret == -1);

    assertProgram(dl,
                  "H(X, X) :- P(X, X).\n"
                  "H(a, Y) :- P(a, Y).\n"
                  "H(X, a) :- P(X, a).\n"
                  "H(X, Y) :- P(X, Y).\n"
                  "H(X, Y) :- P(X, Y).\n"
                  "H(c, c) :- P(c, c).\n"
                  "H(X, Y) :- P(X, Y), !S(Y).\n"
                  "H(X, Y) :- P(X, Y), !=(X, Y), !=(X, a).\n"
                  "H(a, b).\n");

    // The rule of the caller is not changed
    pddl_datalog_rule_t r;
    ruleInit(dl, &r, ATOM(H, X, Y));
    ruleAddBody(dl, &r, ATOM(P, X, Y));
    ruleAddBody(dl, &r, ATOM(E, X, Y));
    ruleAddNeg(dl, &r, ATOM(E, X, a));
    ret = pddlDatalogAddRule(dl, &r);
    assert(ret == 9);
    assert(r.head.arg[0] == X && r.head.arg[1] == Y);
    assert(r.body_size == 2);
    assert(r.body[0].pred == P);
    assert(r.body[0].arg[0] == X && r.body[0].arg[1] == Y);
    assert(r.body[1].pred == E);
    assert(r.body[1].arg[0] == X && r.body[1].arg[1] == Y);
    assert(r.neg_body_size == 1);
    assert(r.neg_body[0].pred == E);
    assert(r.neg_body[0].arg[0] == X && r.neg_body[0].arg[1] == a);
    pddlDatalogRuleFree(dl, &r);
    pddlDatalogDel(dl);

    // The replacement applies also to the weight atoms
    dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    P = addPred(dl, 2, "P");
    H = addPred(dl, 2, "H");
    int F = pddlDatalogAddFunc(dl, 1, "F");
    E = pddlDatalogAddEqPred(dl, "=");
    X = pddlDatalogAddVar(dl, "X");
    Y = pddlDatalogAddVar(dl, "Y");
    Z = pddlDatalogAddVar(dl, "Z");
    ret = frule(dl, 1, WATOMS(ATOM(F, Y)), NULL, NULL, ATOM(H, X, Y), 2,
                ATOM(P, X, Z), ATOM(E, Z, Y));
    assert(ret == 0);
    assertProgram(dl, "H(X, Z) :- P(X, Z). ; w = 1 + F(Z)\n");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by eqProgram() */
struct eq_prog {
    int H, I, J, K, L, M, N;
};
typedef struct eq_prog eq_prog_t;

/** Creates a program with positive and negative atoms of the equality
 *  predicate, which is the built-in one if BUILTIN is true, otherwise an
 *  ordinary predicate with the facts =(o, o) for all constants o. The rules
 *  have non-zero weights if WEIGHTED is true. */
static void eqProgram(pddl_datalog_t *dl,
                      pddl_bool_t builtin,
                      pddl_bool_t weighted,
                      eq_prog_t *p)
{
    int P = addPred(dl, 2, "P");
    int Q = addPred(dl, 1, "Q");
    p->H = addPred(dl, 2, "H");
    p->I = addPred(dl, 1, "I");
    p->J = addPred(dl, 2, "J");
    p->K = addPred(dl, 1, "K");
    p->L = addPred(dl, 2, "L");
    p->M = addPred(dl, 2, "M");
    p->N = addPred(dl, 2, "N");
    int E;
    if (builtin){
        E = pddlDatalogAddEqPred(dl, "=");
    }else{
        E = pddlDatalogAddPred(dl, 2, "=");
    }
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    int W = pddlDatalogAddVar(dl, "W");
    int w = (weighted ? 1 : 0);

    if (!builtin){
        fact(dl, ATOM(E, a, a));
        fact(dl, ATOM(E, b, b));
        fact(dl, ATOM(E, c, c));
    }
    fact(dl, ATOM(P, a, a));
    fact(dl, ATOM(P, a, b));
    fact(dl, ATOM(P, b, a));
    fact(dl, ATOM(P, b, c));
    fact(dl, ATOM(P, c, c));
    fact(dl, ATOM(Q, a));
    fact(dl, ATOM(Q, c));

    // H(X, Y) :- P(X, Y), !=(X, Y).
    nrule(dl, w, ATOM(p->H, X, Y), 1, 1, ATOM(P, X, Y), ATOM(E, X, Y));
    // I(X) :- P(X, Y), =(Y, c).
    nrule(dl, 2 * w, ATOM(p->I, X), 2, 0, ATOM(P, X, Y), ATOM(E, Y, c));
    // J(X, Z) :- P(X, Y), P(Y, Z), !=(X, Z).  (checked after the join)
    nrule(dl, 3 * w, ATOM(p->J, X, Z), 2, 1, ATOM(P, X, Y), ATOM(P, Y, Z),
          ATOM(E, X, Z));
    // K(X) :- P(X, Y), Q(Y), !=(X, a).  (checked at the unification)
    nrule(dl, 4 * w, ATOM(p->K, X), 2, 1, ATOM(P, X, Y), ATOM(Q, Y),
          ATOM(E, X, a));
    // L(X, Y) :- Q(X), Q(Y), =(X, Y).
    nrule(dl, 5 * w, ATOM(p->L, X, Y), 3, 0, ATOM(Q, X), ATOM(Q, Y),
          ATOM(E, X, Y));
    // M(X, Y) :- P(X, Z), P(Z, Y), =(X, Y).
    nrule(dl, 6 * w, ATOM(p->M, X, Y), 3, 0, ATOM(P, X, Z), ATOM(P, Z, Y),
          ATOM(E, X, Y));
    // N(X, W) :- P(X, Y), P(Y, Z), P(Z, W), !=(X, Y), !=(Y, Z), !=(Z, W).
    nrule(dl, 7 * w, ATOM(p->N, X, W), 3, 3,
          ATOM(P, X, Y), ATOM(P, Y, Z), ATOM(P, Z, W),
          ATOM(E, X, Y), ATOM(E, Y, Z), ATOM(E, Z, W));
}

/** Writes the facts of all head predicates of eqProgram() to OUT */
static void eqModelStr(pddl_datalog_t *dl, const eq_prog_t *p, char *out)
{
    facts_t fs;
    fs.size = 0;
    int preds[] = { p->H, p->I, p->J, p->K, p->L, p->M, p->N };
    for (int i = 0; i < (int)(sizeof(preds) / sizeof(int)); ++i)
        pddlDatalogDBForEachFact(dl, preds[i], factCB, &fs);
    factsJoin(&fs, out);
}

/** The canonical model of eqProgram() in an unweighted program */
#define EQ_MODEL \
    "Hab Hba Hbc Ib Ic Jab Jac Jba Jbc Kb Kc Laa Lcc Maa Mbb Mcc" \
    " Nab Nba Nbc"

/*
 * The built-in equality predicate gives the same canonical model as an
 * ordinary predicate with the facts =(o, o), in unweighted and weighted
 * programs.
 */
TEST_ONCE(datalog_eq_model)
{
    for (int builtin = 0; builtin < 2; ++builtin){
        pddl_datalog_t *dl = pddlDatalogNew();
        eq_prog_t p;
        eqProgram(dl, builtin, pddl_false, &p);
        finalize(dl);
        canonicalModelOK(dl);
        char out[STR_SIZE];
        eqModelStr(dl, &p, out);
        assertStrEq(out, EQ_MODEL);
        pddlDatalogDel(dl);
    }

    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        char out[2][STR_SIZE];
        for (int builtin = 0; builtin < 2; ++builtin){
            pddl_datalog_t *dl = newWeighted(types[ti]);
            eq_prog_t p;
            eqProgram(dl, builtin, pddl_true, &p);
            finalize(dl);
            canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
            eqModelStr(dl, &p, out[builtin]);
            pddlDatalogDel(dl);
        }
        assertStrEq(out[1], out[0]);
    }
}

/*
 * Gringo gives the same canonical model of a program with the built-in
 * equality predicate.
 */
TEST_ONCE(datalog_eq_gringo)
{
#ifdef PDDL_CLINGO
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    eq_prog_t p;
    eqProgram(dl, pddl_true, pddl_false, &p);
    finalizeProgram(dl, pddl_false);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assert(!pddlErrIsSet(&err));
    char out[STR_SIZE];
    eqModelStr(dl, &p, out);
    assertStrEq(out, EQ_MODEL);
    pddlDatalogDel(dl);
#endif /* PDDL_CLINGO */
}

/*
 * The normalization merges the auxiliary predicates of two rules that
 * differ only in the order of the arguments of a negative =-atom.
 */
TEST_ONCE(datalog_eq_normal_form_merge)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int B = addPred(dl, 2, "B");
    int D = addPred(dl, 1, "D");
    int H = addPred(dl, 1, "H");
    int I = addPred(dl, 1, "I");
    int E = pddlDatalogAddEqPred(dl, "=");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");

    fact(dl, ATOM(A, a, a));
    fact(dl, ATOM(A, a, b));
    fact(dl, ATOM(A, b, b));
    fact(dl, ATOM(B, a, a));
    fact(dl, ATOM(B, a, b));
    fact(dl, ATOM(B, b, b));
    fact(dl, ATOM(D, a));
    fact(dl, ATOM(D, b));
    nrule(dl, 0, ATOM(H, X), 3, 1, ATOM(A, X, Y), ATOM(B, X, Y), ATOM(D, X),
          ATOM(E, X, Y));
    nrule(dl, 0, ATOM(I, X), 3, 1, ATOM(A, X, Y), ATOM(B, X, Y), ATOM(D, X),
          ATOM(E, Y, X));
    finalize(dl);
    assertProgram(dl,
                  "A(a, a).\n"
                  "A(a, b).\n"
                  "A(b, b).\n"
                  "B(a, a).\n"
                  "B(a, b).\n"
                  "B(b, b).\n"
                  "D(a).\n"
                  "D(b).\n"
                  "H(X) :- D(X), _p6(X).\n"
                  "I(X) :- D(X), _p6(X).\n"
                  "_p6(X) :- A(X, Y), B(X, Y), !=(X, Y).\n");
    canonicalModelOK(dl);
    assertFacts(dl, H, "Ha");
    assertFacts(dl, I, "Ia");
    pddlDatalogDel(dl);
}

/*
 * Facts of the built-in equality predicate cannot be added to the
 * database, and it cannot be an input predicate of a commit; its negative
 * atoms do not restrict anything else.
 */
TEST_ONCE(datalog_eq_errors)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int H = addPred(dl, 1, "H");
    int E = pddlDatalogAddEqPred(dl, "=");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(P, a));
    nrule(dl, 0, ATOM(H, X), 1, 1, ATOM(P, X), ATOM(E, X, a));
    finalize(dl);

    addFactToDBFail(dl, ATOM(E, a, a), NULL);
    addFactToDB(dl, ATOM(P, b), NULL);
    canonicalModelOK(dl);
    assertFacts(dl, H, "Hb");
    addFactToDBFail(dl, ATOM(E, a, b), NULL);

    PDDL_IVEC(input);
    pddlIVecPush(&input, E);
    dbCommitFail(dl, &input);
    pddlIVecPush(&input, P);
    dbCommitFail(dl, &input);
    pddlIVecClear(&input);
    pddlIVecPush(&input, P);
    dbCommit(dl, &input);
    pddlIVecFree(&input);
    pddlDatalogDel(dl);
}

/*
 * Facts added to the database with pddlDatalogDBAddFact() (which
 * initializes the database if necessary), reading facts with
 * pddlDatalogDBFact(), and clearing the database with pddlDatalogDBReset().
 */
TEST_ONCE(datalog_db)
{
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
    finalize(dl);

    assert(dbSize(dl) == 0);
    assertFacts(dl, E, "");
    addFactToDB(dl, ATOM(E, b, c), NULL);
    assertDB(dl, "Ebc");
    canonicalModelOK(dl);
    assertDB(dl, "Eab Ebc Tab Tac Tbc");
    assert(dbSize(dl) == 5);

    int pred, arity, arg[MAX_ARITY];
    assert(pddlDatalogDBFact(dl, -1, &pred, &arity, arg, NULL) == -1);
    assert(pddlDatalogDBFact(dl, 5, &pred, &arity, arg, NULL) == -1);
    assert(pddlDatalogDBFact(dl, 0, &pred, &arity, arg, NULL) == 0);
    assert(pddlDatalogDBFact(dl, 4, &pred, &arity, arg, NULL) == 0);
    assert(arity == 2);

    pddlDatalogDBReset(dl);
    assert(dbSize(dl) == 0);
    assert(pddlDatalogDBFact(dl, 0, &pred, &arity, arg, NULL) == -1);
    assertFacts(dl, E, "");
    assertFacts(dl, T, "");

    canonicalModelOK(dl);
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
    finalize(dl);

    canonicalModelOK(dl);
    dbCommit(dl, NULL);
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
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    assertRollbackSaved(dl, &r);

    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(r.A, r.a, r.c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");
        pddlDatalogDBRollback(dl);
        assertRollbackSaved(dl, &r);

        addFactToDB(dl, ATOM(r.A, r.b, r.c2), NULL);
        addFactToDB(dl, ATOM(r.A, r.c, r.c1), NULL);
        addFactToDB(dl, ATOM(r.A, r.c, r.c5), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Ab2 Ac1 Ac5 Bb Bc C11 C22 Hb2 Hc1 K1 K2");
        pddlDatalogDBRollback(dl);
        assertRollbackSaved(dl, &r);

        // Rollback without any change
        pddlDatalogDBRollback(dl);
        assertRollbackSaved(dl, &r);

        // Facts added without the computation of the canonical model
        addFactToDB(dl, ATOM(r.A, r.a, r.c2), NULL);
        assertDB(dl, "Aa2 C11 C22");
        pddlDatalogDBRollback(dl);
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
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.C, r.c1, r.c2), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "C11 C12 C22");
    pddlDatalogDBRollback(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
    addFactToDB(dl, ATOM(r.A, r.b, r.c1), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Ab1 Bb C11 C22 D57 Hb1 K1");
    pddlDatalogDBRollback(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.A, r.a, r.c1), NULL);
    addFactToDB(dl, ATOM(r.C, r.c1, r.c7), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Aa1 Ba C11 C17 C22 Ha1 Ha7 K1 K7");
    pddlDatalogDBRollback(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
    addFactToDB(dl, ATOM(r.D, r.c7, r.c5), NULL);
    addFactToDB(dl, ATOM(r.A, r.b, r.c1), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Ab1 Bb C11 C22 D57 D75 Hb1 K1");
    pddlDatalogDel(dl);
}

/*
 * A second commit stores the join index changed since the first commit.
 * A(a, 1) processed between the commits stays joinable with C(1, 7) added
 * after any number of rollbacks to the second commit, whereas A(b, 1)
 * joined into the same part of the index after the second commit is
 * removed again: its ID is re-used by D(5, 7), which must not be joined
 * with C(1, 7). Similarly, C(2, 5) added to the part of the index stored by
 * the first commit is removed again: its ID is re-used by D(5, 7), which
 * must not be joined with A(b, 2).
 */
TEST_ONCE(datalog_commit_twice)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    addFactToDB(dl, ATOM(r.A, r.a, r.c1), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");
    dbCommit(dl, NULL);

    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(r.A, r.b, r.c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ab1 Ba Bb C11 C22 Ha1 Hb1 K1");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");

        addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
        addFactToDB(dl, ATOM(r.C, r.c1, r.c7), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ba C11 C17 C22 D57 Ha1 Ha7 K1 K7");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");

        addFactToDB(dl, ATOM(r.C, r.c1, r.c7), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ba C11 C17 C22 Ha1 Ha7 K1 K7");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");

        addFactToDB(dl, ATOM(r.C, r.c2, r.c5), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ba C11 C22 C25 Ha1 K1");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");

        addFactToDB(dl, ATOM(r.D, r.c5, r.c7), NULL);
        addFactToDB(dl, ATOM(r.A, r.b, r.c2), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ab2 Ba Bb C11 C22 D57 Ha1 Hb2 K1 K2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 Ba C11 C22 Ha1 K1");
    }
    pddlDatalogDel(dl);
}

/*
 * Parts of the join index created after a commit and emptied by a rollback
 * are stored by the next commit even if they did not change since: A(a, 5)
 * and A(b, 7) create the parts for Y = 5 and Y = 7 of H(X, Z) :- A(X, Y),
 * C(Y, Z), the rollback empties them, and only Y = 5 receives A(c, 5) before
 * the second commit. A(a, 7) added after the second commit must be removed
 * from Y = 7 by every rollback: its ID is re-used by D(1, 2), which must not
 * be joined with C(7, 1).
 */
TEST_ONCE(datalog_commit_emptied_projections)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    addFactToDB(dl, ATOM(r.A, r.a, r.c5), NULL);
    addFactToDB(dl, ATOM(r.A, r.b, r.c7), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Aa5 Ab7 Ba Bb C11 C22");
    pddlDatalogDBRollback(dl);
    assertRollbackSaved(dl, &r);

    addFactToDB(dl, ATOM(r.A, r.c, r.c5), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Ac5 Bc C11 C22");
    dbCommit(dl, NULL);

    for (int rep = 0; rep < 3; ++rep){
        addFactToDB(dl, ATOM(r.A, r.a, r.c7), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa7 Ac5 Ba Bc C11 C22");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Ac5 Bc C11 C22");

        addFactToDB(dl, ATOM(r.D, r.c1, r.c2), NULL);
        addFactToDB(dl, ATOM(r.C, r.c7, r.c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Ac5 Bc C11 C22 C71 D12");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Ac5 Bc C11 C22");

        addFactToDB(dl, ATOM(r.C, r.c5, r.c2), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Ac5 Bc C11 C22 C52 Hc2 K2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Ac5 Bc C11 C22");
    }
    pddlDatalogDel(dl);
}

/** Maximal number of facts added in datalog_commit_rollback_random */
#define COMMIT_RANDOM_MAX_ADDED 512

/** Predicates and constants of the program created by commitProgram() */
struct commit_prog {
    int E, T, A, H, K;
    /** Constants 1, 2, 3, 4 */
    int n[4];
    /** Constants a, b */
    int x[2];
};
typedef struct commit_prog commit_prog_t;

/** Creates the program with the static fact E(1, 2) and the rules
 *    T(X, Y) :- E(X, Y).    T(X, Z) :- T(X, Y), E(Y, Z).
 *    H(X, Z) :- A(X, Y), T(Y, Z).    K(Z) :- H(X, Z).
 *  Programs created by this function have the same IDs of predicates and
 *  constants. */
static void commitProgram(pddl_datalog_t *dl, commit_prog_t *p)
{
    p->E = addPred(dl, 2, "E");
    p->T = addPred(dl, 2, "T");
    p->A = addPred(dl, 2, "A");
    p->H = addPred(dl, 2, "H");
    p->K = addPred(dl, 1, "K");
    const char *num[4] = { "1", "2", "3", "4" };
    for (int i = 0; i < 4; ++i)
        p->n[i] = addConst(dl, num[i]);
    p->x[0] = addConst(dl, "a");
    p->x[1] = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(p->E, p->n[0], p->n[1]));
    rule(dl, ATOM(p->T, X, Y), 1, ATOM(p->E, X, Y));
    rule(dl, ATOM(p->T, X, Z), 2, ATOM(p->T, X, Y), ATOM(p->E, Y, Z));
    rule(dl, ATOM(p->H, X, Z), 2, ATOM(p->A, X, Y), ATOM(p->T, Y, Z));
    rule(dl, ATOM(p->K, Z), 1, ATOM(p->H, X, Z));
    finalize(dl);
}

/** Writes to OUT the canonical model of the program of commitProgram()
 *  with the facts ADDED[0], ..., ADDED[NUM_ADDED - 1] computed from
 *  scratch */
static void commitReference(const atom_spec_t *added, int num_added, char *out)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    commit_prog_t p;
    commitProgram(dl, &p);
    for (int i = 0; i < num_added; ++i)
        addFactToDB(dl, added[i], NULL);
    canonicalModelOK(dl);
    dbStr(dl, out);
    pddlDatalogDel(dl);
}

/** State of a random sequence of operations over the program of
 *  commitProgram() */
struct commit_random {
    /** ADDED[0..NUM_ADDED) are the facts added and not rolled back,
     *  ADDED[0..NUM_COMMITTED) the ones added before the last commit (-1
     *  before the first commit) */
    atom_spec_t added[COMMIT_RANDOM_MAX_ADDED];
    int num_added;
    int num_committed;
    /** The database stored by the last commit */
    char committed_db[STR_SIZE];
    int num_commits;
    int num_rollbacks;
};
typedef struct commit_random commit_random_t;

/** Records the commit made just now */
static void commitRandomCommitted(pddl_datalog_t *dl, commit_random_t *s)
{
    s->num_committed = s->num_added;
    dbStr(dl, s->committed_db);
    ++s->num_commits;
}

/** Runs NUM_STEPS random operations: added facts A(x, j) (and E(i, j) if
 *  ADD_EDGES is true), canonical models compared with the ones computed
 *  from scratch, commits without input predicates, and rollbacks compared
 *  with the database stored by the last commit */
static void commitRandomSteps(pddl_datalog_t *dl,
                              const commit_prog_t *p,
                              pddl_rand_t *rnd,
                              pddl_bool_t add_edges,
                              int num_steps,
                              commit_random_t *s)
{
    char db[STR_SIZE];
    char ref[STR_SIZE];
    for (int step = 0; step < num_steps; ++step){
        int op = pddlRandInt(rnd) % 20;
        if (op < 7){
            int is_edge = (add_edges && pddlRandInt(rnd) % 2 == 0);
            int arg0 = pddlRandInt(rnd) % (is_edge ? 4 : 2);
            int arg1 = pddlRandInt(rnd) % 4;
            atom_spec_t f;
            if (is_edge){
                f = ATOM(p->E, p->n[arg0], p->n[arg1]);
            }else{
                f = ATOM(p->A, p->x[arg0], p->n[arg1]);
            }
            assert(s->num_added < COMMIT_RANDOM_MAX_ADDED);
            s->added[s->num_added++] = f;
            addFactToDB(dl, f, NULL);

        }else if (op < 12){
            canonicalModelOK(dl);
            commitReference(s->added, s->num_added, ref);
            dbStr(dl, db);
            assertStrEq(db, ref);

        }else if (op < 15){
            dbCommit(dl, NULL);
            commitRandomCommitted(dl, s);

        }else if (s->num_committed >= 0){
            pddlDatalogDBRollback(dl);
            s->num_added = s->num_committed;
            dbStr(dl, db);
            assertStrEq(db, s->committed_db);
            ++s->num_rollbacks;
        }
    }
}

/*
 * A random sequence of added facts E(i, j) and A(x, j), canonical models,
 * commits, and rollbacks (including commits without the computation of the
 * canonical model and series of rollbacks to the same commit). Every
 * canonical model equals the one computed from scratch from the facts
 * added since the creation of the program and not rolled back, and every
 * rollback restores the database stored by the last commit.
 */
TEST_ONCE(datalog_commit_rollback_random)
{
    for (unsigned seed = 1; seed <= 20; ++seed){
        pddl_rand_t rnd;
        pddlRandInit(&rnd, seed);
        pddl_datalog_t *dl = pddlDatalogNew();
        commit_prog_t p;
        commitProgram(dl, &p);
        commit_random_t s;
        s.num_added = 0;
        s.num_committed = -1;
        s.num_commits = s.num_rollbacks = 0;
        commitRandomSteps(dl, &p, &rnd, pddl_true, 400, &s);
        assert(s.num_commits >= 20);
        assert(s.num_rollbacks >= 20);
        pddlDatalogDel(dl);
        pddlRandFree(&rnd);
    }
}

/*
 * The same random sequences, where after the first part the predicates
 * not derivable from A are frozen (E and T, so H(X, Z) :- A(X, Y), T(Y, Z)
 * keeps only the side of T and T(X, Z) :- T(X, Y), E(Y, Z) keeps nothing)
 * and only facts A(x, j) are added: the canonical models and rollbacks are
 * the same as without frozen predicates.
 */
TEST_ONCE(datalog_commit_frozen_random)
{
    for (unsigned seed = 1; seed <= 20; ++seed){
        pddl_rand_t rnd;
        pddlRandInit(&rnd, seed);
        pddl_datalog_t *dl = pddlDatalogNew();
        commit_prog_t p;
        commitProgram(dl, &p);
        commit_random_t s;
        s.num_added = 0;
        s.num_committed = -1;
        s.num_commits = s.num_rollbacks = 0;
        commitRandomSteps(dl, &p, &rnd, pddl_true, 150, &s);

        PDDL_IVEC(input);
        pddlIVecPush(&input, p.A);
        canonicalModelOK(dl);
        dbCommit(dl, &input);
        pddlIVecFree(&input);
        commitRandomCommitted(dl, &s);

        s.num_commits = s.num_rollbacks = 0;
        commitRandomSteps(dl, &p, &rnd, pddl_false, 300, &s);
        assert(s.num_commits >= 10);
        assert(s.num_rollbacks >= 10);
        pddlDatalogDel(dl);
        pddlRandFree(&rnd);
    }
}

/*
 * Commit with the input predicates A and D freezes C and W:
 * H(X, Z) :- A(X, Y), C(Y, Z) keeps only the side of C, J(X, Z) :- A(X, Y),
 * D(Y, Z) keeps both sides, and W(X) :- C(X, Y), C(Y, X) keeps nothing.
 * Facts added after the commit are joined correctly over rollbacks (also
 * when IDs are re-used: D(1, b) and A(a, 1) get the same ID), a commit
 * without input predicates keeps the frozen predicates and an unprocessed
 * fact, and a later commit with the input predicate A freezes D as well.
 */
TEST_ONCE(datalog_commit_frozen)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int C = addPred(dl, 2, "C");
    int D = addPred(dl, 2, "D");
    int H = addPred(dl, 2, "H");
    int J = addPred(dl, 2, "J");
    int W = addPred(dl, 1, "W");
    int K = addPred(dl, 1, "K");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int c1 = addConst(dl, "1");
    int c2 = addConst(dl, "2");
    int c5 = addConst(dl, "5");
    int c7 = addConst(dl, "7");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(C, c1, c1));
    fact(dl, ATOM(C, c2, c2));
    rule(dl, ATOM(H, X, Z), 2, ATOM(A, X, Y), ATOM(C, Y, Z));
    rule(dl, ATOM(J, X, Z), 2, ATOM(A, X, Y), ATOM(D, Y, Z));
    rule(dl, ATOM(W, X), 2, ATOM(C, X, Y), ATOM(C, Y, X));
    rule(dl, ATOM(K, Z), 1, ATOM(H, X, Z));
    finalize(dl);
    canonicalModelOK(dl);
    assertDB(dl, "C11 C22 W1 W2");

    PDDL_IVEC(input);
    pddlIVecPush(&input, A);
    pddlIVecPush(&input, D);
    dbCommit(dl, &input);
    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(A, a, c1), NULL);
        addFactToDB(dl, ATOM(D, c1, b), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 C11 C22 D1b Ha1 Jab K1 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "C11 C22 W1 W2");

        addFactToDB(dl, ATOM(D, c5, c7), NULL);
        addFactToDB(dl, ATOM(A, b, c5), NULL);
        addFactToDB(dl, ATOM(A, c, c2), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Ab5 Ac2 C11 C22 D57 Hc2 Jb7 K2 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "C11 C22 W1 W2");

        addFactToDB(dl, ATOM(D, c1, b), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "C11 C22 D1b W1 W2");
        pddlDatalogDBRollback(dl);
        addFactToDB(dl, ATOM(A, a, c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 C11 C22 Ha1 K1 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "C11 C22 W1 W2");
    }

    // A commit without input predicates and with an unprocessed fact
    addFactToDB(dl, ATOM(A, a, c1), NULL);
    dbCommit(dl, NULL);
    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(D, c1, b), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 C11 C22 D1b Ha1 Jab K1 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 C22 W1 W2");
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 C11 C22 Ha1 K1 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 C22 W1 W2");
    }

    // D is frozen with its fact D(1, b)
    addFactToDB(dl, ATOM(D, c1, b), NULL);
    canonicalModelOK(dl);
    pddlIVecClear(&input);
    pddlIVecPush(&input, A);
    dbCommit(dl, &input);
    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(A, c, c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ac1 C11 C22 D1b Ha1 Hc1 Jab Jcb K1 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 C22 D1b Ha1 Jab K1 W1 W2");

        addFactToDB(dl, ATOM(A, b, c2), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ab2 C11 C22 D1b Ha1 Hb2 Jab K1 K2 W1 W2");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 C22 D1b Ha1 Jab K1 W1 W2");
    }
    pddlIVecFree(&input);
    pddlDatalogDel(dl);
}

/*
 * The commit freezing C (after a commit without input predicates) rebuilds
 * the join index, and the parts kept with facts in them are stored by this
 * commit with their sizes: A(a, 1) is in
 * the part for Y = 1 of J(X, Z) :- A(X, Y), D(Y, Z), and A(b, 1) added to
 * the same part after the commit is removed again by every rollback, so
 * D(1, c) re-using its ID is joined only with A(a, 1).
 */
TEST_ONCE(datalog_commit_frozen_kept_sizes)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int A = addPred(dl, 2, "A");
    int C = addPred(dl, 2, "C");
    int D = addPred(dl, 2, "D");
    int H = addPred(dl, 2, "H");
    int J = addPred(dl, 2, "J");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int c1 = addConst(dl, "1");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(C, c1, c1));
    rule(dl, ATOM(H, X, Z), 2, ATOM(A, X, Y), ATOM(C, Y, Z));
    rule(dl, ATOM(J, X, Z), 2, ATOM(A, X, Y), ATOM(D, Y, Z));
    finalize(dl);
    addFactToDB(dl, ATOM(A, a, c1), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Aa1 C11 Ha1");
    dbCommit(dl, NULL);

    PDDL_IVEC(input);
    pddlIVecPush(&input, A);
    pddlIVecPush(&input, D);
    dbCommit(dl, &input);
    pddlIVecFree(&input);
    for (int rep = 0; rep < 2; ++rep){
        addFactToDB(dl, ATOM(A, b, c1), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 Ab1 C11 Ha1 Hb1");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 Ha1");

        addFactToDB(dl, ATOM(D, c1, c), NULL);
        canonicalModelOK(dl);
        assertDB(dl, "Aa1 C11 D1c Ha1 Jac");
        pddlDatalogDBRollback(dl);
        assertDB(dl, "Aa1 C11 Ha1");
    }
    pddlDatalogDel(dl);
}

/*
 * A commit freezing predicates fails without any change if the canonical
 * model is not computed (before the first one or with a fact added since),
 * for invalid predicates, and if a frozen predicate would become
 * non-frozen. A rollback then returns to the previous commit. In weighted
 * programs, the facts of the frozen predicates must be processed.
 */
TEST_ONCE(datalog_commit_errors)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    commit_prog_t p;
    commitProgram(dl, &p);
    PDDL_IVEC(input);
    pddlIVecPush(&input, p.A);

    // The initial facts are not inserted yet
    dbCommitFail(dl, &input);
    canonicalModelOK(dl);
    assertDB(dl, "E12 T12");
    dbCommit(dl, NULL);

    addFactToDB(dl, ATOM(p.A, p.x[0], p.n[0]), NULL);
    dbCommitFail(dl, &input);
    pddlDatalogDBRollback(dl);
    assertDB(dl, "E12 T12");

    PDDL_IVEC(invalid);
    pddlIVecPush(&invalid, -1);
    dbCommitFail(dl, &invalid);
    pddlIVecClear(&invalid);
    pddlIVecPush(&invalid, p.A);
    pddlIVecPush(&invalid, 1000);
    dbCommitFail(dl, &invalid);
    pddlIVecFree(&invalid);

    // E and T are frozen
    dbCommit(dl, &input);
    addFactToDB(dl, ATOM(p.A, p.x[0], p.n[0]), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Aa1 E12 Ha2 K2 T12");
    PDDL_IVEC(unfreeze);
    pddlIVecPush(&unfreeze, p.E);
    dbCommitFail(dl, &unfreeze);
    pddlIVecClear(&unfreeze);
    pddlIVecPush(&unfreeze, p.A);
    pddlIVecPush(&unfreeze, p.T);
    dbCommitFail(dl, &unfreeze);
    pddlIVecFree(&unfreeze);
    pddlDatalogDBRollback(dl);
    assertDB(dl, "E12 T12");
    pddlDatalogDel(dl);

    // Weighted programs: the initial facts must be inserted first
    dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    commitProgram(dl, &p);
    dbCommitFail(dl, &input);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommit(dl, &input);
    pddlIVecFree(&input);
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
    finalize(dl);
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

    pddl_datalog_t *dl = pddlDatalogNew();
    chain_t ch;
    chainProgram(dl, &ch, &ann_move, &ann_goal);
    canonicalModelOK(dl);
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
    pddlDatalogDBReset(dl);
    addFactToDB(dl, ATOM(ch.At, ch.d), NULL);
    canonicalModelOK(dl);
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
    finalizeProgram(dl, pddl_false);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assertGraphModel(dl, &g);
    pddlDatalogDel(dl);

    dl = pddlDatalogNew();
    neg_t n;
    negProgram(dl, &n);
    finalizeProgram(dl, pddl_false);
    ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assertNegModel(dl, &n);
    // The program is not in the normal form, so the canonical model is
    // refused and the database stays unchanged
    int size = dbSize(dl);
    assert(!pddlDatalogIsNormalForm(dl));
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    assert(dbSize(dl) == size);
    assertNegModel(dl, &n);
    pddlDatalogDel(dl);
#endif /* PDDL_CLINGO */
}

/*
 * Gringo supports only unweighted programs.
 */
TEST_ONCE(datalog_gringo_weighted)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    graph_t g;
    graphProgram(dl, &g);
    finalizeProgram(dl, pddl_false);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);
    pddlDatalogDel(dl);
}

/*
 * Whether the initial facts were inserted is a part of the committed state:
 * after a rollback to a commit made before the first computation of the
 * canonical model, they are inserted again by the next computation; after
 * a rollback to a commit made after it, they stay in the database.
 */
TEST_ONCE(datalog_initial_facts_commit)
{
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
    finalize(dl);

    addFactToDB(dl, ATOM(E, b, c), NULL);
    dbCommit(dl, NULL);
    for (int rep = 0; rep < 2; ++rep){
        assertDB(dl, "Ebc");
        canonicalModelOK(dl);
        assertDB(dl, "Eab Ebc Tab Tac Tbc");
        pddlDatalogDBRollback(dl);
    }
    assertDB(dl, "Ebc");

    pddlDatalogDBReset(dl);
    canonicalModelOK(dl);
    assertDB(dl, "Eab Tab");
    dbCommit(dl, NULL);
    addFactToDB(dl, ATOM(E, b, c), NULL);
    canonicalModelOK(dl);
    assertDB(dl, "Eab Ebc Tab Tac Tbc");
    pddlDatalogDBRollback(dl);
    assertDB(dl, "Eab Tab");
    canonicalModelOK(dl);
    assertDB(dl, "Eab Tab");
    pddlDatalogDel(dl);
}

/*
 * In weighted programs, the derivation tree follows the best weights:
 * At(c) is first derived by the expensive teleport from At(a), then its
 * weight is improved by the move from At(b), so the annotations report the
 * move.
 */
TEST_ONCE(datalog_weighted_annotations)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        ann_out_t out;
        out.len = 0;
        out.buf[0] = '\x0';
        ann_data_t ann_move = { "move", &out };
        ann_data_t ann_teleport = { "teleport", &out };
        ann_data_t ann_goal = { "goal", &out };

        pddl_datalog_t *dl = newWeighted(types[ti]);
        int At = addPred(dl, 1, "A");
        int L = addPred(dl, 2, "L");
        int T = addPred(dl, 2, "T");
        int G = addGoalPred(dl, "G");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int c = addConst(dl, "c");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        fact(dl, ATOM(At, a));
        fact(dl, ATOM(L, a, b));
        fact(dl, ATOM(L, b, c));
        fact(dl, ATOM(T, a, c));
        awrule(dl, 1, annotation, &ann_move, ATOM(At, Y), 2,
               ATOM(At, X), ATOM(L, X, Y));
        awrule(dl, 5, annotation, &ann_teleport, ATOM(At, Y), 2,
               ATOM(At, X), ATOM(T, X, Y));
        arule(dl, annotation, &ann_goal, ATOM0(G), 1, ATOM(At, c));
        finalize(dl);

        canonicalModelOK(dl);
        assertGoalWeight(dl, G, 2);
        pddlDatalogExecuteAnnotations(dl, G);
        assertStrEq(out.buf,
                    "move | (A b) :- (A a) (L a b). w=1\n"
                    "move | (A c) :- (A b) (L b c). w=1\n"
                    "goal | (G) :- (A c). w=0\n");
        pddlDatalogDel(dl);
    }
}

/*
 * The goal predicate does not need a user ID (and the auxiliary predicate
 * created by the normalization has none): the goal fact is found directly
 * in the database by pddlDatalogDBGoalWeight() and
 * pddlDatalogExecuteAnnotations(), also across rollbacks to a commit made
 * before O(c) is added.
 */
TEST_ONCE(datalog_goal_without_user_id)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        ann_out_t out;
        ann_data_t ann_move = { "move", &out };

        pddl_datalog_t *dl = newWeighted(types[ti]);
        int At = addPred(dl, 1, "A");
        int L = addPred(dl, 2, "L");
        int O = addPred(dl, 1, "O");
        int G = pddlDatalogAddGoalPred(dl, "G");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int c = addConst(dl, "c");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        fact(dl, ATOM(At, a));
        fact(dl, ATOM(L, a, b));
        fact(dl, ATOM(L, b, c));
        fact(dl, ATOM(O, b));
        awrule(dl, 1, annotation, &ann_move, ATOM(At, Y), 3,
               ATOM(At, X), ATOM(L, X, Y), ATOM(O, Y));
        rule(dl, ATOM0(G), 1, ATOM(At, c));
        finalize(dl);

        dbCommit(dl, NULL);
        for (int rep = 0; rep < 2; ++rep){
            pddl_num_t w;
            out.len = 0;
            out.buf[0] = '\x0';
            canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
            int ret = pddlDatalogDBGoalWeight(dl, G, &w);
            assert(ret == -1);
            pddlDatalogExecuteAnnotations(dl, G);
            assertStrEq(out.buf, "");
            pddlDatalogDBRollback(dl);

            addFactToDB(dl, ATOM(O, c), NULL);
            canonicalModelOK(dl);
            assertGoalWeight(dl, G, 2);
            pddlDatalogExecuteAnnotations(dl, G);
            assertStrEq(out.buf,
                        "move | (A b) :- (A a) (L a b) (O b). w=1\n"
                        "move | (A c) :- (A b) (L b c) (O c). w=1\n");
            pddlDatalogDBRollback(dl);
        }
        pddlDatalogDel(dl);
    }
}

/*
 * Annotations of weighted programs receive the ground weight of the rule,
 * not the weight of the derived fact: also for an annotated body-less rule
 * with a non-zero weight and for a rule with three body atoms (normalized,
 * the weight stays with the top rule), and also after rollbacks.
 */
TEST_ONCE(datalog_annotations_rule_weights)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    int goal_weight[2] = { 17, 12 };
    for (int ti = 0; ti < 2; ++ti){
        ann_out_t out;
        ann_data_t ann_init = { "init", &out };
        ann_data_t ann_move = { "move", &out };
        ann_data_t ann_goal = { "goal", &out };

        pddl_datalog_t *dl = newWeighted(types[ti]);
        int At = addPred(dl, 1, "A");
        int L = addPred(dl, 2, "L");
        int O = addPred(dl, 1, "O");
        int G = addGoalPred(dl, "G");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int c = addConst(dl, "c");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        awrule(dl, 2, annotation, &ann_init, ATOM(At, a), 0);
        fact(dl, ATOM(L, a, b));
        fact(dl, ATOM(L, b, c));
        fact(dl, ATOM(O, b));
        fact(dl, ATOM(O, c));
        awrule(dl, 3, annotation, &ann_move, ATOM(At, Y), 3,
               ATOM(At, X), ATOM(L, X, Y), ATOM(O, Y));
        awrule(dl, 4, annotation, &ann_goal, ATOM0(G), 2,
               ATOM(At, c), ATOM(At, b));
        finalize(dl);

        dbCommit(dl, NULL);
        for (int rep = 0; rep < 2; ++rep){
            out.len = 0;
            out.buf[0] = '\x0';
            canonicalModelOK(dl);
            // ADD: A(a) = 2, A(b) = 2 + 3, A(c) = 5 + 3, G = 4 + 8 + 5
            // MAX: A(a) = 2, A(b) = 3 + 2, A(c) = 3 + 5, G = 4 + max(8, 5)
            assertGoalWeight(dl, G, goal_weight[ti]);
            pddlDatalogExecuteAnnotations(dl, G);
            assertStrEq(out.buf,
                        "init | (A a) :-. w=2\n"
                        "move | (A b) :- (A a) (L a b) (O b). w=3\n"
                        "move | (A c) :- (A b) (L b c) (O c). w=3\n"
                        "goal | (G) :- (A b) (A c). w=4\n");
            pddlDatalogDBRollback(dl);
        }
        pddlDatalogDel(dl);
    }
}

/** Shortest paths A(Y) :- A(X), E(X, Y) with the lengths of the edges
 *  given by the weight atom D(X, Y) and the goal G :- A(c), A(b) with the
 *  aggregation TYPE; GOAL is the expected weight of the goal */
static void weightAtomsTest(pddl_datalog_weight_type_t type, int goal)
{
    pddl_datalog_t *dl = newWeighted(type);
    int At = addPred(dl, 1, "A");
    int E = addPred(dl, 2, "E");
    int G = addGoalPred(dl, "G");
    int D = pddlDatalogAddFunc(dl, 2, "D");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(At, a));
    fact(dl, ATOM(E, a, b));
    fact(dl, ATOM(E, b, c));
    fact(dl, ATOM(E, a, c));
    fluent(dl, ATOM(D, a, b), 2);
    fluent(dl, ATOM(D, b, c), 3);
    fluent(dl, ATOM(D, a, c), 10);
    frule(dl, 0, WATOMS(ATOM(D, X, Y)), NULL, NULL, ATOM(At, Y), 2,
          ATOM(At, X), ATOM(E, X, Y));
    rule(dl, ATOM0(G), 2, ATOM(At, c), ATOM(At, b));
    assertProgram(dl,
                  "A(a).\n"
                  "E(a, b).\n"
                  "E(b, c).\n"
                  "E(a, c).\n"
                  "A(Y) :- A(X), E(X, Y). ; w = D(X, Y)\n"
                  "G() :- A(c), A(b).\n");
    finalize(dl);

    canonicalModelOK(dl);
    assertGoalWeight(dl, G, goal);
    // A(c) is derived first from A(a) with 10, then improved via A(b)
    assertFacts(dl, At, "Aa:0 Ab:2 Ac:5");
    pddlDatalogDel(dl);
}

/*
 * The weight of a rule given by a weight atom grounded by the variables of
 * the rule (shortest paths with the lengths of edges given by a numeric
 * fluent) with the sum of weights.
 */
TEST_ONCE(datalog_weight_atoms_add)
{
    weightAtomsTest(PDDL_DATALOG_WEIGHT_ADD, 7);
}

/*
 * Same as datalog_weight_atoms_add with the maximum of weights.
 */
TEST_ONCE(datalog_weight_atoms_max)
{
    weightAtomsTest(PDDL_DATALOG_WEIGHT_MAX, 5);
}

/*
 * The ground weight of a rule is the sum of its constant weight and the
 * values of all its weight atoms: an atom added twice counts twice, the
 * variables of the atoms come from different body atoms, and a 0-ary
 * function works as well. Setting the same value of a fluent again does
 * nothing. The annotations receive the whole ground weight.
 */
TEST_ONCE(datalog_weight_atoms_sum)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        ann_out_t out;
        out.len = 0;
        out.buf[0] = '\x0';
        ann_data_t ann_r = { "R", &out };
        ann_data_t ann_s = { "S", &out };

        pddl_datalog_t *dl = newWeighted(types[ti]);
        int P = addPred(dl, 1, "P");
        int Q = addPred(dl, 1, "Q");
        int R = addPred(dl, 2, "R");
        int S = addPred(dl, 0, "S");
        int G = addGoalPred(dl, "G");
        int F = pddlDatalogAddFunc(dl, 1, "F");
        int H = pddlDatalogAddFunc(dl, 2, "H");
        int K = pddlDatalogAddFunc(dl, 0, "K");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        fact(dl, ATOM(P, a));
        fact(dl, ATOM(Q, b));
        fluent(dl, ATOM(F, a), 1);
        fluent(dl, ATOM(F, b), 2);
        fluent(dl, ATOM(H, a, b), 4);
        fluent(dl, ATOM0(K), 8);
        fluent(dl, ATOM(F, a), 1);
        frule(dl, 16,
              WATOMS(ATOM(F, X), ATOM(F, Y), ATOM(H, X, Y), ATOM(F, X)),
              annotation, &ann_r, ATOM(R, X, Y), 2, ATOM(P, X), ATOM(Q, Y));
        frule(dl, 0, WATOMS(ATOM0(K)), annotation, &ann_s, ATOM0(S), 1,
              ATOM(R, X, Y));
        rule(dl, ATOM0(G), 1, ATOM0(S));
        assertProgram(dl,
                      "P(a).\n"
                      "Q(b).\n"
                      "R(X, Y) :- P(X), Q(Y). ; w = 16 + F(X) + F(Y)"
                      " + H(X, Y) + F(X); ann_size = 1\n"
                      "S() :- R(X, Y). ; w = K(); ann_size = 1\n"
                      "G() :- S().\n");
        finalize(dl);

        canonicalModelOK(dl);
        // R(a, b) = 16 + 1 + 2 + 4 + 1, S() = 24 + 8 (the annotations get
        // the ground weights of the rules, i.e., 24 and 8)
        assertFacts(dl, R, "Rab:24");
        assertGoalWeight(dl, G, 32);
        pddlDatalogExecuteAnnotations(dl, G);
        assertStrEq(out.buf,
                    "R | (R a b) :- (P a) (Q b). w=24\n"
                    "S | (S) :- (R a b). w=8\n");
        pddlDatalogDel(dl);
    }
}

/*
 * An overflow of the sum of the values of weight atoms is an error.
 */
TEST_ONCE(datalog_weight_atoms_overflow)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        pddl_datalog_t *dl = newWeighted(types[ti]);
        int P = addPred(dl, 1, "P");
        int Q = addPred(dl, 1, "Q");
        int F = pddlDatalogAddFunc(dl, 1, "F");
        int a = addConst(dl, "a");
        int X = pddlDatalogAddVar(dl, "X");
        fact(dl, ATOM(P, a));
        fluent(dl, ATOM(F, a), INT_MAX - 1);
        frule(dl, 0, WATOMS(ATOM(F, X), ATOM(F, X)), NULL, NULL,
              ATOM(Q, X), 1, ATOM(P, X));
        finalize(dl);
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
        assertFacts(dl, Q, "");
        pddlDatalogDel(dl);
    }
}

/*
 * The normalization keeps the variables of the weight atoms in the
 * auxiliary predicates: Y occurs only in B(X, Y), C(Y), and the weight
 * atom F(Y), so without it, joining B and C would project Y away. The
 * variables of the weight atoms are renamed by the canonization, and the
 * auxiliary predicates of two rules differing only in their weights are
 * merged.
 */
TEST_ONCE(datalog_weight_atoms_normalization)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    int goal[2] = { 7, 4 };
    for (int ti = 0; ti < 2; ++ti){
        pddl_datalog_t *dl = newWeighted(types[ti]);
        int A = addPred(dl, 1, "A");
        int B = addPred(dl, 2, "B");
        int C = addPred(dl, 1, "C");
        int H = addPred(dl, 1, "H");
        int I = addPred(dl, 1, "I");
        int G = addGoalPred(dl, "G");
        int F = pddlDatalogAddFunc(dl, 1, "F");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int c = addConst(dl, "c");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        int Z = pddlDatalogAddVar(dl, "Z");
        int W = pddlDatalogAddVar(dl, "W");
        fact(dl, ATOM(A, a));
        fact(dl, ATOM(B, a, b));
        fact(dl, ATOM(B, a, c));
        fact(dl, ATOM(C, b));
        fact(dl, ATOM(C, c));
        fluent(dl, ATOM(F, b), 5);
        fluent(dl, ATOM(F, c), 3);
        frule(dl, 0, WATOMS(ATOM(F, Y)), NULL, NULL, ATOM(H, X), 3,
              ATOM(A, X), ATOM(B, X, Y), ATOM(C, Y));
        frule(dl, 1, WATOMS(ATOM(F, W)), NULL, NULL, ATOM(I, Z), 3,
              ATOM(A, Z), ATOM(B, Z, W), ATOM(C, W));
        rule(dl, ATOM0(G), 2, ATOM(H, a), ATOM(I, a));
        finalize(dl);
        assertProgram(dl,
                      "A(a).\n"
                      "B(a, b).\n"
                      "B(a, c).\n"
                      "C(b).\n"
                      "C(c).\n"
                      "H(X) :- C(Y), _p7(X, Y). ; w = F(Y)\n"
                      "I(X) :- C(Y), _p7(X, Y). ; w = 1 + F(Y)\n"
                      "G() :- H(a), I(a).\n"
                      "_p7(X, Y) :- A(X), B(X, Y).\n");

        canonicalModelOK(dl);
        // H(a) = min(F(b), F(c)), I(a) = 1 + min(F(b), F(c))
        assertFacts(dl, H, "Ha:3");
        assertFacts(dl, I, "Ia:4");
        assertGoalWeight(dl, G, goal[ti]);
        pddlDatalogDel(dl);
    }
}

/** Paths A(Y) :- A(X), E(X, Y) with the weight D(X, Y) + V(Y), where
 *  D(a, c) has no value, and the goal G :- A(d), computed with the policy
 *  MISSING for the missing values */
static void missingFluentTest(pddl_datalog_weight_type_t type,
                              pddl_datalog_missing_fluent_t missing)
{
    pddl_datalog_t *dl = pddlDatalogNewWeighted(type, missing);
    int At = addPred(dl, 1, "A");
    int E = addPred(dl, 2, "E");
    int G = addGoalPred(dl, "G");
    int D = pddlDatalogAddFunc(dl, 2, "D");
    int V = pddlDatalogAddFunc(dl, 1, "V");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(At, a));
    fact(dl, ATOM(E, a, b));
    fact(dl, ATOM(E, a, c));
    fact(dl, ATOM(E, b, d));
    fact(dl, ATOM(E, c, d));
    fluent(dl, ATOM(D, a, b), 1);
    fluent(dl, ATOM(D, b, d), 1);
    fluent(dl, ATOM(D, c, d), 0);
    fluent(dl, ATOM(V, b), 0);
    fluent(dl, ATOM(V, c), 0);
    fluent(dl, ATOM(V, d), 0);
    frule(dl, 0, WATOMS(ATOM(D, X, Y), ATOM(V, Y)), NULL, NULL,
          ATOM(At, Y), 2, ATOM(At, X), ATOM(E, X, Y));
    rule(dl, ATOM0(G), 1, ATOM(At, d));
    finalize(dl);

    switch (missing){
    case PDDL_DATALOG_MISSING_FLUENT_ZERO:
        // A(c) = 0 + 0, A(d) = 0 via A(c)
        canonicalModelOK(dl);
        assertGoalWeight(dl, G, 0);
        assertFacts(dl, At, "Aa:0 Ab:1 Ac:0 Ad:0");
        break;
    case PDDL_DATALOG_MISSING_FLUENT_ERR:
        // A(c) is derived when A(a) is processed, i.e., before the goal
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
        break;
    case PDDL_DATALOG_MISSING_FLUENT_SKIP_RULE:
        // A(c) is not derived, A(d) = 2 via A(b)
        canonicalModelOK(dl);
        assertGoalWeight(dl, G, 2);
        assertFacts(dl, At, "Aa:0 Ab:1 Ad:2");
        break;
    }
    pddlDatalogDel(dl);
}

/*
 * A missing value of one of two weight atoms of a rule counts as zero.
 */
TEST_ONCE(datalog_weight_atoms_missing_zero)
{
    missingFluentTest(PDDL_DATALOG_WEIGHT_ADD,
                      PDDL_DATALOG_MISSING_FLUENT_ZERO);
    missingFluentTest(PDDL_DATALOG_WEIGHT_MAX,
                      PDDL_DATALOG_MISSING_FLUENT_ZERO);
}

/*
 * A missing value of one of two weight atoms of a rule is an error.
 */
TEST_ONCE(datalog_weight_atoms_missing_err)
{
    missingFluentTest(PDDL_DATALOG_WEIGHT_ADD,
                      PDDL_DATALOG_MISSING_FLUENT_ERR);
    missingFluentTest(PDDL_DATALOG_WEIGHT_MAX,
                      PDDL_DATALOG_MISSING_FLUENT_ERR);
}

/*
 * A missing value of one of two weight atoms of a rule skips the ground
 * rule.
 */
TEST_ONCE(datalog_weight_atoms_missing_skip)
{
    missingFluentTest(PDDL_DATALOG_WEIGHT_ADD,
                      PDDL_DATALOG_MISSING_FLUENT_SKIP_RULE);
    missingFluentTest(PDDL_DATALOG_WEIGHT_MAX,
                      PDDL_DATALOG_MISSING_FLUENT_SKIP_RULE);
}

/*
 * Body-less rules with ground and 0-ary weight atoms; a missing value
 * (of R(b)) is handled by each policy, with the error repeated by every
 * computation, because the initial facts are inserted again.
 */
TEST_ONCE(datalog_weight_atoms_bodyless)
{
    pddl_datalog_missing_fluent_t policies[3] = {
        PDDL_DATALOG_MISSING_FLUENT_ZERO,
        PDDL_DATALOG_MISSING_FLUENT_ERR,
        PDDL_DATALOG_MISSING_FLUENT_SKIP_RULE,
    };
    for (int pi = 0; pi < 3; ++pi){
        pddl_datalog_t *dl;
        dl = pddlDatalogNewWeighted(PDDL_DATALOG_WEIGHT_ADD, policies[pi]);
        int P = addPred(dl, 1, "P");
        int Q = addPred(dl, 0, "Q");
        int R = addPred(dl, 1, "R");
        int G = addGoalPred(dl, "G");
        int F = pddlDatalogAddFunc(dl, 1, "F");
        int K = pddlDatalogAddFunc(dl, 0, "K");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        fluent(dl, ATOM(F, a), 2);
        fluent(dl, ATOM0(K), 3);
        frule(dl, 0, WATOMS(ATOM(F, a)), NULL, NULL, ATOM(P, a), 0);
        frule(dl, 1, WATOMS(ATOM0(K)), NULL, NULL, ATOM0(Q), 0);
        frule(dl, 0, WATOMS(ATOM(F, b)), NULL, NULL, ATOM(R, b), 0);
        rule(dl, ATOM0(G), 2, ATOM(P, a), ATOM0(Q));
        finalize(dl);

        switch (policies[pi]){
        case PDDL_DATALOG_MISSING_FLUENT_ZERO:
            canonicalModelOK(dl);
            assertGoalWeight(dl, G, 6);
            assertFacts(dl, R, "Rb:0");
            break;
        case PDDL_DATALOG_MISSING_FLUENT_ERR:
            canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
            canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
            assertFacts(dl, R, "");
            break;
        case PDDL_DATALOG_MISSING_FLUENT_SKIP_RULE:
            canonicalModelOK(dl);
            assertGoalWeight(dl, G, 6);
            assertFacts(dl, R, "");
            break;
        }
        pddlDatalogDel(dl);
    }
}

/*
 * Weight atoms with commit/rollback and frozen predicates: the static part
 * is computed and frozen once, and every query rolls back, adds its own
 * fact, and gets the weights given by the fluents.
 */
TEST_ONCE(datalog_weight_atoms_commit)
{
    pddl_datalog_weight_type_t types[2] = { PDDL_DATALOG_WEIGHT_ADD,
                                            PDDL_DATALOG_WEIGHT_MAX };
    for (int ti = 0; ti < 2; ++ti){
        pddl_datalog_t *dl = newWeighted(types[ti]);
        int At = addPred(dl, 1, "A");
        int S = addPred(dl, 2, "S");
        int G = addGoalPred(dl, "G");
        int D = pddlDatalogAddFunc(dl, 2, "D");
        int a = addConst(dl, "a");
        int b = addConst(dl, "b");
        int c = addConst(dl, "c");
        int X = pddlDatalogAddVar(dl, "X");
        int Y = pddlDatalogAddVar(dl, "Y");
        fact(dl, ATOM(S, a, b));
        fact(dl, ATOM(S, b, c));
        fact(dl, ATOM(S, a, c));
        fluent(dl, ATOM(D, a, b), 1);
        fluent(dl, ATOM(D, b, c), 1);
        fluent(dl, ATOM(D, a, c), 5);
        frule(dl, 0, WATOMS(ATOM(D, X, Y)), NULL, NULL, ATOM(At, Y), 2,
              ATOM(At, X), ATOM(S, X, Y));
        rule(dl, ATOM0(G), 1, ATOM(At, c));
        finalize(dl);

        canonicalModelWithIntLimit(dl, 0,
                                   PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        PDDL_IVEC(input);
        pddlIVecPush(&input, At);
        dbCommit(dl, &input);
        pddlIVecFree(&input);
        for (int rep = 0; rep < 2; ++rep){
            addFactToDB(dl, ATOM(At, a), NULL);
            canonicalModelOK(dl);
            assertGoalWeight(dl, G, 2);
            pddlDatalogDBRollback(dl);

            addFactToDB(dl, ATOM(At, b), NULL);
            canonicalModelOK(dl);
            assertGoalWeight(dl, G, 1);
            pddlDatalogDBRollback(dl);
        }
        pddlDatalogDel(dl);
    }
}

/*
 * Functions are not predicates: they have no facts in the database and
 * they cannot be input predicates of a commit.
 */
TEST_ONCE(datalog_func_not_pred)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    fluent(dl, ATOM(F, a), 1);
    finalize(dl);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    addFactToDBFail(dl, ATOM(F, a), NULL);
    PDDL_IVEC(input);
    pddlIVecPush(&input, F);
    dbCommitFail(dl, &input);
    pddlIVecFree(&input);
    pddlDatalogDel(dl);
}

/*
 * Adding a numeric fluent to an unweighted program is an error.
 */
TEST_ONCE(datalog_fluent_err_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fluentFail(dl, ATOM(P, a), 1, "only to weighted");
    pddlDatalogDel(dl);
}

/*
 * Setting a different value of a numeric fluent is an error and the
 * original value is kept.
 */
TEST_ONCE(datalog_fluent_err_different_value)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int a = addConst(dl, "a");
    int G = addGoalPred(dl, "G");
    fluent(dl, ATOM(F, a), 1);
    fluentFail(dl, ATOM(F, a), 2, "F(a) already has a different value");
    fluent(dl, ATOM(F, a), 1);
    fluentFail(dl, ATOM(F, a), 2, "F(a) already has a different value");

    fact(dl, ATOM(P, a));
    frule(dl, 3, WATOMS(ATOM(F, a)), NULL, NULL, ATOM0(G), 1, ATOM(P, a));
    finalize(dl);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_OK);
    assertGoalWeight(dl, G, 4);
    pddlDatalogDel(dl);
}

/*
 * A negative value of a numeric fluent is an error.
 */
TEST_ONCE(datalog_fluent_err_negative)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int a = addConst(dl, "a");
    fluentFail(dl, ATOM(F, a), -1, "negative values");
    pddlDatalogDel(dl);
}

/*
 * Setting a value of an atom of a predicate (not a function) is an error.
 */
TEST_ONCE(datalog_fluent_err_not_func)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fluentFail(dl, ATOM(P, a), 1, "Invalid ID of a function");
    pddlDatalogDel(dl);
}

/*
 * A numeric fluent with a variable argument is an error.
 */
TEST_ONCE(datalog_fluent_err_not_const)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int X = pddlDatalogAddVar(dl, "X");
    fluentFail(dl, ATOM(F, X), 1, "Invalid ID of a constant");
    pddlDatalogDel(dl);
}

/*
 * A rule with a non-zero weight cannot be added to an unweighted program.
 */
TEST_PANIC_ONCE(datalog_panic_rule_weight_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    wrule(dl, 1, ATOM(P, a), 0);
}

/*
 * Facts of unweighted programs cannot be read with weights.
 */
TEST_PANIC_ONCE(datalog_panic_fact_weight_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    finalize(dl);
    canonicalModelOK(dl);
    int pred, arity, arg[1];
    pddl_num_t weight;
    int ret = pddlDatalogDBFact(dl, 0, &pred, &arity, arg, &weight);
    assert(ret == 0);
}

/*
 * Unweighted programs have no goal weights.
 */
TEST_PANIC_ONCE(datalog_panic_goal_weight_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int G = addGoalPred(dl, "G");
    fact(dl, ATOM0(G));
    finalize(dl);
    canonicalModelOK(dl);
    pddl_num_t weight;
    int ret = pddlDatalogDBGoalWeight(dl, G, &weight);
    assert(ret == 0);
}

/*
 * Rollback requires a previous commit since the last reset of the
 * database: the commit of rollbackProgram() is dropped by the reset.
 */
TEST_PANIC_ONCE(datalog_panic_rollback_no_commit)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    rollback_t r;
    rollbackProgram(dl, &r);
    pddlDatalogDBReset(dl);
    pddlDatalogDBRollback(dl);
}

/*
 * Only the facts of the predicates with user IDs can be iterated over.
 */
TEST_PANIC_ONCE(datalog_panic_facts_without_user_id)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = pddlDatalogAddPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    finalize(dl);
    canonicalModelOK(dl);
    facts_t fs;
    fs.size = 0;
    pddlDatalogDBForEachFact(dl, P, factCB, &fs);
}


/*
 * The goal weight can be asked only for goal predicates.
 */
TEST_PANIC_ONCE(datalog_panic_goal_weight_not_goal)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    finalize(dl);
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    pddl_num_t weight;
    int ret = pddlDatalogDBGoalWeight(dl, P, &weight);
    assert(ret == 0);
}

/*
 * Annotations can be executed only from goal predicates.
 */
TEST_PANIC_ONCE(datalog_panic_annotations_not_goal)
{
    ann_out_t out;
    out.len = 0;
    ann_data_t ann = { "ann", &out };
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int Q = addPred(dl, 1, "Q");
    int a = addConst(dl, "a");
    int X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(P, a));
    arule(dl, annotation, &ann, ATOM(Q, X), 1, ATOM(P, X));
    finalize(dl);
    canonicalModelOK(dl);
    pddlDatalogExecuteAnnotations(dl, Q);
}

/*
 * Gringo cannot compute the canonical model while any predicate is frozen.
 */
TEST_PANIC_ONCE(datalog_panic_gringo_frozen)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    commit_prog_t p;
    commitProgram(dl, &p);
    canonicalModelOK(dl);
    PDDL_IVEC(input);
    pddlIVecPush(&input, p.A);
    dbCommit(dl, &input);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, NULL);
    assert(ret == 0);
}

/*
 * Rules cannot be given negative weights.
 */
TEST_PANIC_ONCE(datalog_panic_rule_set_negative_weight)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    pddl_datalog_rule_t rule;
    pddlDatalogRuleInit(dl, &rule);
    pddl_num_t w;
    pddlNumSetIntDelta(&w, 0, -1);
    pddlDatalogRuleSetWeight(dl, &rule, &w);
}

/*
 * A rule with a negative weight assigned directly cannot be added.
 */
TEST_PANIC_ONCE(datalog_panic_add_rule_negative_weight)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int S = addPred(dl, 0, "S");
    int P = addPred(dl, 0, "P");
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, ATOM0(P));
    ruleAddBody(dl, &rule, ATOM0(S));
    pddlNumSetInt(&rule.weight, -1);
    pddlDatalogAddRule(dl, &rule);
}

/*
 * Predicates cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_pred_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    pddlDatalogAddPred(dl, 1, "P");
}

/*
 * Goal predicates cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_goal_pred_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    pddlDatalogAddGoalPred(dl, "G");
}

/*
 * Constants cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_const_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    pddlDatalogAddConst(dl, "a");
}

/*
 * Variables cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_var_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    pddlDatalogAddVar(dl, "X");
}

/*
 * Rules cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_rule_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    finalize(dl);
    fact(dl, ATOM(P, a));
}

/*
 * User IDs of predicates cannot be set in a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_set_pred_user_id_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = pddlDatalogAddPred(dl, 1, "P");
    finalize(dl);
    pddlDatalogSetPredUserId(dl, P, 'P');
}

/*
 * User IDs of constants cannot be set in a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_set_const_user_id_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int a = pddlDatalogAddConst(dl, "a");
    finalize(dl);
    pddlDatalogSetConstUserId(dl, a, 'a');
}

/*
 * A program can be finalized only once.
 */
TEST_PANIC_ONCE(datalog_panic_finalize_twice)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    finalizeProgram(dl, pddl_false);
}

/*
 * The canonical model requires a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_canonical_model_not_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    canonicalModelOK(dl);
}

/*
 * The database can be committed only in a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_commit_not_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    addPred(dl, 1, "P");
    dbCommit(dl, NULL);
}

/*
 * The normal form requires a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_normal_form_not_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    fact(dl, ATOM(P, a));
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
}

/*
 * The normal form cannot be created once the database was used, because
 * it changes the program.
 */
TEST_PANIC_ONCE(datalog_panic_normal_form_after_db)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    finalizeProgram(dl, pddl_false);
    addFactToDB(dl, ATOM(P, a), NULL);
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogToNormalForm(dl, &err);
    assert(ret == 0);
}

/*
 * The normal form of a program is determined by its finalization.
 */
TEST_PANIC_ONCE(datalog_panic_is_normal_form_not_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int ret = pddlDatalogIsNormalForm(dl);
    assert(ret);
}

/*
 * The canonical model with a limit requires a weighted program.
 */
TEST_PANIC_ONCE(datalog_panic_with_limit_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 0, "P");
    fact(dl, ATOM0(P));
    finalize(dl);
    canonicalModelWithIntLimit(dl, 1, PDDL_DATALOG_CANONICAL_MODEL_OK);
}

/*
 * Adding facts to the database fails without changing anything if the
 * program is not finalized, the arguments are invalid, the predicate is
 * frozen, or the weight is invalid, i.e., given in an unweighted program,
 * negative, or lower than the weight of the last processed fact (also
 * after it was restored by a rollback).
 */
TEST_ONCE(datalog_add_fact_errors)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_num_t neg, one;
    pddlNumSetIntDelta(&neg, 0, -1);
    pddlNumSetInt(&one, 1);

    // The program is not finalized (the database cannot be read yet)
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    int X = pddlDatalogAddVar(dl, "X");
    int arg[1] = { a };
    int ret = pddlDatalogDBAddFact(dl, P, arg, NULL, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    pddlErrClear(&err);

    // Invalid arguments and weights in an unweighted program
    finalize(dl);
    addFactToDBFail(dl, ATOM(P, a), &pddl_num_zero);
    addFactToDBFail(dl, ATOM(-1, a), NULL);
    addFactToDBFail(dl, ATOM(P + 1, a), NULL);
    addFactToDBFail(dl, ATOM(P, X), NULL);
    addFactToDB(dl, ATOM(P, a), NULL);
    assertDB(dl, "Pa");
    pddlDatalogDel(dl);

    // A frozen predicate
    dl = pddlDatalogNew();
    commit_prog_t cp;
    commitProgram(dl, &cp);
    canonicalModelOK(dl);
    PDDL_IVEC(input);
    pddlIVecPush(&input, cp.A);
    dbCommit(dl, &input);
    pddlIVecFree(&input);
    addFactToDBFail(dl, ATOM(cp.E, cp.n[1], cp.n[2]), NULL);
    addFactToDB(dl, ATOM(cp.A, cp.x[0], cp.n[0]), NULL);
    pddlDatalogDel(dl);

    // Weights in a weighted program: the weights of S, A, B, G are 0, 1, 3,
    // and 6
    dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    limit_prog_t lp;
    limitProgram(dl, &lp);
    addFactToDBFail(dl, ATOM0(lp.B), &neg);
    canonicalModelWithIntLimit(dl, 1,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommit(dl, NULL);
    addFactToDBFail(dl, ATOM0(lp.B), NULL);
    pddlDatalogDBRollback(dl);
    // The weight of the last processed fact A is restored by the rollback
    addFactToDBFail(dl, ATOM0(lp.B), &pddl_num_zero);
    addFactToDB(dl, ATOM0(lp.B), &one);
    assertFacts(dl, lp.B, "B:1");
    canonicalModelOK(dl);
    assertGoalWeight(dl, lp.G, 4);
    pddlDatalogDel(dl);
}

/*
 * Functions cannot be added to unweighted programs.
 */
TEST_PANIC_ONCE(datalog_panic_add_func_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int F = pddlDatalogAddFunc(dl, 1, "F");
    (void)F;
}

/*
 * A rule with a weight atom cannot be added to an unweighted program.
 */
TEST_PANIC_ONCE(datalog_panic_weight_atom_unweighted)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    frule(dl, 0, WATOMS(ATOM(P, a)), NULL, NULL, ATOM(P, a), 0);
}

/*
 * The head of a rule cannot be an atom of a function.
 */
TEST_PANIC_ONCE(datalog_panic_func_in_head)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int a = addConst(dl, "a");
    fact(dl, ATOM(F, a));
}

/*
 * The positive body of a rule cannot contain an atom of a function.
 */
TEST_PANIC_ONCE(datalog_panic_func_in_body)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int X = pddlDatalogAddVar(dl, "X");
    rule(dl, ATOM(P, X), 1, ATOM(F, X));
}

/*
 * The negative body of a rule cannot contain an atom of a function.
 */
TEST_PANIC_ONCE(datalog_panic_func_in_neg_body)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int Q = addPred(dl, 1, "Q");
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int X = pddlDatalogAddVar(dl, "X");
    pddl_datalog_rule_t rule;
    ruleInit(dl, &rule, ATOM(P, X));
    ruleAddBody(dl, &rule, ATOM(Q, X));
    ruleAddNeg(dl, &rule, ATOM(F, X));
    ruleAdd(dl, &rule);
}

/*
 * A weight atom must be an atom of a function.
 */
TEST_PANIC_ONCE(datalog_panic_weight_atom_not_func)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int P = addPred(dl, 1, "P");
    int a = addConst(dl, "a");
    frule(dl, 0, WATOMS(ATOM(P, a)), NULL, NULL, ATOM(P, a), 0);
}

/*
 * Functions cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_func_finalized)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    finalize(dl);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    (void)F;
}

/*
 * Numeric fluents cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_add_fluent_finalized)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    int a = addConst(dl, "a");
    finalize(dl);
    fluent(dl, ATOM(F, a), 1);
}

/*
 * Functions have no user IDs.
 */
TEST_PANIC_ONCE(datalog_panic_set_func_user_id)
{
    pddl_datalog_t *dl = newWeighted(PDDL_DATALOG_WEIGHT_ADD);
    int F = pddlDatalogAddFunc(dl, 1, "F");
    pddlDatalogSetPredUserId(dl, F, 'F');
}

/*
 * The built-in equality predicate can be added only once.
 */
TEST_PANIC_ONCE(datalog_panic_eq_pred_twice)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    pddlDatalogAddEqPred(dl, "=");
    pddlDatalogAddEqPred(dl, "=");
}

/*
 * The built-in equality predicate cannot be added to a finalized program.
 */
TEST_PANIC_ONCE(datalog_panic_eq_pred_finalized)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    finalize(dl);
    pddlDatalogAddEqPred(dl, "=");
}

/*
 * The built-in equality predicate cannot be the head of a rule.
 */
TEST_PANIC_ONCE(datalog_panic_eq_pred_head)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = pddlDatalogAddEqPred(dl, "=");
    int a = pddlDatalogAddConst(dl, "a");
    fact(dl, ATOM(E, a, a));
}

/*
 * The built-in equality predicate cannot have a user ID.
 */
TEST_PANIC_ONCE(datalog_panic_eq_pred_user_id)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = pddlDatalogAddEqPred(dl, "=");
    pddlDatalogSetPredUserId(dl, E, 1);
}

/** Asserts that the stratum of the predicate PRED is STRATUM */
static void assertStratum(const pddl_datalog_t *dl, int pred, int stratum)
{
    int st = pddlDatalogPredStratum(dl, pred);
    if (st != stratum){
        fprintf(stderr, "Predicate %d: expected stratum %d, got %d\n",
                pred, stratum, st);
    }
    assert(st == stratum);
}

/** Predicates of the program created by tcComplementProgram() */
struct tc_comp {
    int N, G, T, C;
};
typedef struct tc_comp tc_comp_t;

/** Creates the program computing the complement of the transitive closure
 *  (P_TCcomp from Abiteboul, Hull, Vianu: Foundations of Databases,
 *  Section 15.1, made safe by the active domain N):
 *    T(X, Y) :- G(X, Y).
 *    T(X, Y) :- G(X, Z), T(Z, Y).
 *    C(X, Y) :- N(X), N(Y), !T(X, Y).
 *  with the nodes a, b, c, d and the edges a -> b -> c. */
static void tcComplementProgram(pddl_datalog_t *dl, tc_comp_t *p)
{
    p->N = addPred(dl, 1, "N");
    p->G = addPred(dl, 2, "G");
    p->T = addPred(dl, 2, "T");
    p->C = addPred(dl, 2, "C");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");

    fact(dl, ATOM(p->N, a));
    fact(dl, ATOM(p->N, b));
    fact(dl, ATOM(p->N, c));
    fact(dl, ATOM(p->N, d));
    fact(dl, ATOM(p->G, a, b));
    fact(dl, ATOM(p->G, b, c));

    rule(dl, ATOM(p->T, X, Y), 1, ATOM(p->G, X, Y));
    rule(dl, ATOM(p->T, X, Y), 2, ATOM(p->G, X, Z), ATOM(p->T, Z, Y));
    nrule(dl, 0, ATOM(p->C, X, Y), 2, 1,
          ATOM(p->N, X), ATOM(p->N, Y), ATOM(p->T, X, Y));
}

/** Asserts the canonical model of tcComplementProgram() */
static void assertTCComplementModel(pddl_datalog_t *dl, const tc_comp_t *p)
{
    assertFacts(dl, p->T, "Tab Tac Tbc");
    assertFacts(dl, p->C, "Caa Cad Cba Cbb Cbd Cca Ccb Ccc Ccd"
                          " Cda Cdb Cdc Cdd");
}

/*
 * The complement of the transitive closure (P_TCcomp from Foundations of
 * Databases, Section 15.1) is computed in two strata: the transitive
 * closure first, then its complement.
 */
TEST_ONCE(datalog_strat_tc_complement)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    tc_comp_t p;
    tcComplementProgram(dl, &p);
    assert(pddlDatalogIsStratifiable(dl));
    finalize(dl);
    assertStratum(dl, p.N, 0);
    assertStratum(dl, p.G, 0);
    assertStratum(dl, p.T, 0);
    assertStratum(dl, p.C, 1);
    canonicalModelOK(dl);
    assertTCComplementModel(dl, &p);
    // Nothing changes by another call
    canonicalModelOK(dl);
    assertTCComplementModel(dl, &p);
    pddlDatalogDel(dl);
}

/** Predicates of the program created by p7Program() */
struct p7 {
    int R, A, B, C, D, S, T, U, V, Q;
};
typedef struct p7 p7_t;

/** Creates the program P7 from Foundations of Databases (Example 15.2.5)
 *    S(X) :- A(X), !R(X).
 *    T(X) :- B(X), !R(X).
 *    U(X) :- C(X), !T(X).
 *    V(X) :- D(X), !S(X), !U(X).
 *  where A, B, C, D stand for R1', R2', R3', R4'. If R_DERIVED is true,
 *  the facts of R are derived by the rule R(X) :- Q(X), otherwise they are
 *  static. */
static void p7Program(pddl_datalog_t *dl, p7_t *p, pddl_bool_t r_derived)
{
    p->R = addPred(dl, 1, "R");
    p->A = addPred(dl, 1, "A");
    p->B = addPred(dl, 1, "B");
    p->C = addPred(dl, 1, "C");
    p->D = addPred(dl, 1, "D");
    p->S = addPred(dl, 1, "S");
    p->T = addPred(dl, 1, "T");
    p->U = addPred(dl, 1, "U");
    p->V = addPred(dl, 1, "V");
    p->Q = addPred(dl, 1, "Q");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int e = addConst(dl, "e");
    int X = pddlDatalogAddVar(dl, "X");

    if (r_derived){
        fact(dl, ATOM(p->Q, a));
        rule(dl, ATOM(p->R, X), 1, ATOM(p->Q, X));
    }else{
        fact(dl, ATOM(p->R, a));
    }
    fact(dl, ATOM(p->A, a));
    fact(dl, ATOM(p->A, b));
    fact(dl, ATOM(p->A, c));
    fact(dl, ATOM(p->B, a));
    fact(dl, ATOM(p->B, b));
    fact(dl, ATOM(p->B, d));
    const int all[] = { a, b, c, d, e };
    for (int i = 0; i < 5; ++i){
        fact(dl, ATOM(p->C, all[i]));
        fact(dl, ATOM(p->D, all[i]));
    }

    // The rules are added in the reverse order of the strata
    nrule(dl, 0, ATOM(p->V, X), 1, 2,
          ATOM(p->D, X), ATOM(p->S, X), ATOM(p->U, X));
    nrule(dl, 0, ATOM(p->U, X), 1, 1, ATOM(p->C, X), ATOM(p->T, X));
    nrule(dl, 0, ATOM(p->T, X), 1, 1, ATOM(p->B, X), ATOM(p->R, X));
    nrule(dl, 0, ATOM(p->S, X), 1, 1, ATOM(p->A, X), ATOM(p->R, X));
}

/** Asserts the canonical model of p7Program() */
static void assertP7Model(pddl_datalog_t *dl, const p7_t *p)
{
    assertFacts(dl, p->R, "Ra");
    assertFacts(dl, p->S, "Sb Sc");
    assertFacts(dl, p->T, "Tb Td");
    assertFacts(dl, p->U, "Ua Uc Ue");
    assertFacts(dl, p->V, "Vd");
}

/*
 * The program P7 from Foundations of Databases (Example 15.2.5) gets the
 * minimal strata: the negative atoms of the static predicate R do not
 * constrain the strata, the negative atoms of the derived predicate R do.
 */
TEST_ONCE(datalog_strat_p7)
{
    for (int derived = 0; derived < 2; ++derived){
        pddl_datalog_t *dl = pddlDatalogNew();
        p7_t p;
        p7Program(dl, &p, derived);
        finalize(dl);
        int r = (derived ? 1 : 0);
        assertStratum(dl, p.R, 0);
        assertStratum(dl, p.A, 0);
        assertStratum(dl, p.D, 0);
        assertStratum(dl, p.S, r);
        assertStratum(dl, p.T, r);
        assertStratum(dl, p.U, r + 1);
        assertStratum(dl, p.V, r + 2);
        canonicalModelOK(dl);
        assertP7Model(dl, &p);
        pddlDatalogDel(dl);
    }
}

/** Asserts that the program DL is not stratifiable, i.e., its finalization
 *  fails and the canonical model is not computed */
static void assertNotStratifiable(pddl_datalog_t *dl)
{
    assert(!pddlDatalogIsStratifiable(dl));
    finalizeFail(dl);
    assert(!pddlDatalogIsStratifiable(dl));
    canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_ERR);
    assert(dbSize(dl) == 0);
}

/*
 * Programs with recursion through negation are not stratifiable
 * (Foundations of Databases, Example 15.1.1 and Example 15.3.1).
 */
TEST_ONCE(datalog_strat_not_stratifiable)
{
    // P1: p :- !p.
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 0, "p");
    int Q = addPred(dl, 0, "q");
    nrule(dl, 0, ATOM0(P), 0, 1, ATOM0(P));
    assertNotStratifiable(dl);
    pddlDatalogDel(dl);

    // P2: p :- !q.  q :- !p.
    dl = pddlDatalogNew();
    P = addPred(dl, 0, "p");
    Q = addPred(dl, 0, "q");
    nrule(dl, 0, ATOM0(P), 0, 1, ATOM0(Q));
    assert(pddlDatalogIsStratifiable(dl));
    nrule(dl, 0, ATOM0(Q), 0, 1, ATOM0(P));
    assertNotStratifiable(dl);
    pddlDatalogDel(dl);

    // Pwin: W(X) :- M(X, Y), !W(Y).
    dl = pddlDatalogNew();
    int M = addPred(dl, 2, "M");
    int W = addPred(dl, 1, "W");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(M, a, b));
    nrule(dl, 0, ATOM(W, X), 1, 1, ATOM(M, X, Y), ATOM(W, Y));
    assertNotStratifiable(dl);
    pddlDatalogDel(dl);

    // Negation in a longer cycle of positive atoms:
    // A(X) :- B(X).  B(X) :- C(X).  C(X) :- N(X), !A(X).
    dl = pddlDatalogNew();
    int A = addPred(dl, 1, "A");
    int B = addPred(dl, 1, "B");
    int C = addPred(dl, 1, "C");
    int N = addPred(dl, 1, "N");
    a = addConst(dl, "a");
    X = pddlDatalogAddVar(dl, "X");
    fact(dl, ATOM(N, a));
    rule(dl, ATOM(A, X), 1, ATOM(B, X));
    rule(dl, ATOM(B, X), 1, ATOM(C, X));
    nrule(dl, 0, ATOM(C, X), 1, 1, ATOM(N, X), ATOM(A, X));
    assertNotStratifiable(dl);
    pddlDatalogDel(dl);
}

/*
 * The stratum of a predicate cannot be queried in a program that is not
 * stratifiable.
 */
TEST_PANIC_ONCE(datalog_panic_pred_stratum_not_stratifiable)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int P = addPred(dl, 0, "p");
    nrule(dl, 0, ATOM0(P), 0, 1, ATOM0(P));
    pddl_err_t err = PDDL_ERR_INIT;
    int ret = pddlDatalogFinalizeProgram(dl, pddl_true, &err);
    assert(ret == -1);
    ret = pddlDatalogPredStratum(dl, P);
}

/*
 * A semipositive program (negation of static predicates only) computing the
 * transitive closure of the complement of a graph (Foundations of
 * Databases, Section 15.2) has a single stratum, also with a negative atom
 * moved to an auxiliary predicate by the normalization.
 */
TEST_ONCE(datalog_strat_semipositive)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int N = addPred(dl, 1, "N");
    int G = addPred(dl, 2, "G");
    int T = addPred(dl, 2, "T");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    const int all[] = { a, b, c };
    for (int i = 0; i < 3; ++i){
        fact(dl, ATOM(N, all[i]));
        for (int j = 0; j < 3; ++j){
            // The complement of G is { (a, b), (b, c) }
            if ((i == 0 && j == 1) || (i == 1 && j == 2))
                continue;
            fact(dl, ATOM(G, all[i], all[j]));
        }
    }
    // T(X, Y) :- N(X), N(Y), !G(X, Y).
    nrule(dl, 0, ATOM(T, X, Y), 2, 1, ATOM(N, X), ATOM(N, Y), ATOM(G, X, Y));
    // T(X, Y) :- N(X), N(Z), T(Z, Y), !G(X, Z).
    nrule(dl, 0, ATOM(T, X, Y), 3, 1,
          ATOM(N, X), ATOM(N, Z), ATOM(T, Z, Y), ATOM(G, X, Z));
    finalize(dl);
    assertStratum(dl, N, 0);
    assertStratum(dl, G, 0);
    assertStratum(dl, T, 0);
    canonicalModelOK(dl);
    assertFacts(dl, T, "Tab Tac Tbc");
    pddlDatalogDel(dl);
}

/** Predicates of the program created by stratChainProgram() */
struct strat_chain {
    int E, P, R, U, Z, Y, W, K, L;
};
typedef struct strat_chain strat_chain_t;

/** Creates a program with four strata, body-less rules with negative atoms
 *  of derived predicates, and a rule with more than two positive body
 *  atoms and a negative atom of a derived predicate:
 *    R(a).  R(Y) :- R(X), E(X, Y).             (stratum 0)
 *    U(X) :- P(X), !R(X).                      (stratum 1)
 *    Z() :- !U(c).  Y() :- !U(a).              (stratum 2)
 *    W(X) :- P(X), E(X, Y), P(Y), !U(Y).       (stratum 2)
 *    K(X) :- W(X), !R(X).                      (stratum 2)
 *    L() :- !Y().                              (stratum 3)
 *  with the nodes a, b, c, d and the edges a -> b, c -> d, d -> a. The
 *  rules with weights have the weight 1 in weighted programs. */
static void stratChainProgram(pddl_datalog_t *dl,
                              strat_chain_t *p,
                              pddl_bool_t weighted)
{
    p->E = addPred(dl, 2, "E");
    p->P = addPred(dl, 1, "P");
    p->R = addPred(dl, 1, "R");
    p->U = addPred(dl, 1, "U");
    p->Z = addPred(dl, 0, "Z");
    p->Y = addPred(dl, 0, "Y");
    p->W = addPred(dl, 1, "W");
    p->K = addPred(dl, 1, "K");
    p->L = addPred(dl, 0, "L");
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int c = addConst(dl, "c");
    int d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int w = (weighted ? 1 : 0);

    // The rules are added before the facts and in the reverse order of
    // the strata
    nrule(dl, w, ATOM0(p->L), 0, 1, ATOM0(p->Y));
    nrule(dl, w, ATOM(p->K, X), 1, 1, ATOM(p->W, X), ATOM(p->R, X));
    nrule(dl, w, ATOM(p->W, X), 3, 1,
          ATOM(p->P, X), ATOM(p->E, X, Y), ATOM(p->P, Y), ATOM(p->U, Y));
    nrule(dl, w, ATOM0(p->Y), 0, 1, ATOM(p->U, a));
    nrule(dl, w, ATOM0(p->Z), 0, 1, ATOM(p->U, c));
    nrule(dl, w, ATOM(p->U, X), 1, 1, ATOM(p->P, X), ATOM(p->R, X));
    nrule(dl, w, ATOM(p->R, Y), 2, 0, ATOM(p->R, X), ATOM(p->E, X, Y));
    fact(dl, ATOM(p->R, a));

    fact(dl, ATOM(p->P, a));
    fact(dl, ATOM(p->P, b));
    fact(dl, ATOM(p->P, c));
    fact(dl, ATOM(p->P, d));
    fact(dl, ATOM(p->E, a, b));
    fact(dl, ATOM(p->E, c, d));
    fact(dl, ATOM(p->E, d, a));
}

/** Asserts the strata of stratChainProgram() */
static void assertStratChainStrata(const pddl_datalog_t *dl,
                                   const strat_chain_t *p)
{
    assertStratum(dl, p->E, 0);
    assertStratum(dl, p->P, 0);
    assertStratum(dl, p->R, 0);
    assertStratum(dl, p->U, 1);
    assertStratum(dl, p->Z, 2);
    assertStratum(dl, p->Y, 2);
    assertStratum(dl, p->W, 2);
    assertStratum(dl, p->K, 2);
    assertStratum(dl, p->L, 3);
}

/*
 * A program with four strata, body-less rules with negative atoms of
 * derived predicates, and a rule split by the normalization with a negative
 * atom of a derived predicate.
 */
TEST_ONCE(datalog_strat_chain)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    strat_chain_t p;
    stratChainProgram(dl, &p, pddl_false);
    finalize(dl);
    assertStratChainStrata(dl, &p);
    canonicalModelOK(dl);
    assertFacts(dl, p.R, "Ra Rb");
    assertFacts(dl, p.U, "Uc Ud");
    assertFacts(dl, p.Z, "");
    assertFacts(dl, p.Y, "Y");
    assertFacts(dl, p.W, "Wa Wd");
    assertFacts(dl, p.K, "Kd");
    assertFacts(dl, p.L, "");

    // The same model after a reset
    pddlDatalogDBReset(dl);
    canonicalModelOK(dl);
    assertFacts(dl, p.U, "Uc Ud");
    assertFacts(dl, p.W, "Wa Wd");
    assertFacts(dl, p.K, "Kd");
    assertFacts(dl, p.Y, "Y");
    pddlDatalogDel(dl);
}

/*
 * A weighted program with four strata: the weights of the facts of each
 * stratum are computed from the final weights of the lower strata.
 */
TEST_ONCE(datalog_weighted_strat_chain)
{
    for (int t = 0; t < 2; ++t){
        pddl_datalog_weight_type_t type = PDDL_DATALOG_WEIGHT_ADD;
        if (t == 1)
            type = PDDL_DATALOG_WEIGHT_MAX;
        pddl_datalog_t *dl = newWeighted(type);
        strat_chain_t p;
        stratChainProgram(dl, &p, pddl_true);
        finalize(dl);
        assertStratChainStrata(dl, &p);
        canonicalModel(dl, PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        // R(b) :- R(a), E(a, b).
        assertFacts(dl, p.R, "Ra:0 Rb:1");
        assertFacts(dl, p.U, "Uc:1 Ud:1");
        assertFacts(dl, p.Z, "");
        assertFacts(dl, p.Y, "Y:1");
        // W(d) :- P(d), E(d, a), P(a).
        assertFacts(dl, p.W, "Wa:1 Wd:1");
        assertFacts(dl, p.K, "Kd:2");
        assertFacts(dl, p.L, "");
        pddlDatalogDel(dl);
    }
}

/** Predicates of the program created by incProgram() */
struct inc {
    int P, E, R, U, I, K, O, W;
    int a, b, c, d;
};
typedef struct inc inc_t;

/** Creates the program
 *    R(a).  R(Y) :- R(X), E(X, Y).        (stratum 0)
 *    U(X) :- P(X), !R(X).                 (stratum 1)
 *    K(X) :- I(X), R(X).                  (stratum 0)
 *    O(X) :- I(X), U(X).                  (stratum 1)
 *    W(X, Y) :- K(X), U(Y).               (stratum 1)
 *  with the nodes a, b, c, d and the edges a -> b, b -> d, c -> a, where
 *  the facts of the input predicate I are added to the database. */
static void incProgram(pddl_datalog_t *dl, inc_t *p)
{
    p->P = addPred(dl, 1, "P");
    p->E = addPred(dl, 2, "E");
    p->R = addPred(dl, 1, "R");
    p->U = addPred(dl, 1, "U");
    p->I = addPred(dl, 1, "I");
    p->K = addPred(dl, 1, "K");
    p->O = addPred(dl, 1, "O");
    p->W = addPred(dl, 2, "W");
    int a = p->a = addConst(dl, "a");
    int b = p->b = addConst(dl, "b");
    int c = p->c = addConst(dl, "c");
    int d = p->d = addConst(dl, "d");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    fact(dl, ATOM(p->P, a));
    fact(dl, ATOM(p->P, b));
    fact(dl, ATOM(p->P, c));
    fact(dl, ATOM(p->P, d));
    fact(dl, ATOM(p->E, a, b));
    fact(dl, ATOM(p->E, b, d));
    fact(dl, ATOM(p->E, c, a));
    fact(dl, ATOM(p->R, a));
    rule(dl, ATOM(p->R, Y), 2, ATOM(p->R, X), ATOM(p->E, X, Y));
    nrule(dl, 0, ATOM(p->U, X), 1, 1, ATOM(p->P, X), ATOM(p->R, X));
    rule(dl, ATOM(p->K, X), 2, ATOM(p->I, X), ATOM(p->R, X));
    rule(dl, ATOM(p->O, X), 2, ATOM(p->I, X), ATOM(p->U, X));
    rule(dl, ATOM(p->W, X, Y), 2, ATOM(p->K, X), ATOM(p->U, Y));
}

/*
 * The facts of an input predicate feeding the strata 0 and 1 are added
 * after a commit freezing the other predicates, over a series of
 * rollbacks; the higher strata are computed again from the new facts of
 * the lower strata, and the facts of the predicates affecting negative
 * atoms cannot be added.
 */
TEST_ONCE(datalog_strat_commit)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    inc_t p;
    incProgram(dl, &p);
    finalize(dl);
    assertStratum(dl, p.R, 0);
    assertStratum(dl, p.U, 1);
    assertStratum(dl, p.I, 0);
    assertStratum(dl, p.K, 0);
    assertStratum(dl, p.O, 1);
    assertStratum(dl, p.W, 1);
    int a = p.a, b = p.b, c = p.c, d = p.d;

    pddl_ivec_t input = PDDL_IVEC_INIT;
    pddlIVecPush(&input, p.I);
    dbCommitFail(dl, &input);
    canonicalModelOK(dl);
    assertFacts(dl, p.R, "Ra Rb Rd");
    assertFacts(dl, p.U, "Uc");
    dbCommit(dl, &input);
    pddlIVecFree(&input);

    const atom_spec_t steps[3][2] = {
        { ATOM(p.I, a), ATOM(p.I, c) },
        { ATOM(p.I, b), ATOM(p.I, d) },
        { ATOM(p.I, c), ATOM(p.I, c) },
    };
    const char *expected[3][3] = {
        { "Ka", "Oc", "Wac" },
        { "Kb Kd", "", "Wbc Wdc" },
        { "", "Oc", "" },
    };
    for (int rep = 0; rep < 2; ++rep){
        for (int si = 0; si < 3; ++si){
            pddlDatalogDBRollback(dl);
            for (int fi = 0; fi < 2; ++fi)
                addFactToDB(dl, steps[si][fi], NULL);
            canonicalModelOK(dl);
            assertFacts(dl, p.K, expected[si][0]);
            assertFacts(dl, p.O, expected[si][1]);
            assertFacts(dl, p.W, expected[si][2]);
            assertFacts(dl, p.U, "Uc");
            // E and R affect the negative atom !R(X) and they are frozen
            addFactToDBFail(dl, ATOM(p.E, d, c), NULL);
            addFactToDBFail(dl, ATOM(p.R, c), NULL);
        }
    }
    pddlDatalogDel(dl);
}

/** Predicates of the program created by weightedStratProgram() */
struct wstrat {
    int A, B, C, D, F, X, E, H, G, I, Q;
};
typedef struct wstrat wstrat_t;

/** Creates the weighted program with the goal G:
 *    A.  B.                                   (static)
 *    C :- A.  (1)     D :- B.  (5)            (stratum 0)
 *    F :- X.  (0)     Q :- I.  (10)           (stratum 0, F has no facts)
 *    E :- !F.  (3)                            (stratum 1)
 *    H :- D, !C.  (1)                         (stratum 1, blocked)
 *    G :- E, D.  (1)  G :- C, !F.  (10)       (stratum 1)
 *  where I is an input predicate without facts. */
static void weightedStratProgram(pddl_datalog_t *dl, wstrat_t *p)
{
    p->A = addPred(dl, 0, "A");
    p->B = addPred(dl, 0, "B");
    p->C = addPred(dl, 0, "C");
    p->D = addPred(dl, 0, "D");
    p->F = addPred(dl, 0, "F");
    p->X = addPred(dl, 0, "X");
    p->E = addPred(dl, 0, "E");
    p->H = addPred(dl, 0, "H");
    p->G = addGoalPred(dl, "G");
    p->I = addPred(dl, 0, "I");
    p->Q = addPred(dl, 0, "Q");
    fact(dl, ATOM0(p->A));
    fact(dl, ATOM0(p->B));
    wrule(dl, 1, ATOM0(p->C), 1, ATOM0(p->A));
    wrule(dl, 5, ATOM0(p->D), 1, ATOM0(p->B));
    rule(dl, ATOM0(p->F), 1, ATOM0(p->X));
    wrule(dl, 10, ATOM0(p->Q), 1, ATOM0(p->I));
    nrule(dl, 3, ATOM0(p->E), 0, 1, ATOM0(p->F));
    nrule(dl, 1, ATOM0(p->H), 1, 1, ATOM0(p->D), ATOM0(p->C));
    wrule(dl, 1, ATOM0(p->G), 2, ATOM0(p->E), ATOM0(p->D));
    nrule(dl, 10, ATOM0(p->G), 1, 1, ATOM0(p->C), ATOM0(p->F));
}

/** Weighted program with two strata and the aggregation TYPE, where
 *  GOAL_W is the expected weight of the goal */
static void weightedStratTest(pddl_datalog_weight_type_t type, int goal_w)
{
    pddl_datalog_t *dl = newWeighted(type);
    wstrat_t p;
    weightedStratProgram(dl, &p);
    finalize(dl);
    assertStratum(dl, p.C, 0);
    assertStratum(dl, p.D, 0);
    assertStratum(dl, p.F, 0);
    assertStratum(dl, p.E, 1);
    assertStratum(dl, p.H, 1);
    assertStratum(dl, p.G, 1);
    canonicalModelOK(dl);
    assertGoalWeight(dl, p.G, goal_w);
    assertFacts(dl, p.C, "C:1");
    assertFacts(dl, p.D, "D:5");
    assertFacts(dl, p.E, "E:3");
    assertFacts(dl, p.H, "");
    pddlDatalogDel(dl);

    // The computation with limits stops in the stratum 0 (D waits in the
    // queue, the stratum 1 is not started), and in the stratum 1 (G waits
    // in the queue); the commits in between restore the per-stratum state
    dl = newWeighted(type);
    weightedStratProgram(dl, &p);
    finalize(dl);
    canonicalModelWithIntLimit(dl, 2,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertFacts(dl, p.C, "C:1");
    assertFacts(dl, p.D, "D:5");
    assertFacts(dl, p.E, "");
    dbCommit(dl, NULL);
    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBRollback(dl);
        canonicalModelWithIntLimit(dl, 5,
                                   PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
        assertFacts(dl, p.E, "E:3");
        canonicalModelOK(dl);
        assertGoalWeight(dl, p.G, goal_w);
    }

    pddlDatalogDBRollback(dl);
    canonicalModelWithIntLimit(dl, 5,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    dbCommit(dl, NULL);
    for (int rep = 0; rep < 2; ++rep){
        pddlDatalogDBRollback(dl);
        assertFacts(dl, p.E, "E:3");
        canonicalModelOK(dl);
        assertGoalWeight(dl, p.G, goal_w);
    }
    pddlDatalogDel(dl);

    // Freezing all predicates but I and Q fails while the stratum 1 (with
    // rules deriving frozen predicates) is not started, even though all
    // facts of the predicates to be frozen are processed
    dl = newWeighted(type);
    weightedStratProgram(dl, &p);
    finalize(dl);
    addFactToDB(dl, ATOM0(p.I), NULL);
    canonicalModelWithIntLimit(dl, 5,
                               PDDL_DATALOG_CANONICAL_MODEL_GOAL_UNREACHED);
    assertFacts(dl, p.D, "D:5");
    assertFacts(dl, p.Q, "Q:10");
    pddl_ivec_t input = PDDL_IVEC_INIT;
    pddlIVecPush(&input, p.I);
    dbCommitFail(dl, &input);
    canonicalModelOK(dl);
    dbCommit(dl, &input);
    pddlIVecFree(&input);
    pddlDatalogDel(dl);
}

/*
 * A weighted program with two strata and the sum of weights: the goal of
 * the stratum 1 is reached through a negative atom of the stratum 0, also
 * with limits stopping in either stratum and commits in between.
 */
TEST_ONCE(datalog_weighted_strat_add)
{
    // G :- E, D.  1 + 3 + 5
    weightedStratTest(PDDL_DATALOG_WEIGHT_ADD, 9);
}

/*
 * A weighted program with two strata and the maximum of weights: the goal
 * of the stratum 1 is reached through a negative atom of the stratum 0,
 * also with limits stopping in either stratum and commits in between.
 */
TEST_ONCE(datalog_weighted_strat_max)
{
    // G :- E, D.  1 + max(3, 5)
    weightedStratTest(PDDL_DATALOG_WEIGHT_MAX, 6);
}

/*
 * Gringo gives the same canonical models of stratified programs.
 */
TEST_ONCE(datalog_strat_gringo)
{
#ifdef PDDL_CLINGO
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_datalog_t *dl = pddlDatalogNew();
    tc_comp_t tc;
    tcComplementProgram(dl, &tc);
    finalizeProgram(dl, pddl_false);
    int ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
    assert(ret == 0);
    assertTCComplementModel(dl, &tc);
    pddlDatalogDel(dl);

    for (int derived = 0; derived < 2; ++derived){
        dl = pddlDatalogNew();
        p7_t p;
        p7Program(dl, &p, derived);
        finalizeProgram(dl, pddl_false);
        ret = pddlDatalogCanonicalModelGringo(dl, NULL, &err);
        assert(ret == 0);
        assertP7Model(dl, &p);
        pddlDatalogDel(dl);
    }
#endif /* PDDL_CLINGO */
}

/*
 * The normalization merges the auxiliary predicates of rules of different
 * strata: the auxiliary predicate created for a rule of the stratum 1 is
 * used also by a rule of the stratum 0, so the strata are recomputed for
 * the normal form rather than carried over from the rules.
 */
TEST_ONCE(datalog_strat_normal_form_merge)
{
    pddl_datalog_t *dl = pddlDatalogNew();
    int E = addPred(dl, 2, "E");
    int U = addPred(dl, 1, "U");
    int R = addPred(dl, 1, "R");
    int M = addPred(dl, 2, "M");
    int H = addPred(dl, 2, "H");
    int P = addPred(dl, 1, "P");
    int S = addPred(dl, 1, "S");
    // The auxiliary predicate created by the normalization
    int aux = S + 1;
    int a = addConst(dl, "a");
    int b = addConst(dl, "b");
    int W = pddlDatalogAddVar(dl, "W");
    int X = pddlDatalogAddVar(dl, "X");
    int Y = pddlDatalogAddVar(dl, "Y");
    int Z = pddlDatalogAddVar(dl, "Z");
    fact(dl, ATOM(P, a));
    fact(dl, ATOM(P, b));
    fact(dl, ATOM(S, a));
    fact(dl, ATOM(E, a, b));
    fact(dl, ATOM(E, b, a));
    rule(dl, ATOM(R, X), 1, ATOM(S, X));
    nrule(dl, 0, ATOM(U, X), 1, 1, ATOM(P, X), ATOM(R, X));
    // H(X, W) :- E(X, Y), E(Y, Z), U(W).  (stratum 1)
    rule(dl, ATOM(H, X, W), 3, ATOM(E, X, Y), ATOM(E, Y, Z), ATOM(U, W));
    // M(X, W) :- E(X, Y), E(Y, Z), P(W).  (stratum 0)
    rule(dl, ATOM(M, X, W), 3, ATOM(E, X, Y), ATOM(E, Y, Z), ATOM(P, W));
    finalize(dl);
    assertProgram(dl,
                  "P(a).\n"
                  "P(b).\n"
                  "S(a).\n"
                  "E(a, b).\n"
                  "E(b, a).\n"
                  "R(W) :- S(W).\n"
                  "U(W) :- P(W), !R(W).\n"
                  "H(W, X) :- U(X), _p7(W).\n"
                  "M(W, X) :- P(X), _p7(W).\n"
                  "_p7(W) :- E(W, X), E(X, Y).\n");
    assertStratum(dl, R, 0);
    assertStratum(dl, U, 1);
    assertStratum(dl, H, 1);
    assertStratum(dl, M, 0);
    assertStratum(dl, aux, 0);
    canonicalModelOK(dl);
    assertFacts(dl, U, "Ub");
    assertFacts(dl, H, "Hab Hbb");
    assertFacts(dl, M, "Maa Mab Mba Mbb");
    pddlDatalogDel(dl);
}
