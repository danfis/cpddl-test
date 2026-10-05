/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the FDR state packer (pddl/fdr_state_packer.h): widths of
 * variables, the FFD assignment of variables to words and their placement
 * within words, the optimal (ILP) layout, and pack/unpack round trips.
 *
 * Run with:  cd tests && make && ./test -a -Q -s fdr_state_packer
 */

#include "pddl/fdr_state_packer.h"
#include "pddl/lp.h"
#include "pddl/rand.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Appends a variable with VAL_SIZE values to VARS. Only .var_size and
 * .var[i].val_size are set, because the packer reads nothing else; this
 * allows variables with a single value and with up to INT_MAX values, which
 * pddlFDRVarsAdd() does not (it requires at least two values and allocates
 * every value). */
static void varsAdd(pddl_fdr_vars_t *vars, int val_size)
{
    assert(val_size >= 1);
    vars->var = realloc(vars->var, sizeof(*vars->var) * (vars->var_size + 1));
    pddl_fdr_var_t *var = vars->var + vars->var_size;
    memset(var, 0, sizeof(*var));
    var->var_id = vars->var_size;
    var->val_size = val_size;
    var->val_none_of_those = -1;
    ++vars->var_size;
}

/* Initializes VARS with N variables of VAL_SIZE[i] values (see varsAdd()). */
static void varsInit(pddl_fdr_vars_t *vars, const int *val_size, int n)
{
    memset(vars, 0, sizeof(*vars));
    for (int i = 0; i < n; ++i)
        varsAdd(vars, val_size[i]);
}

/* Frees VARS created by varsInit() or randVars(). */
static void varsFree(pddl_fdr_vars_t *vars)
{
    free(vars->var);
}

/* Initializes P over VARS with the given LAYOUT method. */
static void packerInit(pddl_fdr_state_packer_t *p,
                       const pddl_fdr_vars_t *vars,
                       pddl_fdr_state_packer_layout_t layout)
{
    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    cfg.vars = vars;
    cfg.layout = layout;
    int ret = pddlFDRStatePackerInit(p, &cfg, NULL);
    assert(ret == 0);
}

/* Number of words of the packed state of P. */
static int numWords(const pddl_fdr_state_packer_t *p)
{
    int size = pddlFDRStatePackerBufSize(p);
    assert(size % sizeof(pddl_fdr_packer_word_t) == 0);
    return size / sizeof(pddl_fdr_packer_word_t);
}

/* Expected width of a variable with VAL_SIZE values computed independently
 * of the packer: the smallest w >= 1 such that 2^w >= VAL_SIZE. */
static int expWidth(int val_size)
{
    int w = 1;
    while ((1ull << w) < (unsigned long long)val_size)
        ++w;
    return w;
}

/* Number of values of a variable of the width W, 1 <= W <= 31. */
static int valSizeOfWidth(int w)
{
    assert(w >= 1 && w <= 31);
    if (w == 31)
        return INT_MAX;
    return 1 << w;
}

/* Sum of expected widths of all variables of VARS. */
static int sumBits(const pddl_fdr_vars_t *vars)
{
    int sum = 0;
    for (int i = 0; i < vars->var_size; ++i)
        sum += expWidth(vars->var[i].val_size);
    return sum;
}

/* Asserts that the number of words of P is between the lower bound
 * ceil(sum of widths / B) and the upper bound of any-fit bin packing
 * algorithms (at most one word is at most half full), and at most the
 * number of variables. */
static void assertNumWordsBounds(const pddl_fdr_state_packer_t *p,
                                 const pddl_fdr_vars_t *vars)
{
    const int B = PDDL_FDR_PACKER_WORD_BITS;
    int bits = sumBits(vars);
    int words = numWords(p);
    assert(words >= (bits + B - 1) / B);
    assert(words * B <= 2 * bits + B);
    assert(words <= vars->var_size);
}

/* Asserts that the variable VAR of P is stored in the word WORD with the
 * shift SHIFT: the state with all zeros except VAR = VAL_SIZE - 1 must be
 * packed into all-zero words except WORD == (VAL_SIZE - 1) << SHIFT, and
 * it must be unpacked back. */
