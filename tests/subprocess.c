#include "pddl/pddl.h"
#include "test.h"
#include "context.h"
#include <assert.h>

TEST_EXPLICIT(subprocess_execvp)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddlErrInfoEnable(&err, stderr);

    char *const argv[] = {
        "cat",
        "-n",
        "-E",
        NULL
    };
    char *const argv2[] = {
        "cat",
        NULL
    };
    char *const argverr[] = {
        "ls",
        "-d",
        "/bin",
        "/i/hope/this/path/doesnt/exist",
        NULL
    };

    int ret = pddlExecvp(argv,
                         NULL,
                         "line 1\nline 2\n", 14,
                         NULL, NULL,
                         NULL, NULL,
                         &err);
    assert(ret == 0);

    pddl_exec_status_t st;
    ret = pddlExecvp(argv,
                     &st,
                     "line 1\nline 2\n", 14,
                     NULL, NULL,
                     NULL, NULL,
                     &err);
    assert(ret == 0);
    assert(st.exited);
    assert(st.exit_status == 0);
    assert(!st.signaled);

    char *bufout;
    int bufout_size;
    ret = pddlExecvp(argv2,
                     &st,
                     "line 1\nline 2\n", 14,
                     &bufout, &bufout_size,
                     NULL, NULL,
                     &err);
    assert(ret == 0);
    assert(st.exited);
    assert(st.exit_status == 0);
    assert(!st.signaled);
    assert(bufout != NULL);
    fprintf(stdout, "OUT[cat]\n%s", bufout);
    assert(bufout_size == 14);
    PDDL_FREE(bufout);

    ret = pddlExecvp(argv,
                     &st,
                     "line 1\nline 2\n", 14,
                     &bufout, &bufout_size,
                     NULL, NULL,
                     &err);
    assert(ret == 0);
    assert(st.exited);
    assert(st.exit_status == 0);
    assert(!st.signaled);
    assert(bufout != NULL);
    fprintf(stdout, "\nOUT[cat]\n%s", bufout);
    PDDL_FREE(bufout);

    char *buferr;
    int buferr_size;
    ret = pddlExecvp(argverr,
                     &st,
                     NULL, 0,
                     &bufout, &bufout_size,
                     &buferr, &buferr_size,
                     &err);
    assert(ret == 0);
    assert(st.exited);
    assert(st.exit_status != 0);
    assert(!st.signaled);
    assert(bufout != NULL);
    assert(buferr != NULL);
    fprintf(stdout, "\nOUT[ls]\n%s", bufout);
    fprintf(stdout, "\nERR[ls]\n%s", buferr);
    PDDL_FREE(bufout);
    PDDL_FREE(buferr);
}
