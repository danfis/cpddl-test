#ifndef __CPDDL_TEST_H__
#define __CPDDL_TEST_H__

struct test_def {
    char *name;
    void (*fn)(void);
    void (*fn_tear_down)(void);
    char *parent;
};
typedef struct test_def test_def_t;

typedef struct test_test test_test_t;
struct test_test {
    int id;
    const char *name;
    void (*test_fn)(void);
    void (*test_fn_tear_down)(void);

    test_test_t *parent;
    test_test_t **child;
    int child_size;

    int num_failed;
    int num_succeeded;
};

extern const char *TEST_TASK;

#define TEST(NAME, PARENT) \
    void test_##NAME(void)
#define TEST_TEAR_DOWN(NAME) \
    void test_tear_down_##NAME(void)

#endif /* __CPDDL_TEST_H__ */
