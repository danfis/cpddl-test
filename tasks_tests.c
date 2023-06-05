#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "tasks_tests.h"


#include "test.in.c"

static int testIdFromName(const char *name)
{
    for (int test_id = 0; test_id < test_set_size; ++test_id){
        if (strcmp(test_set[test_id].name, name) == 0)
            return test_id;
    }
    return -1;
}

#include "tasks.in.c"

static void taskEnableTestAndChildren(int task_id, int test_id, int force)
{
    if (!force && tasks[task_id].default_disabled_tests[test_id])
        return;
    tasks[task_id].enabled_tests[test_id] = 1;

    const test_def_t *test = tasksTestsGetTest(test_id);
    for (int i = 0; i < test->children_size; ++i)
        taskEnableTestAndChildren(task_id, test->children[i], 0);
}

static void taskEnableTestAndParents(int task_id, int test_id, int force)
{
    if (!force && tasks[task_id].default_disabled_tests[test_id])
        return;

    test_set[test_id].enabled = 1;
    tasks[task_id].enabled_tests[test_id] = 1;

    const test_def_t *test = tasksTestsGetTest(test_id);
    if (test->parent >= 0)
        taskEnableTestAndParents(task_id, test->parent, 0);
}

static void tasksEnableTask(int task_id)
{
    tasks[task_id].enabled = 1;
}

static void tasksEnableTestAndParents(int test_id, int force)
{
    for (int taski = 0; taski < tasks_size; ++taski)
        taskEnableTestAndParents(taski, test_id, force);
}

static int cmpEq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int cmpMatch(const char *a, const char *b)
{
    return strstr(a, b) != NULL;
}

static void _tasksTestsEnableTest(const char *pat,
                                  int (*cmp)(const char *, const char *),
                                  int force)
{
    for (int testi = 0; testi < test_set_size; ++testi){
        if (cmp(test_set[testi].name, pat)){
            test_set[testi].enabled = 1;
            tasksEnableTestAndParents(testi, force);
        }
    }
}

void tasksTestsEnableTestMatch(const char *pat)
{
    _tasksTestsEnableTest(pat, cmpMatch, 1);
}

void tasksTestsEnableTestEq(const char *pat)
{
    _tasksTestsEnableTest(pat, cmpEq, 1);
}

static void _tasksTestsEnableTask(const char *pat,
                                  int (*cmp)(const char *, const char *))
{
    for (int taski = 0; taski < tasks_size; ++taski){
        if (cmp(tasks[taski].name, pat))
            tasks[taski].enabled = 1;
    }
}

void tasksTestsEnableTaskMatch(const char *pat)
{
    _tasksTestsEnableTask(pat, cmpMatch);
}

void tasksTestsEnableTaskEq(const char *pat)
{
    _tasksTestsEnableTask(pat, cmpEq);
}

void tasksTestsEnableAllTasks(int only_base)
{
    for (int taski = 0; taski < tasks_size; ++taski){
        if (only_base && !tasks[taski].is_base)
            continue;
        tasksEnableTask(taski);
    }
}

void tasksTestsEnableAllTests(void)
{
    for (int testi = 0; testi < test_set_size; ++testi)
        test_set[testi].enabled = 1;

    for (int taski = 0; taski < tasks_size; ++taski){
        for (int testi = 0; testi < test_set_size; ++testi){
            if (tasks[taski].default_disabled_tests[testi])
                continue;

            const test_def_t *test = tasksTestsGetTest(testi);
            if (tasks[taski].default_enabled_tests[testi]){
                tasks[taski].enabled_tests[testi] = 1;

            }else if (!test->is_explicit && test->parent < 0){
                taskEnableTestAndChildren(taski, testi, 0);

            }else if (test->is_explicit
                        && test->parent < 0
                        && tasks[taski].default_enabled_tests[testi]){
                taskEnableTestAndChildren(taski, testi, 0);
            }
        }
    }
}

