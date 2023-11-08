#include "test.h"
#include "context.h"
#include <assert.h>


static void printFact(int pred_user_id,
                      int arity,
                      const int *arg_user_id,
                      void *user_data)
{
    printf("(%c", pred_user_id);
    for (int i = 0; i < arity; ++i)
        printf(" %c", arg_user_id[i]);
    printf(")\n");
}

TEST_EXPLICIT(datalog)
{
    pddl_datalog_t *dl;

    dl = pddlDatalogNew();
    unsigned P = pddlDatalogAddPred(dl, 2, "P");
    unsigned Q = pddlDatalogAddPred(dl, 2, "Q");
    unsigned R = pddlDatalogAddPred(dl, 3, "R");
    pddlDatalogSetUserId(dl, R, 'R');
    unsigned S = pddlDatalogAddPred(dl, 2, "S");
    unsigned c1 = pddlDatalogAddConst(dl, "1");
    unsigned c2 = pddlDatalogAddConst(dl, "2");
    unsigned v1 = pddlDatalogAddVar(dl, "X");
    unsigned v2 = pddlDatalogAddVar(dl, "Y");
    unsigned v3 = pddlDatalogAddVar(dl, "Z");
    unsigned v4 = pddlDatalogAddVar(dl, "W");

    pddl_datalog_atom_t atom;
    pddl_datalog_rule_t rule;

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, c1);
    pddlDatalogAtomSetArg(dl, &atom, 1, c2);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, c2);
    pddlDatalogAtomSetArg(dl, &atom, 1, c1);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, R);
    pddlDatalogAtomSetArg(dl, &atom, 0, c2);
    pddlDatalogAtomSetArg(dl, &atom, 1, c1);
    pddlDatalogAtomSetArg(dl, &atom, 2, c1);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);


    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, R);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogAtomSetArg(dl, &atom, 2, v4);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, v2);
    pddlDatalogAtomSetArg(dl, &atom, 1, v3);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, S);
    pddlDatalogAtomSetArg(dl, &atom, 0, v3);
    pddlDatalogAtomSetArg(dl, &atom, 1, v4);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogPrint(dl, stdout);

    pddl_err_t err = PDDL_ERR_INIT;
    //pddlErrInfoEnable(&err, stderr);
    fprintf(stdout, "Normalized:\n");
    pddlDatalogToNormalForm(dl, &err);

    pddlDatalogCanonicalModel(dl, &err);
    pddlDatalogPrint(dl, stdout);
    pddlDatalogDel(dl);
}

static void annotation(pddl_datalog_t *dl,
                       int head_fact_id,
                       const pddl_iset_t *body_fact_ids,
                       void *userdata)
{
    int pred;
    int arity;
    int arg[10];

    printf("ann: %s | ", (char *)userdata);
    int st = pddlDatalogFact(dl, head_fact_id, &pred, &arity, arg, NULL);
    assert(st == 0);
    printf("(%c", pred);
    for (int i = 0; i < arity; ++i)
        printf(" %c", arg[i]);
    printf(")");

    printf(" :-");

    int fact_id;
    PDDL_ISET_FOR_EACH(body_fact_ids, fact_id){
        int st = pddlDatalogFact(dl, fact_id, &pred, &arity, arg, NULL);
        assert(st == 0);
        printf(" (%c", pred);
        for (int i = 0; i < arity; ++i)
            printf(" %c", arg[i]);
        printf(")");
    }

    printf(".\n");
}