static void assertVarAt(const pddl_fdr_state_packer_t *p,
                        const pddl_fdr_vars_t *vars,
                        int var, int word, int shift)
{
    int num_words = numWords(p);
    assert(word >= 0 && word < num_words);
    assert(shift >= 0 && shift < (int)PDDL_FDR_PACKER_WORD_BITS);

    int *state = calloc(vars->var_size, sizeof(int));
    int *state2 = calloc(vars->var_size, sizeof(int));
    pddl_fdr_packer_word_t *buf = malloc(num_words * sizeof(*buf));
    state[var] = vars->var[var].val_size - 1;
    pddlFDRStatePackerPack(p, state, buf);
    for (int w = 0; w < num_words; ++w){
        pddl_fdr_packer_word_t exp = 0;
        if (w == word)
            exp = ((pddl_fdr_packer_word_t)state[var]) << shift;
        assert(buf[w] == exp);
    }

    pddlFDRStatePackerUnpack(p, buf, state2);
    assert(memcmp(state, state2, sizeof(int) * vars->var_size) == 0);

    free(buf);
    free(state);
    free(state2);
}

/* Asserts that the packer P round-trips STATE, and that packing into a
 * garbage-filled buffer gives the same bytes as into a zeroed buffer, i.e.,
 * all bits not occupied by variables are zeroed. */
static void assertRoundTrip(const pddl_fdr_state_packer_t *p,
                            const int *state)
{
    int size = pddlFDRStatePackerBufSize(p);
    char *buf = calloc(size + 1, 1);
    char *buf2 = malloc(size + 1);
    memset(buf2, 0xff, size + 1);
    pddlFDRStatePackerPack(p, state, buf);
    pddlFDRStatePackerPack(p, state, buf2);
    assert(memcmp(buf, buf2, size) == 0);
    // Nothing is written past the end of the buffer
    assert(buf[size] == 0);
    assert((unsigned char)buf2[size] == 0xffu);

    int *state2 = calloc(p->num_vars + 1, sizeof(int));
    pddlFDRStatePackerUnpack(p, buf, state2);
    assert(memcmp(state, state2, sizeof(int) * p->num_vars) == 0);

    free(buf);
    free(buf2);
    free(state2);
}

/* Fills STATE with uniformly random values of VARS. */
static void randState(const pddl_fdr_vars_t *vars, pddl_rand_t *rnd,
                      int *state)
{
    for (int i = 0; i < vars->var_size; ++i)
        state[i] = pddlRandInt(rnd) % (uint32_t)vars->var[i].val_size;
}

/* Initializes VARS with a random number of variables with random numbers of
 * values of mixed magnitudes, including the extremes 1 and INT_MAX (see
 * varsAdd()). */
static void randVars(pddl_fdr_vars_t *vars, pddl_rand_t *rnd)
{
    memset(vars, 0, sizeof(*vars));
    int n = 1 + pddlRandInt(rnd) % 64;
    for (int i = 0; i < n; ++i){
        int val_size;
        uint32_t r = pddlRandInt(rnd) % 20;
        if (r == 0){
            val_size = 1;
        }else if (r == 1){
            val_size = INT_MAX;
        }else{
            int k = pddlRandInt(rnd) % 31;
            val_size = 1 + pddlRandInt(rnd) % (1u << k);
        }
        varsAdd(vars, val_size);
    }
}

/*
 * The default configuration has no variables and the FFD layout.
 */
TEST_ONCE(fdr_state_packer_config_init)
{
    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    assert(cfg.vars == NULL);
    assert(cfg.layout == PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
}

/*
 * A single variable with N values occupies ceil(log2(N)) bits (at least 1)
 * at the top of the first word: checked for N = 2, 3, 4, 5, 2^k, 2^k + 1,
 * and INT_MAX (31 bits). Variables with a single value take 1 bit each, so
 * 32 of them fit in one word and 33 need two.
 */
TEST_ONCE(fdr_state_packer_bit_width)
{
    static const struct {
        int val_size;
        int width;
    } cases[] = {
        { 2, 1 },
        { 3, 2 },
        { 4, 2 },
        { 5, 3 },
        { 8, 3 },
        { 9, 4 },
        { 256, 8 },
        { 257, 9 },
        { 65536, 16 },
        { 65537, 17 },
        { 1 << 30, 30 },
        { (1 << 30) + 1, 31 },
        { INT_MAX, 31 },
    };
    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int ci = 0; ci < num_cases; ++ci){
        assert(expWidth(cases[ci].val_size) == cases[ci].width);
        pddl_fdr_vars_t vars;
        varsInit(&vars, &cases[ci].val_size, 1);
        pddl_fdr_state_packer_t p;
        packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        assert(numWords(&p) == 1);
        assertVarAt(&p, &vars, 0, 0,
                    PDDL_FDR_PACKER_WORD_BITS - cases[ci].width);
        pddlFDRStatePackerFree(&p);
        varsFree(&vars);
    }

    // Variables with a single value take one bit each
    int ones[33];
    for (int i = 0; i < 33; ++i)
        ones[i] = 1;
    for (int n = 1; n <= 33; ++n){
        pddl_fdr_vars_t vars;
        varsInit(&vars, ones, n);
        pddl_fdr_state_packer_t p;
        packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        assert(numWords(&p) == (n <= 32 ? 1 : 2));
        int *state = calloc(n, sizeof(int));
        assertRoundTrip(&p, state);
        free(state);
        pddlFDRStatePackerFree(&p);
        varsFree(&vars);
    }
}

