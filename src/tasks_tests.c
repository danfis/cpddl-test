#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "tasks_tests.h"


#include "tests_tasks.in.c"

static void tasksEnableTask(int task_id)
{
    tasks[task_id].enabled = 1;
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
                                  int (*cmp)(const char *, const char *))
{
    for (int testi = 0; testi < test_set_size; ++testi){
        if (cmp(test_set[testi].name, pat)){
            test_set[testi].enabled = 1;
        }
    }
}

void tasksTestsEnableTestMatch(const char *pat)
{
    _tasksTestsEnableTest(pat, cmpMatch);
}

void tasksTestsEnableTestEq(const char *pat)
{
    _tasksTestsEnableTest(pat, cmpEq);
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

void tasksTestsEnableAllTasks(int task_set)
{
    for (int taski = 0; taski < tasks_size; ++taski){
        if (task_set == PDDL_TASK_SET_BASE && !tasks[taski].is_base)
            continue;
        if (task_set == PDDL_TASK_SET_QUICK && !tasks[taski].is_quick)
            continue;
        tasksEnableTask(taski);
    }
}

void tasksTestsEnableAllTests(void)
{
    for (int testi = 0; testi < test_set_size; ++testi)
        test_set[testi].enabled = 1;
}


static int enableTestForTaskRec(int task_id, int test_id)
{
    if (!tasks[task_id].allowed_tests[test_id])
        return 0;

    const test_def_t *test = tasksTestsGetTest(test_id);
    if (test->parent < 0){
        tasks[task_id].enabled_tests[test_id] = 1;
        return 1;

    }else{
        if (enableTestForTaskRec(task_id, test->parent)){
            tasks[task_id].enabled_tests[test_id] = 1;
            return 1;

        }else{
            return 0;
        }
    }
}

void tasksTestsSetEnableMatrix(void)
{
    for (int taski = 0; taski < tasks_size; ++taski){
        if (!tasks[taski].enabled)
            continue;
        for (int testi = 0; testi < test_set_size; ++testi){
            if (!test_set[testi].enabled)
                continue;

            enableTestForTaskRec(taski, testi);
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
    printf("%s", test_set[test_id].name);
    printf("\n");
    for (int i = 0; i < test_set[test_id].children_size; ++i)
        printPlan(task_id, test_set[test_id].children[i], depth + 1);

}

void tasksTestsPrintPlan(void)
{
    for (int task_id = 0; task_id < tasks_size; ++task_id){
        if (!tasks[task_id].enabled)
            continue;

        int printed_task_name = 0;
        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].parent < 0
                    && tasks[task_id].enabled_tests[test_id]){
                if (!printed_task_name){
                    printf("Task %s:\n", tasks[task_id].name);
                    printed_task_name = 1;
                }
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
        for (int testi = 0; testi < test_set_size; ++testi){
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
