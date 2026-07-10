/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/opts.h"
#include "test.h"
#include <assert.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_init_free)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    assert(!opts.err);
    assert(opts.parse_seq_size == 0);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Bool options
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_bool_long)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    pddlOptsBool(&opts, "flag", 0, &flag, "A flag");

    char arg0[] = "prog";
    char arg1[] = "--flag";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(flag == pddl_true);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_bool_no_prefix)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_true;
    pddlOptsBool(&opts, "flag", 0, &flag, "A flag");

    char arg0[] = "prog";
    char arg1[] = "--no-flag";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(flag == pddl_false);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_bool_short)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    pddlOptsBool(&opts, "flag", 'f', &flag, "A flag");

    char arg0[] = "prog";
    char arg1[] = "-f";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(flag == pddl_true);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_bool_not_set_keeps_default)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    pddlOptsBool(&opts, "flag", 0, &flag, "A flag");

    char arg0[] = "prog";
    char *argv[] = {arg0, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 1, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    /* default value must remain unchanged */
    assert(flag == pddl_false);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Numeric options
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_int)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 0, &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "42";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 42);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_int_negative)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 0, &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "-7";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == -7);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_int_short)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 'n', &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "-n";
    char arg2[] = "99";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 99);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_uint32)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    uint32_t val = 0;
    pddlOptsUInt32(&opts, "val", 0, &val, "A uint32");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "100";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 100u);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_sizet)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    size_t val = 0;
    pddlOptsSizeT(&opts, "val", 0, &val, "A size_t");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "1024";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == (size_t)1024);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_flt)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    float val = 0.0f;
    pddlOptsFlt(&opts, "val", 0, &val, "A float");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "3.5";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val > 3.4f && val < 3.6f);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_dbl)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    double val = 0.0;
    pddlOptsDbl(&opts, "val", 0, &val, "A double");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "2.5";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val > 2.4 && val < 2.6);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * String option
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_str)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char *val = NULL;
    pddlOptsStr(&opts, "val", 0, &val, "A string");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "hello";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val != NULL);
    assert(strcmp(val, "hello") == 0);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Enum option
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_enum)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsEnum(&opts, "mode", 0, &val, "Mode",
                 "fast", 1, "Fast mode",
                 "slow", 2, "Slow mode",
                 NULL);

    /* Select the second value */
    char arg0[] = "prog";
    char arg1[] = "--mode";
    char arg2[] = "slow";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 2);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_enum_first_value)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsEnum(&opts, "mode", 0, &val, "Mode",
                 "fast", 10, "Fast mode",
                 "slow", 20, "Slow mode",
                 NULL);

    /* Select the first value */
    char arg0[] = "prog";
    char arg1[] = "--mode";
    char arg2[] = "fast";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 10);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Multiple options in one parse call
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_multiple_opts)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    int count = 0;
    char *name = NULL;
    pddlOptsBool(&opts, "verbose", 'v', &verbose, "Verbose");
    pddlOptsInt(&opts, "count", 'c', &count, "Count");
    pddlOptsStr(&opts, "name", 'n', &name, "Name");

    char arg0[] = "prog";
    char arg1[] = "--verbose";
    char arg2[] = "--count";
    char arg3[] = "5";
    char arg4[] = "--name";
    char arg5[] = "world";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 6, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(verbose == pddl_true);
    assert(count == 5);
    assert(name != NULL && strcmp(name, "world") == 0);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Required positional arguments
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_req_bool)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t val = pddl_false;
    pddlOptsReqBool(&opts, "mybool", &val, "A required bool");

    char arg0[] = "prog";
    char arg1[] = "true";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == pddl_true);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_bool_false)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t val = pddl_true;
    pddlOptsReqBool(&opts, "mybool", &val, "A required bool");

    char arg0[] = "prog";
    char arg1[] = "false";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == pddl_false);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_int)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsReqInt(&opts, "myint", &val, "A required int");

    char arg0[] = "prog";
    char arg1[] = "77";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 77);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_flt)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    float val = 0.0f;
    pddlOptsReqFlt(&opts, "myflt", &val, "A required float");

    char arg0[] = "prog";
    char arg1[] = "1.5";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val > 1.4f && val < 1.6f);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_str)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char *val = NULL;
    pddlOptsReqStr(&opts, "mystr", &val, "A required string");

    char arg0[] = "prog";
    char arg1[] = "hello";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val != NULL);
    assert(strcmp(val, "hello") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_enum)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsReqEnum(&opts, "myenum", &val, "A required enum",
                    "a", 1, "value a",
                    "b", 2, "value b",
                    NULL);

    char arg0[] = "prog";
    char arg1[] = "b";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val == 2);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_str_arr)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char **arr = NULL;
    int arr_size = 0;
    pddlOptsReqStrArr(&opts, "files", 1, 4, &arr, &arr_size, "Files");

    char arg0[] = "prog";
    char arg1[] = "a.txt";
    char arg2[] = "b.txt";
    char arg3[] = "c.txt";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(arr_size == 3);
    assert(arr != NULL);
    assert(strcmp(arr[0], "a.txt") == 0);
    assert(strcmp(arr[1], "b.txt") == 0);
    assert(strcmp(arr[2], "c.txt") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_str_arr_single)
{
    /* Minimum constraint satisfied with a single arg. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char **arr = NULL;
    int arr_size = 0;
    pddlOptsReqStrArr(&opts, "files", 1, 3, &arr, &arr_size, "Files");

    char arg0[] = "prog";
    char arg1[] = "only.txt";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(arr_size == 1);
    assert(arr != NULL);
    assert(strcmp(arr[0], "only.txt") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_req_str_remainder)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char **arr = NULL;
    int arr_size = 0;
    pddlOptsReqStrRemainder(&opts, "args", &arr, &arr_size, "All args");

    char arg0[] = "prog";
    char arg1[] = "x";
    char arg2[] = "y";
    char arg3[] = "z";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(arr_size == 3);
    assert(strcmp(arr[0], "x") == 0);
    assert(strcmp(arr[1], "y") == 0);
    assert(strcmp(arr[2], "z") == 0);
    pddlOptsFree(&opts);
}

/* Mixed: optional flags before required positional args */
TEST_ONCE(opts_mixed_opt_and_req)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    pddlOptsBool(&opts, "verbose", 0, &verbose, "Verbose");
    char *target = NULL;
    pddlOptsReqStr(&opts, "target", &target, "Target");

    char arg0[] = "prog";
    char arg1[] = "--verbose";
    char arg2[] = "myfile";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(verbose == pddl_true);
    assert(target != NULL && strcmp(target, "myfile") == 0);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Multi-val options
 * ----------------------------------------------------------------------- */

static int g_multi_int_vals[16];
static int g_multi_int_count;

static void multiIntCb(pddl_opts_t *o, int val, void *data)
{
    (void)o;
    int *arr = (int *)data;
    arr[g_multi_int_count++] = val;
}

TEST_ONCE(opts_multi_int)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    g_multi_int_count = 0;
    pddlOptsMultiInt(&opts, "val", 0, multiIntCb, g_multi_int_vals, "Multi int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "1";
    char arg3[] = "--val";
    char arg4[] = "2";
    char arg5[] = "--val";
    char arg6[] = "3";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 7, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(g_multi_int_count == 3);
    assert(g_multi_int_vals[0] == 1);
    assert(g_multi_int_vals[1] == 2);
    assert(g_multi_int_vals[2] == 3);
    pddlOptsFree(&opts);
}

static pddl_bool_t g_multi_bool_vals[16];
static int g_multi_bool_count;

static void multiBoolCb(pddl_opts_t *o, pddl_bool_t val, void *data)
{
    (void)o;
    pddl_bool_t *arr = (pddl_bool_t *)data;
    arr[g_multi_bool_count++] = val;
}

TEST_ONCE(opts_multi_bool)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    g_multi_bool_count = 0;
    pddlOptsMultiBool(&opts, "flag", 0, multiBoolCb, g_multi_bool_vals,
                      "Multi bool");

    /* --flag --no-flag --flag: true, false, true */
    char arg0[] = "prog";
    char arg1[] = "--flag";
    char arg2[] = "--no-flag";
    char arg3[] = "--flag";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(g_multi_bool_count == 3);
    assert(g_multi_bool_vals[0] == pddl_true);
    assert(g_multi_bool_vals[1] == pddl_false);
    assert(g_multi_bool_vals[2] == pddl_true);
    pddlOptsFree(&opts);
}