TEST_EXPLICIT(datalog_annotated)
{
    pddl_datalog_t *dl;

    dl = pddlDatalogNew();
    unsigned P = pddlDatalogAddPred(dl, 2, "P");
    pddlDatalogSetUserId(dl, P, 'P');
    unsigned Q = pddlDatalogAddPred(dl, 2, "Q");
    pddlDatalogSetUserId(dl, Q, 'Q');
    unsigned R = pddlDatalogAddPred(dl, 3, "R");
    pddlDatalogSetUserId(dl, R, 'R');
    unsigned S = pddlDatalogAddPred(dl, 2, "S");
    pddlDatalogSetUserId(dl, S, 'S');
    unsigned G = pddlDatalogAddGoalPred(dl, "G");
    pddlDatalogSetUserId(dl, G, 'G');
    unsigned c1 = pddlDatalogAddConst(dl, "1");
    pddlDatalogSetUserId(dl, c1, '1');
    unsigned c2 = pddlDatalogAddConst(dl, "2");
    pddlDatalogSetUserId(dl, c2, '2');
    unsigned v1 = pddlDatalogAddVar(dl, "X");
    pddlDatalogSetUserId(dl, v1, 'X');
    unsigned v2 = pddlDatalogAddVar(dl, "Y");
    pddlDatalogSetUserId(dl, v2, 'Y');
    unsigned v3 = pddlDatalogAddVar(dl, "Z");
    pddlDatalogSetUserId(dl, v3, 'Z');
    unsigned v4 = pddlDatalogAddVar(dl, "W");
    pddlDatalogSetUserId(dl, v4, 'W');

    pddl_datalog_atom_t atom;
    pddl_datalog_rule_t rule;

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, c1);
    pddlDatalogAtomSetArg(dl, &atom, 1, c2);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, c2);
    pddlDatalogAtomSetArg(dl, &atom, 1, c1);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, S);
    pddlDatalogAtomSetArg(dl, &atom, 0, c1);
    pddlDatalogAtomSetArg(dl, &atom, 1, c2);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogRuleAddAnnotation(dl, &rule, annotation, "Q(X,Y)");
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, R);
    pddlDatalogAtomSetArg(dl, &atom, 0, c2);
    pddlDatalogAtomSetArg(dl, &atom, 1, c1);
    pddlDatalogAtomSetArg(dl, &atom, 2, c1);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogRuleAddAnnotation(dl, &rule, annotation, "R(2,1,1)");
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);


    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, R);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogAtomSetArg(dl, &atom, 2, v4);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, P);
    pddlDatalogAtomSetArg(dl, &atom, 0, v1);
    pddlDatalogAtomSetArg(dl, &atom, 1, v2);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, Q);
    pddlDatalogAtomSetArg(dl, &atom, 0, v2);
    pddlDatalogAtomSetArg(dl, &atom, 1, v3);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogAtomInit(dl, &atom, S);
    pddlDatalogAtomSetArg(dl, &atom, 0, v3);
    pddlDatalogAtomSetArg(dl, &atom, 1, v4);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);

    pddlDatalogRuleAddAnnotation(dl, &rule, annotation, "R(X,Y,W)");
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogRuleInit(dl, &rule);
    pddlDatalogAtomInit(dl, &atom, R);
    pddlDatalogAtomSetArg(dl, &atom, 0, c1);
    pddlDatalogAtomSetArg(dl, &atom, 1, c2);
    pddlDatalogAtomSetArg(dl, &atom, 2, c2);
    pddlDatalogRuleAddBody(dl, &rule, &atom);
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAtomInit(dl, &atom, G);
    pddlDatalogRuleSetHead(dl, &rule, &atom);
    pddlDatalogRuleAddAnnotation(dl, &rule, annotation, "G");
    pddlDatalogAtomFree(dl, &atom);
    pddlDatalogAddRule(dl, &rule);
    pddlDatalogRuleFree(dl, &rule);

    pddlDatalogPrint(dl, stdout);

    pddl_err_t err = PDDL_ERR_INIT;
    //pddlErrInfoEnable(&err, stderr);
    fprintf(stdout, "Normalized:\n");
    pddlDatalogToNormalForm(dl, &err);
    pddlDatalogCanonicalModel(dl, &err);
    pddlDatalogPrint(dl, stdout);

    pddlDatalogFactsFromCanonicalModel(dl, P, printFact, NULL);
    pddlDatalogFactsFromCanonicalModel(dl, Q, printFact, NULL);
    pddlDatalogFactsFromCanonicalModel(dl, R, printFact, NULL);
    pddlDatalogFactsFromCanonicalModel(dl, S, printFact, NULL);
    pddlDatalogFactsFromCanonicalModel(dl, G, printFact, NULL);

    pddlDatalogExecuteAnnotations(dl, G);
    pddlDatalogDel(dl);
}
