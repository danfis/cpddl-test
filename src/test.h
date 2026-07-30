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
#define TEST_GLOBAL_TEAR_DOWN() \
    void __test_global_tear_down(void)

#define XTEST(NAME, PARENT) \
    void __disabled_test_##NAME(void)
#endif /* __CPDDL_TEST_H__ */