static int g_multi_enum_vals[16];
static int g_multi_enum_count;

static void multiEnumCb(pddl_opts_t *o, int val, void *data)
{
    (void)o;
    int *arr = (int *)data;
    arr[g_multi_enum_count++] = val;
}

TEST_ONCE(opts_multi_enum)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    g_multi_enum_count = 0;
    pddlOptsMultiEnum(&opts, "mode", 0, multiEnumCb, g_multi_enum_vals,
                      "Multi enum",
                      "a", 1, "value a",
                      "b", 2, "value b",
                      NULL);

    char arg0[] = "prog";
    char arg1[] = "--mode";
    char arg2[] = "b";
    char arg3[] = "--mode";
    char arg4[] = "a";
    char arg5[] = "--mode";
    char arg6[] = "b";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 7, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(g_multi_enum_count == 3);
    assert(g_multi_enum_vals[0] == 2);
    assert(g_multi_enum_vals[1] == 1);
    assert(g_multi_enum_vals[2] == 2);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Alias option
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_alias)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    int val = 0;
    pddlOptsBool(&opts, "flag", 0, &flag, "A flag");
    pddlOptsInt(&opts, "val", 0, &val, "An int");
    /* "both" expands to "--flag --val 99" */
    pddlOptsAlias(&opts, "both", 0, "--flag", "--val", "99", NULL);

    char arg0[] = "prog";
    char arg1[] = "--both";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(flag == pddl_true);
    assert(val == 99);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Commands
 * ----------------------------------------------------------------------- */