/* Hand-made instance of the FFD layout test */
struct ffd_case {
    /** Number of variables */
    int n;
    /** Width of each variable */
    int width[8];
    /** Expected word of each variable */
    int word[8];
    /** Expected shift of each variable */
    int shift[8];
    /** Expected number of words */
    int num_words;
};
typedef struct ffd_case ffd_case_t;

/*
 * FFD on hand-made instances with known results; the exact word and shift
 * of every variable is checked:
 *   - widths {16,16,16,16}  -> 2 words
 *   - widths {17,17,17}     -> 3 words
 *   - widths {20,12,20,12}  -> 2 words, each 20 + 12
 *   - widths {8,30,2,24}    -> word 0: 30 + 2, word 1: 24 + 8
 *   - widths {1,31,1,31,30} -> word 0: 31 + 1, word 1: 31 + 1, word 2: 30
 *     (ties broken by the variable ID)
 *   - widths {5,5,5,5,5,5,5} -> word 0: 6 x 5, word 1: 5
 */
TEST_ONCE(fdr_state_packer_ffd_layout)
{
    static const ffd_case_t cases[] = {
        { 4, { 16, 16, 16, 16 }, { 0, 0, 1, 1 }, { 16, 0, 16, 0 }, 2 },
        { 3, { 17, 17, 17 }, { 0, 1, 2 }, { 15, 15, 15 }, 3 },
        { 4, { 20, 12, 20, 12 }, { 0, 0, 1, 1 }, { 12, 0, 12, 0 }, 2 },
        { 4, { 8, 30, 2, 24 }, { 1, 0, 0, 1 }, { 0, 2, 0, 8 }, 2 },
        { 5, { 1, 31, 1, 31, 30 }, { 0, 0, 1, 1, 2 }, { 0, 1, 0, 1, 2 }, 3 },
        { 7, { 5, 5, 5, 5, 5, 5, 5 }, { 0, 0, 0, 0, 0, 0, 1 },
          { 27, 22, 17, 12, 7, 2, 27 }, 2 },
    };
    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int ci = 0; ci < num_cases; ++ci){
        const ffd_case_t *c = cases + ci;
        int val_size[8];
        for (int i = 0; i < c->n; ++i)
            val_size[i] = valSizeOfWidth(c->width[i]);

        pddl_fdr_vars_t vars;
        varsInit(&vars, val_size, c->n);
        pddl_fdr_state_packer_t p;
        packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        assert(numWords(&p) == c->num_words);
        for (int i = 0; i < c->n; ++i)
            assertVarAt(&p, &vars, i, c->word[i], c->shift[i]);
        pddlFDRStatePackerFree(&p);
        varsFree(&vars);
    }
}

/*
 * Random sets of variables (fixed seed) with mixed widths: the number of
 * words is at least ceil(sum of widths / 32), at most the number of
 * variables, and at most one word is at most half full; random states
 * round-trip through pack/unpack with all unused bits zeroed.
 */
