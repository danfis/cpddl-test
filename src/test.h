#ifndef __CPDDL_TEST_H__
#define __CPDDL_TEST_H__

extern const char *TEST_TASK;

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