#define CMD_RUN  1
#define CMD_STOP 2

TEST_ONCE(opts_cmd_basic)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    /* Global option visible regardless of which command is selected */
    pddl_bool_t verbose = pddl_false;
    pddlOptsBool(&opts, "verbose", 'v', &verbose, "Verbose mode");

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run something");
    int count = 0;
    pddlOptsInt(&opts, "count", 'c', &count, "Count");

    pddlOptsCmd(&opts, CMD_STOP, "stop", "Stop something");
    char *reason = NULL;
    pddlOptsStr(&opts, "reason", 'r', &reason, "Reason");

    char arg0[] = "prog";
    char arg1[] = "--verbose";
    char arg2[] = "run";
    char arg3[] = "--count";
    char arg4[] = "3";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 5, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    assert(verbose == pddl_true);
    assert(count == 3);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_cmd_second)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run something");
    int count = 0;
    pddlOptsInt(&opts, "count", 0, &count, "Count");

    pddlOptsCmd(&opts, CMD_STOP, "stop", "Stop something");
    char *reason = NULL;
    pddlOptsStr(&opts, "reason", 0, &reason, "Reason");

    char arg0[] = "prog";
    char arg1[] = "stop";
    char arg2[] = "--reason";
    char arg3[] = "done";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_STOP);
    assert(reason != NULL && strcmp(reason, "done") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_cmd_req)
{
    /* Command with a required positional argument */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run something");
    char *target = NULL;
    pddlOptsReqStr(&opts, "target", &target, "Target to run");

    char arg0[] = "prog";
    char arg1[] = "run";
    char arg2[] = "myapp";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    assert(target != NULL && strcmp(target, "myapp") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_cmd_alias_name)
{
    /* Command with an alternative name (alias) */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddlOptsCmd(&opts, CMD_RUN, "run", "Run something");
    pddlOptsCmdAddAlias(&opts, CMD_RUN, "start");

    char arg0[] = "prog";
    char arg1[] = "start";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    pddlOptsFree(&opts);
}

static int g_cmd_callback_called;

static void cmdRunCallback(pddl_opts_t *o, void *data)
{
    (void)o;
    int *called = (int *)data;
    *called = 1;
}

TEST_ONCE(opts_cmd_callback)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    g_cmd_callback_called = 0;
    pddlOptsCmd(&opts, CMD_RUN, "run", "Run something");
    pddlOptsCmdSetCallback(&opts, CMD_RUN, cmdRunCallback, &g_cmd_callback_called);

    char arg0[] = "prog";
    char arg1[] = "run";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    assert(g_cmd_callback_called == 1);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_cmd_disable_opt)
{
    /* A global bool is disabled for CMD_RUN; using it must produce an error. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    int verbid = pddlOptsBool(&opts, "verbose", 0, &verbose, "Verbose");

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run");
    pddlOptsCmdDisableOpt(&opts, CMD_RUN, verbid);

    char arg0[] = "prog";
    char arg1[] = "run";
    char arg2[] = "--verbose";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_cmd_disable_opt_by_dest)
{
    /* pddlOptsCmdDisableOptByDest: disable a global opt by its dst pointer.
     * Parsing without the disabled opt must still succeed. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    pddlOptsBool(&opts, "verbose", 0, &verbose, "Verbose");

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run");
    pddlOptsCmdDisableOptByDest(&opts, CMD_RUN, &verbose);

    /* CMD_RUN without --verbose: must succeed */
    char arg0[] = "prog";
    char arg1[] = "run";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Shared option groups
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_group_shared_run)
{
    /* Named group created before commands is added to both;
     * verify it works when CMD_RUN is selected. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int grp = pddlOptsGroup(&opts, "shared", "Shared options");
    int timeout = 0;
    pddlOptsInt(&opts, "timeout", 0, &timeout, "Timeout");
    pddlOptsGroupEnd(&opts);

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run");
    pddlOptsAddGroup(&opts, grp);
    pddlOptsCmd(&opts, CMD_STOP, "stop", "Stop");
    pddlOptsAddGroup(&opts, grp);

    char arg0[] = "prog";
    char arg1[] = "run";
    char arg2[] = "--timeout";
    char arg3[] = "30";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_RUN);
    assert(timeout == 30);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_group_shared_stop)
{
    /* Same shared group, this time CMD_STOP is selected. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int grp = pddlOptsGroup(&opts, "shared", "Shared options");
    int timeout = 0;
    pddlOptsInt(&opts, "timeout", 0, &timeout, "Timeout");
    pddlOptsGroupEnd(&opts);

    pddlOptsCmd(&opts, CMD_RUN, "run", "Run");
    pddlOptsAddGroup(&opts, grp);
    pddlOptsCmd(&opts, CMD_STOP, "stop", "Stop");
    pddlOptsAddGroup(&opts, grp);

    char arg0[] = "prog";
    char arg1[] = "stop";
    char arg2[] = "--timeout";
    char arg3[] = "60";
    char *argv[] = {arg0, arg1, arg2, arg3, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 4, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(cmd == CMD_STOP);
    assert(timeout == 60);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * String specification (strspec) groups
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_strspec_req)
{
    /* strspec as a required positional argument.
     * Format: "flag,count=5" -> ss_flag=true, ss_val=5 */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int ss_id = pddlOptsStrSpec(&opts);
    pddl_bool_t ss_flag = pddl_false;
    int ss_val = 0;
    pddlOptsBool(&opts, "flag", 0, &ss_flag, "flag sub-opt");
    pddlOptsInt(&opts, "count", 0, &ss_val, "count sub-opt");
    pddlOptsStrSpecEnd(&opts);

    pddlOptsReqStrSpec(&opts, ss_id, "spec", "Spec positional arg");

    char arg0[] = "prog";
    char arg1[] = "flag,count=5";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(ss_flag == pddl_true);
    assert(ss_val == 5);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_strspec_req_partial)
{
    /* Only some sub-opts specified; unspecified ones keep their defaults. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int ss_id = pddlOptsStrSpec(&opts);
    pddl_bool_t ss_flag = pddl_false;
    int ss_val = 99;
    pddlOptsBool(&opts, "flag", 0, &ss_flag, "flag sub-opt");
    pddlOptsInt(&opts, "count", 0, &ss_val, "count sub-opt");
    pddlOptsStrSpecEnd(&opts);

    pddlOptsReqStrSpec(&opts, ss_id, "spec", "Spec positional arg");

    char arg0[] = "prog";
    char arg1[] = "flag";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(ss_flag == pddl_true);
    /* count was not specified; default must remain */
    assert(ss_val == 99);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_strspec_bool_false)
{
    /* Explicit "flag=false" in strspec must set the bool sub-opt to false. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int ss_id = pddlOptsStrSpec(&opts);
    pddl_bool_t ss_flag = pddl_true;
    pddlOptsBool(&opts, "flag", 0, &ss_flag, "flag sub-opt");
    pddlOptsStrSpecEnd(&opts);

    pddlOptsReqStrSpec(&opts, ss_id, "spec", "Spec positional arg");

    char arg0[] = "prog";
    char arg1[] = "flag=false";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(ss_flag == pddl_false);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_strspec_opt)
{
    /* strspec as a regular --option VALUE style option. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int ss_id = pddlOptsStrSpec(&opts);
    pddl_bool_t ss_flag = pddl_false;
    int ss_val = 0;
    pddlOptsBool(&opts, "flag", 0, &ss_flag, "flag sub-opt");
    pddlOptsInt(&opts, "count", 0, &ss_val, "count sub-opt");
    pddlOptsStrSpecEnd(&opts);

    pddlOptsOptStrSpec(&opts, ss_id, "spec", 's', "Spec option");

    char arg0[] = "prog";
    char arg1[] = "--spec";
    char arg2[] = "flag,count=7";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(ss_flag == pddl_true);
    assert(ss_val == 7);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_strspec_opt_short)
{
    /* strspec option addressed by its short name. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);

    int ss_id = pddlOptsStrSpec(&opts);
    int ss_val = 0;
    pddlOptsInt(&opts, "n", 0, &ss_val, "n sub-opt");
    pddlOptsStrSpecEnd(&opts);

    pddlOptsOptStrSpec(&opts, ss_id, "spec", 's', "Spec option");

    char arg0[] = "prog";
    char arg1[] = "-s";
    char arg2[] = "n=12";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(ss_val == 12);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Parse-order sequence (parse_seq)
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_parse_seq_single_val)
{
    /* Single-value options must have val_idx == -1 in parse_seq.
     * parse_seq_size must equal the number of options that were set. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    int val = 0;
    char *str = NULL;
    pddlOptsBool(&opts, "flag", 0, &flag, "flag");
    pddlOptsInt(&opts, "val", 0, &val, "val");
    pddlOptsStr(&opts, "str", 0, &str, "str");

    char arg0[] = "prog";
    char arg1[] = "--flag";
    char arg2[] = "--val";
    char arg3[] = "10";
    char arg4[] = "--str";
    char arg5[] = "hi";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 6, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);

    /* Three options parsed -> three parse_seq entries */
    assert(opts.parse_seq_size == 3);
    for (int i = 0; i < opts.parse_seq_size; ++i)
        assert(opts.parse_seq[i].val_idx == -1);

    /* All entries must point to distinct option descriptors */
    assert(opts.parse_seq[0].opt != opts.parse_seq[1].opt);
    assert(opts.parse_seq[1].opt != opts.parse_seq[2].opt);
    assert(opts.parse_seq[0].opt != opts.parse_seq[2].opt);

    assert(flag == pddl_true);
    assert(val == 10);
    assert(str != NULL && strcmp(str, "hi") == 0);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_parse_seq_multi_val_idx)
{
    /* Multi-val entries must have distinct non-negative val_idx values. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    g_multi_int_count = 0;
    pddlOptsMultiInt(&opts, "n", 0, multiIntCb, g_multi_int_vals, "n");

    char arg0[] = "prog";
    char arg1[] = "--n";
    char arg2[] = "1";
    char arg3[] = "--n";
    char arg4[] = "2";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 5, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(opts.parse_seq_size == 2);
    assert(opts.parse_seq[0].val_idx >= 0);
    assert(opts.parse_seq[1].val_idx >= 0);
    assert(opts.parse_seq[0].val_idx != opts.parse_seq[1].val_idx);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Two-dash separator (--)
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_two_dashes)
{
    /* After "--", tokens that look like options are treated as positionals. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    char *val = NULL;
    pddlOptsReqStr(&opts, "file", &val, "File");

    char arg0[] = "prog";
    char arg1[] = "--";
    char arg2[] = "--not-an-option";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret == 0);
    assert(!opts.err);
    assert(val != NULL);
    assert(strcmp(val, "--not-an-option") == 0);
    pddlOptsFree(&opts);
}

/* -----------------------------------------------------------------------
 * Error handling
 * ----------------------------------------------------------------------- */

TEST_ONCE(opts_err_unknown_opt)
{
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t flag = pddl_false;
    pddlOptsBool(&opts, "flag", 0, &flag, "A flag");

    char arg0[] = "prog";
    char arg1[] = "--unknown";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_missing_req)
{
    /* Required positional arg omitted while other args are present -> error.
     * (argc=1 is a special "show help" case that returns 0 with no checks.) */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    int val = 0;
    pddlOptsBool(&opts, "verbose", 0, &verbose, "Verbose");
    pddlOptsReqInt(&opts, "num", &val, "Required int");

    /* Provide the optional flag but omit the required positional arg */
    char arg0[] = "prog";
    char arg1[] = "--verbose";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_bad_int_val)
{
    /* Non-numeric string for an int option -> error. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 0, &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "notanint";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_bad_enum_val)
{
    /* Unrecognised enum name -> error. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsEnum(&opts, "mode", 0, &val, "Mode",
                 "fast", 1, "Fast mode",
                 "slow", 2, "Slow mode",
                 NULL);

    char arg0[] = "prog";
    char arg1[] = "--mode";
    char arg2[] = "bogus";
    char *argv[] = {arg0, arg1, arg2, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 3, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_missing_cmd)
{
    /* Commands are defined but none is provided; a global flag IS given.
     * (argc=1 is a special "show help" case that returns 0 with no checks.) */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    pddl_bool_t verbose = pddl_false;
    pddlOptsBool(&opts, "verbose", 0, &verbose, "Verbose");
    pddlOptsCmd(&opts, CMD_RUN, "run", "Run");
    pddlOptsCmd(&opts, CMD_STOP, "stop", "Stop");

    /* Global flag is present but no command follows */
    char arg0[] = "prog";
    char arg1[] = "--verbose";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_missing_opt_value)
{
    /* Option that requires a value is the last argument -> error. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 0, &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char *argv[] = {arg0, arg1, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 2, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_opt_set_twice)
{
    /* Single-val option set more than once -> error. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    int val = 0;
    pddlOptsInt(&opts, "val", 0, &val, "An int");

    char arg0[] = "prog";
    char arg1[] = "--val";
    char arg2[] = "1";
    char arg3[] = "--val";
    char arg4[] = "2";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
    int cmd = 0;
    int ret = pddlOptsParse(&opts, 5, argv, &cmd);
    assert(ret != 0);
    assert(opts.err);
    pddlOptsFree(&opts);
}

TEST_ONCE(opts_err_direct)
{
    /* pddlOptsErr() can be called directly (e.g., from a user callback)
     * and must set the error flag. */
    pddl_opts_t opts;
    pddlOptsInit(&opts);
    assert(!opts.err);
    pddlOptsErr(&opts, "test error %d", 42);
    assert(opts.err);
    assert(opts.err_msg != NULL);
    pddlOptsFree(&opts);
}