TEST_ONCE(fdr_state_packer_random)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 2026);
    for (int iter = 0; iter < 500; ++iter){
        pddl_fdr_vars_t vars;
        randVars(&vars, &rnd);
        pddl_fdr_state_packer_t p;
        packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        assert(p.num_vars == vars.var_size);
        assertNumWordsBounds(&p, &vars);

        int *state = calloc(vars.var_size, sizeof(int));
        assertRoundTrip(&p, state);
        for (int i = 0; i < vars.var_size; ++i)
            state[i] = vars.var[i].val_size - 1;
        assertRoundTrip(&p, state);
        for (int si = 0; si < 20; ++si){
            randState(&vars, &rnd, state);
            assertRoundTrip(&p, state);
        }
        free(state);

        pddlFDRStatePackerFree(&p);
        varsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * pddlFDRStatePackerInitCopy() creates a packer with the same buffer size
 * that packs states into the same bytes.
 */
TEST_ONCE(fdr_state_packer_copy)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 7);
    for (int iter = 0; iter < 50; ++iter){
        pddl_fdr_vars_t vars;
        randVars(&vars, &rnd);
        pddl_fdr_state_packer_t p, p2;
        packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        pddlFDRStatePackerInitCopy(&p2, &p);
        assert(p2.num_vars == p.num_vars);
        int size = pddlFDRStatePackerBufSize(&p);
        assert(pddlFDRStatePackerBufSize(&p2) == size);

        int *state = calloc(vars.var_size, sizeof(int));
        int *state2 = calloc(vars.var_size, sizeof(int));
        char *buf = malloc(size);
        char *buf2 = malloc(size);
        for (int si = 0; si < 20; ++si){
            randState(&vars, &rnd, state);
            pddlFDRStatePackerPack(&p, state, buf);
            pddlFDRStatePackerPack(&p2, state, buf2);
            assert(memcmp(buf, buf2, size) == 0);
            pddlFDRStatePackerUnpack(&p2, buf, state2);
            assert(memcmp(state, state2, sizeof(int) * vars.var_size) == 0);
        }
        free(buf);
        free(buf2);
        free(state);
        free(state2);

        pddlFDRStatePackerFree(&p);
        pddlFDRStatePackerFree(&p2);
        varsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * A packer over no variables has a zero-sized buffer.
 */
TEST_ONCE(fdr_state_packer_no_vars)
{
    pddl_fdr_vars_t vars;
    varsInit(&vars, NULL, 0);
    pddl_fdr_state_packer_t p;
    packerInit(&p, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    assert(p.num_vars == 0);
    assert(pddlFDRStatePackerBufSize(&p) == 0);
    pddlFDRStatePackerFree(&p);
    varsFree(&vars);
}

/*
 * Initializing a packer from a config without variables fails with an
 * error.
 */
TEST_ONCE(fdr_state_packer_err_no_vars_in_config)
{
    pddl_err_t err;
    pddlErrInit(&err);
    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    pddl_fdr_state_packer_t p;
    int ret = pddlFDRStatePackerInit(&p, &cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
}

/*
 * Initializing a packer with an unknown layout method fails with an error
 * and allocates nothing.
 */
TEST_ONCE(fdr_state_packer_err_unknown_layout)
{
    pddl_err_t err;
    pddlErrInit(&err);
    pddl_fdr_vars_t vars;
    int val_size = 2;
    varsInit(&vars, &val_size, 1);
    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    cfg.vars = &vars;
    cfg.layout = (pddl_fdr_state_packer_layout_t)-100;
    pddl_fdr_state_packer_t p;
    int ret = pddlFDRStatePackerInit(&p, &cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
    varsFree(&vars);
}

/*
 * The packer over the variables of the task round-trips the initial state
 * and random states, and its number of words is within the bounds.
 */
TEST(fdr_state_packer, fdr)
{
    const pddl_fdr_vars_t *vars = &C.fdr.var;
    pddl_fdr_state_packer_t p;
    packerInit(&p, vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    assert(p.num_vars == vars->var_size);
    assertNumWordsBounds(&p, vars);

    assertRoundTrip(&p, C.fdr.init);
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1);
    int *state = calloc(vars->var_size + 1, sizeof(int));
    for (int si = 0; si < 1000; ++si){
        randState(vars, &rnd, state);
        assertRoundTrip(&p, state);
    }
    free(state);
    pddlRandFree(&rnd);

    pddlFDRStatePackerFree(&p);
}

/* Recursive step of optWords(): places the I-th of the N widths W (sorted
 * in decreasing order) into one of the NUM words with loads LOAD or into a
 * new word, and updates BEST with the least number of words found. */
static void optWordsRec(const int *w, int n, int i, int *load, int num,
                        int *best)
{
    if (num >= *best)
        return;
    if (i == n){
        *best = num;
        return;
    }
    for (int b = 0; b < num; ++b){
        if (load[b] + w[i] <= (int)PDDL_FDR_PACKER_WORD_BITS){
            load[b] += w[i];
            optWordsRec(w, n, i + 1, load, num, best);
            load[b] -= w[i];
        }
    }
    load[num] = w[i];
    optWordsRec(w, n, i + 1, load, num + 1, best);
}

/* Optimal number of words of VARS computed by an exhaustive search
 * independently of the packer (use only for a few variables). */
static int optWords(const pddl_fdr_vars_t *vars)
{
    int n = vars->var_size;
    if (n == 0)
        return 0;
    int *w = calloc(n, sizeof(int));
    int *load = calloc(n, sizeof(int));
    for (int i = 0; i < n; ++i)
        w[i] = expWidth(vars->var[i].val_size);
    // Insertion sort in decreasing order
    for (int i = 1; i < n; ++i){
        for (int j = i; j > 0 && w[j - 1] < w[j]; --j){
            int tmp = w[j];
            w[j] = w[j - 1];
            w[j - 1] = tmp;
        }
    }
    int best = n + 1;
    optWordsRec(w, n, 0, load, 0, &best);
    free(w);
    free(load);
    return best;
}

/* Asserts that P round-trips the state with the maximal values of all
 * variables and NUM random states of VARS. */
static void assertRoundTripRand(const pddl_fdr_state_packer_t *p,
                                const pddl_fdr_vars_t *vars,
                                pddl_rand_t *rnd,
                                int num)
{
    int *state = calloc(vars->var_size + 1, sizeof(int));
    for (int i = 0; i < vars->var_size; ++i)
        state[i] = vars->var[i].val_size - 1;
    assertRoundTrip(p, state);
    for (int si = 0; si < num; ++si){
        randState(vars, rnd, state);
        assertRoundTrip(p, state);
    }
    free(state);
}

/* Asserts that the packers P1 and P2 over VARS pack the NUM random states
 * into the same bytes, i.e., they have the same layout. */
static void assertSameLayout(const pddl_fdr_state_packer_t *p1,
                             const pddl_fdr_state_packer_t *p2,
                             const pddl_fdr_vars_t *vars,
                             pddl_rand_t *rnd,
                             int num)
{
    int size = pddlFDRStatePackerBufSize(p1);
    assert(pddlFDRStatePackerBufSize(p2) == size);
    int *state = calloc(vars->var_size + 1, sizeof(int));
    char *buf1 = malloc(size + 1);
    char *buf2 = malloc(size + 1);
    for (int si = 0; si < num; ++si){
        randState(vars, rnd, state);
        pddlFDRStatePackerPack(p1, state, buf1);
        pddlFDRStatePackerPack(p2, state, buf2);
        assert(memcmp(buf1, buf2, size) == 0);
    }
    free(buf1);
    free(buf2);
    free(state);
}

/* Checks the ILP layout of VARS: it round-trips states, it has the least
 * number of words if OPT >= 0, it never has more words than FFD, and it is
 * the FFD layout if it has the same number of words. Returns the number of
 * words of the ILP layout. */
static int checkILP(const pddl_fdr_vars_t *vars, int opt, pddl_rand_t *rnd)
{
    pddl_fdr_state_packer_t ffd, ilp;
    packerInit(&ffd, vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    packerInit(&ilp, vars, PDDL_FDR_STATE_PACKER_LAYOUT_ILP);
    assert(ilp.num_vars == vars->var_size);
    assertNumWordsBounds(&ilp, vars);
    int words = numWords(&ilp);
    assert(words <= numWords(&ffd));
    if (opt >= 0)
        assert(words == opt);
    if (words == numWords(&ffd))
        assertSameLayout(&ffd, &ilp, vars, rnd, 20);
    assertRoundTripRand(&ilp, vars, rnd, 20);
    pddlFDRStatePackerFree(&ffd);
    pddlFDRStatePackerFree(&ilp);
    return words;
}

/*
 * The default configuration has no time limit for the ILP.
 */
TEST_ONCE(fdr_state_packer_config_init_ilp)
{
    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    assert(cfg.ilp_time_limit <= 0.f);
}

/*
 * Instances on which FFD needs one word more than the optimum, e.g.,
 * widths {15,13,12,10,7,5}: FFD needs 3 words, but 2 words suffice
 * ({15,12,5} and {13,10,7}). The ILP layout finds the optimum. Skipped
 * without an LP solver.
 */
TEST_ONCE(fdr_state_packer_ilp_beats_ffd)
{
    if (!pddlLPSolverAvailable(PDDL_LP_DEFAULT))
        return;

    static const struct {
        int n;
        int width[8];
        int opt;
    } cases[] = {
        { 6, { 15, 13, 12, 10, 7, 5 }, 2 },
        { 6, { 16, 13, 12, 10, 7, 6 }, 2 },
        { 6, { 5, 16, 9, 19, 7, 5 }, 2 },
        { 8, { 16, 16, 13, 13, 10, 9, 7, 7 }, 3 },
        { 8, { 6, 18, 15, 14, 12, 11, 10, 8 }, 3 },
    };
    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 15);
    for (int ci = 0; ci < num_cases; ++ci){
        int val_size[8];
        for (int i = 0; i < cases[ci].n; ++i)
            val_size[i] = valSizeOfWidth(cases[ci].width[i]);
        pddl_fdr_vars_t vars;
        varsInit(&vars, val_size, cases[ci].n);
        assert(optWords(&vars) == cases[ci].opt);

        pddl_fdr_state_packer_t ffd;
        packerInit(&ffd, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        assert(numWords(&ffd) == cases[ci].opt + 1);
        pddlFDRStatePackerFree(&ffd);

        checkILP(&vars, cases[ci].opt, &rnd);
        varsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * Random small sets of variables (fixed seed): the ILP layout has the
 * optimal number of words computed by an exhaustive search, it never has
 * more words than FFD, it is the FFD layout if FFD is optimal, and it
 * round-trips states. Larger random sets (up to 64 variables) are checked
 * the same way, but without the exhaustive search. Skipped without an LP
 * solver.
 */
TEST_ONCE(fdr_state_packer_ilp_random)
{
    if (!pddlLPSolverAvailable(PDDL_LP_DEFAULT))
        return;

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 2027);
    int num_better = 0;
    for (int iter = 0; iter < 300; ++iter){
        pddl_fdr_vars_t vars;
        memset(&vars, 0, sizeof(vars));
        int n = 1 + pddlRandInt(&rnd) % 9;
        for (int i = 0; i < n; ++i){
            // Widths 5..20 make FFD suboptimal more often
            int w;
            if (iter % 2 == 0){
                w = 5 + pddlRandInt(&rnd) % 16;
            }else{
                w = 1 + pddlRandInt(&rnd) % 31;
            }
            varsAdd(&vars, valSizeOfWidth(w));
        }

        pddl_fdr_state_packer_t ffd;
        packerInit(&ffd, &vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
        int opt = optWords(&vars);
        if (opt < numWords(&ffd))
            ++num_better;
        pddlFDRStatePackerFree(&ffd);

        checkILP(&vars, opt, &rnd);
        varsFree(&vars);
    }
    // Make sure the ILP was really needed in some of the instances
    assert(num_better > 0);

    for (int iter = 0; iter < 30; ++iter){
        pddl_fdr_vars_t vars;
        randVars(&vars, &rnd);
        checkILP(&vars, -1, &rnd);
        varsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * The ILP layout of the variables of the task never has more words than
 * the FFD layout, and it round-trips the initial state and random states.
 */
TEST_COND(fdr_state_packer_ilp, fdr, LP)
{
    const pddl_fdr_vars_t *vars = &C.fdr.var;
    pddl_fdr_state_packer_t ffd, ilp;
    packerInit(&ffd, vars, PDDL_FDR_STATE_PACKER_LAYOUT_FFD);

    pddl_fdr_state_packer_config_t cfg = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    cfg.vars = vars;
    cfg.layout = PDDL_FDR_STATE_PACKER_LAYOUT_ILP;
    cfg.ilp_time_limit = 30.f;
    int ret = pddlFDRStatePackerInit(&ilp, &cfg, &C.err);
    assert(ret == 0);
    assertNumWordsBounds(&ilp, vars);
    assert(numWords(&ilp) <= numWords(&ffd));

    assertRoundTrip(&ilp, C.fdr.init);
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1);
    assertRoundTripRand(&ilp, vars, &rnd, 1000);
    pddlRandFree(&rnd);

    pddlFDRStatePackerFree(&ffd);
    pddlFDRStatePackerFree(&ilp);
}
