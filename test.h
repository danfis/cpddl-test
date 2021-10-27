#ifndef __CPDDL_TEST_H__
#define __CPDDL_TEST_H__

struct test_def {
    char *name;
    void (*fn)(void);
    void (*fn_tear_down)(void);
    char *parent;
    int is_explicit;
};
typedef struct test_def test_def_t;

extern const char *TEST_TASK;

#define TEST(NAME, PARENT) \
    void test_##NAME(void)
#define TEST_TEAR_DOWN(NAME) \
    void test_tear_down_##NAME(void)
#define TEST_EXPLICIT(NAME) \
    void test_##NAME(void)
#define TEST_GLOBAL_TEAR_DOWN() \
    void __test_global_tear_down(void)

#endif /* __CPDDL_TEST_H__ */
