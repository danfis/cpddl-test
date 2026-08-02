#ifndef __CPDDL_TEST_H__
#define __CPDDL_TEST_H__

extern const char *TEST_TASK;

extern int __test_skip_children;

/** Call from anywhere within a test function to skip, for the current
 *  task/run, all tests that depend on this one. It does not return -- the
 *  current test continues and completes normally (it is not a failure). */
#define TEST_SKIP_CHILDREN do { __test_skip_children = 1; } while (0)

#define TEST(NAME, PARENT) \
    void test_##NAME(void)
#define TEST_COND(NAME, PARENT, COND) \
    void test_##NAME(void)
#define TEST_TEAR_DOWN(NAME) \
    void test_tear_down_##NAME(void)
#define TEST_ONCE(NAME) \
    void test_##NAME(void)

/** Test that is expected to terminate with PANIC, i.e., exit(-1). It has no
 *  parent, it is run only once (not per task), and its body is run in its own
 *  fork -- the test fails if that fork does not PANIC.
 *
 *  The fork inherits the redirected stdout/stderr, so the body's output is
 *  captured in reg/_/NAME.out.tmp and the PANIC message in reg/_/NAME.err.tmp
 *  exactly as for any other test.
 *
 *  Because PANIC exits without unwinding, whatever the body allocated is
 *  leaked. This is deliberate: test.supp suppresses all leaks originating in
 *  test_panic_*(), so that make check-valgrind reports no memory leaks for
 *  this kind of test. The flip side is that a genuine leak in the tested code
 *  cannot be detected here either -- so the body must be self-contained, i.e.,
 *  it must allocate everything it needs itself, and this kind of test must
 *  never be relied on for leak checking. */
#define TEST_PANIC_ONCE(NAME) \
    void test_panic_##NAME(void)
#define TEST_GLOBAL_TEAR_DOWN() \
    void __test_global_tear_down(void)

#define XTEST(NAME, PARENT) \
    void __disabled_test_##NAME(void)
#endif /* __CPDDL_TEST_H__ */
