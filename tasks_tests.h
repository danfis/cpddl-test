#ifndef TASKS_TESTS_H
#define TASKS_TESTS_H

struct test_def {
    int id;
    char *name;
    void (*fn)(void);
    void (*fn_tear_down)(void);
    int parent;
    const int *children;
    int children_size;
    int is_explicit;
};
typedef struct test_def test_def_t;


void tasksTestsInit(void);
void tasksTestsSelectAll(void);
void tasksTestsSelectTasksMatch(const char *pat);
void tasksTestsSelectTask(const char *task_name);
void tasksTestsSelectTestsMatch(const char *pat);
void tasksTestsSelectTest(const char *name);
void tasksTestsPrintPlan(void);
int tasksTestsNumTasks(void);
int tasksTestsNumTests(void);
const test_def_t *tasksTestsGetTest(int test_id);
const char *tasksTestsGetTaskName(int task_id);
int tasksTestsIsEnabled(int task_id, int test_id);
int tasksTestsNumActiveTasks(void);

void (*tasksTestsGlobalTearDown(void))(void);
#endif /* TASKS_TESTS_H */
