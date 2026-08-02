#ifndef TASKS_TESTS_H
#define TASKS_TESTS_H

#define PDDL_TASK_SET_ALL   0
#define PDDL_TASK_SET_QUICK 1
#define PDDL_TASK_SET_BASE  2

struct test_def {
    int id;
    char *name;
    void (*fn)(void);
    void (*fn_tear_down)(void);
    int parent;
    const int *children;
    int children_size;
    int is_once;
    int is_panic;
    int enabled;
};
typedef struct test_def test_def_t;


void tasksTestsEnableTestMatch(const char *pat);
void tasksTestsEnableTestEq(const char *pat);
void tasksTestsEnableTaskMatch(const char *pat);
void tasksTestsEnableTaskEq(const char *pat);
void tasksTestsEnableAllTasks(int task_set);
void tasksTestsEnableAllTests(void);

void tasksTestsSetEnableMatrix(void);


void tasksTestsPrintPlan(void);
void tasksTestsPrintTasks(void);
void tasksTestsPrintTests(void);
int tasksTestsNumEnabledTasks(void);
int tasksTestsNumEnabledTests(void);
int tasksTestsNumEnabledJobs(void);

int tasksTestsNumTasks(void);
int tasksTestsNumTests(void);
const test_def_t *tasksTestsGetTest(int test_id);
const char *tasksTestsGetTaskName(int task_id);
int tasksTestsTaskIsEnabled(int task_id);
int tasksTestsIsEnabled(int task_id, int test_id);

void (*tasksTestsGlobalTearDown(void))(void);
#endif /* TASKS_TESTS_H */