void tasksTestsInit(void)
{
    tasksInit();

    for (int taski = 0; taski < tasks_size; ++taski){
        if (strncmp(tasks[taski].name, "_", 1) == 0){
            for (int testi = 0; testi < test_set_size; ++testi){
                if (!tasks[taski].default_enabled_tests[testi]){
                    tasks[taski].default_disabled_tests[testi] = 1;
                }
            }
        }
    }

}

static void printPlan(int task_id, int test_id, int depth)
{
    if (!tasks[task_id].enabled_tests[test_id])
        return;

    for (int i = 0; i < depth - 1; ++i)
        printf("| ");
    printf("|-");
    printf("%s\n", test_set[test_id].name);
    for (int i = 0; i < test_set[test_id].children_size; ++i)
        printPlan(task_id, test_set[test_id].children[i], depth + 1);

}

void tasksTestsPrintPlan(void)
{
    for (int task_id = 0; task_id < tasks_size; ++task_id){
        if (!tasks[task_id].enabled)
            continue;

        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (!test_set[test_id].enabled)
                continue;

            if (test_set[test_id].parent < 0
                    && tasks[task_id].enabled_tests[test_id]){
                printf("Task %s:\n", tasks[task_id].name);
                printPlan(task_id, test_id, 1);
            }
        }
    }
}

void tasksTestsPrintTasks(void)
{
    for (int task_id = 0; task_id < tasks_size; ++task_id){
        printf("%s", tasks[task_id].name);
        for (int i = strlen(tasks[task_id].name); i < 50; ++i)
            printf(" ");
        if (tasks[task_id].enabled){
            printf(" enabled\n");
        }else{
            printf(" disabled\n");
        }
    }
}

void tasksTestsPrintTests(void)
{
    for (int test_id = 0; test_id < test_set_size; ++test_id){
        printf("%s", test_set[test_id].name);
        for (int i = strlen(test_set[test_id].name); i < 50; ++i)
            printf(" ");
        if (test_set[test_id].enabled){
            printf(" enabled\n");
        }else{
            printf(" disabled\n");
        }
    }
}

int tasksTestsNumTasks(void)
{
    return tasks_size;
}

int tasksTestsNumTests(void)
{
    return test_set_size;
}

const test_def_t *tasksTestsGetTest(int id)
{
    return test_set + id;
}

const char *tasksTestsGetTaskName(int task_id)
{
    return tasks[task_id].name;
}

int tasksTestsTaskIsEnabled(int task_id)
{
    return tasks[task_id].enabled;
}

int tasksTestsIsEnabled(int task_id, int test_id)
{
    return tasks[task_id].enabled
                && test_set[test_id].enabled
                && tasks[task_id].enabled_tests[test_id];
}


int tasksTestsNumEnabledTasks(void)
{
    int num = 0;
    for (int i = 0; i < tasks_size; ++i){
        if (tasks[i].enabled)
            ++num;
    }
    return num;
}

int tasksTestsNumEnabledTests(void)
{
    int num = 0;
    for (int i = 0; i < test_set_size; ++i){
        if (test_set[i].enabled)
            ++num;
    }
    return num;
}

int tasksTestsNumEnabledJobs(void)
{
    int num = 0;
    for (int taski = 0; taski < tasks_size; ++taski){
        if (!tasks[taski].enabled)
            continue;
        for (int testi = 0; testi < tasks_size; ++testi){
            if (!test_set[testi].enabled)
                continue;
            if (tasks[taski].enabled_tests[testi])
                ++num;
        }
    }
    return num;
}

void (*tasksTestsGlobalTearDown(void))(void)
{
#ifdef USE_GLOBAL_TEAR_DOWN
    return __test_global_tear_down;
#else /* USE_GLOBAL_TEAR_DOWN */
    return NULL;
#endif /* USE_GLOBAL_TEAR_DOWN */
}
