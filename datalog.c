#include <assert.h>
#include "test.h"
#include "context.h"

TEST_EXPLICIT(datalog)
{
    pddl_datalog_t *dl;

    dl = pddlDatalogNew();
    unsigned P = pddlDatalogAddPred(dl, 2, "P");
    unsigned Q = pddlDatalogAddPred(dl, 2, "Q");
    unsigned R = pddlDatalogAddPred(dl, 3, "R");
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

    bor_err_t err = BOR_ERR_INIT;
    borErrInfoEnable(&err, stderr);
    fprintf(stdout, "Normalized:\n");
    pddlDatalogToNormalForm(dl, &err);

    pddlDatalogCanonicalModel(dl, &err);
    pddlDatalogPrint(dl, stdout);
    pddlDatalogDel(dl);
}
