#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "tasks_tests.h"

static const char **task_set = NULL;
static int *task_test_map = NULL;
static int task_size = 0;

#include "test.in.c"

static int testIdFromName(const char *name)
{
    for (int test_id = 0; test_id < test_set_size; ++test_id){
        if (strcmp(test_set[test_id].name, name) == 0)
            return test_id;
    }
    return -1;
}

static void initTaskTestMap(const char **tasks, int *map, int task_size)
{
    for (int task_id = 0; task_id < task_size; ++task_id){
        int enabled = 1;
        if (strcmp(tasks[task_id], "_") == 0)
            enabled = 0;

        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].is_explicit){
                map[task_id * test_set_size + test_id] = 0;
            }else{
                map[task_id * test_set_size + test_id] = enabled;
            }
        }
    }
}

static void disableTaskTest(int *map, int task_size, int task_id, int test_id)
{
    map[task_id * test_set_size + test_id] = 0;
    for (int i = 0; i < test_set[test_id].children_size; ++i)
        disableTaskTest(map, task_size, task_id, test_set[test_id].children[i]);
}

static void enableTaskTest(int *map, int task_size, int task_id, int test_id)
{
    assert(test_set[test_id].parent < 0);
    map[task_id * test_set_size + test_id] = 1;
}

#include "test.tasks.base.in.c"
#include "test.tasks.all.in.c"

void tasksTestsInit(void)
{
    setTaskTestMap_base();
    setTaskTestMap_all();
    task_set = tasks_base;
    task_test_map = task_test_map_base;
    task_size = tasks_base_size;
}

void tasksTestsSelectAll(void)
{
    task_set = tasks_all;
    task_test_map = task_test_map_all;
    task_size = tasks_all_size;
}

void tasksTestsSelectTasksMatch(const char *pat)
{
    for (int task_id = 0; task_id < task_size; ++task_id){
        if (strstr(task_set[task_id], pat) == NULL){
            for (int test_id = 0; test_id < test_set_size; ++test_id){
                task_test_map[task_id * test_set_size + test_id] = 0;
            }
        }
    }
}

void tasksTestsSelectTask(const char *task_name)
{
    for (int task_id = 0; task_id < task_size; ++task_id){
        if (strcmp(task_set[task_id], task_name) != 0){
            for (int test_id = 0; test_id < test_set_size; ++test_id){
                task_test_map[task_id * test_set_size + test_id] = 0;
            }
        }
    }
}

static int selectTestMatch(int task_id, int test_id, const char *pat)
{
    if (!task_test_map[task_id * test_set_size + test_id])
        return 0;

    int enable = 0;
    for (int i = 0; i < test_set[test_id].children_size; ++i){
        int chid = test_set[test_id].children[i];
        enable |= selectTestMatch(task_id, chid, pat);
    }

    if (enable || strstr(test_set[test_id].name, pat) != NULL){
        return 1;
    }else{
        task_test_map[task_id * test_set_size + test_id] = 0;
        return 0;
    }
}

static int selectTest(int task_id, int test_id, const char *name)
{
    if (!task_test_map[task_id * test_set_size + test_id])
        return 0;

    int enable = 0;
    for (int i = 0; i < test_set[test_id].children_size; ++i){
        int chid = test_set[test_id].children[i];
        enable |= selectTest(task_id, chid, name);
    }

    if (enable || strcmp(test_set[test_id].name, name) == 0){
        return 1;
    }else{
        task_test_map[task_id * test_set_size + test_id] = 0;
        return 0;
    }
}

void tasksTestsSelectTestsMatch(const char *pat)
{
    for (int task_id = 0; task_id < task_size; ++task_id){
        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].parent < 0
                    && task_test_map[task_id * test_set_size + test_id]){
                selectTestMatch(task_id, test_id, pat);
            }
        }
    }
}

void tasksTestsSelectTest(const char *name)
{
    for (int task_id = 0; task_id < task_size; ++task_id){
        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].parent < 0
                    && task_test_map[task_id * test_set_size + test_id]){
                selectTest(task_id, test_id, name);
            }
        }
    }
}

static void printPlan(int task_id, int test_id, int depth)
{
    if (!task_test_map[task_id * test_set_size + test_id])
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
    for (int task_id = 0; task_id < task_size; ++task_id){
        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].parent < 0
                    && task_test_map[task_id * test_set_size + test_id]){
                printf("Task %s:\n", task_set[task_id]);
                printPlan(task_id, test_id, 1);
            }
        }
    }
}

int tasksTestsNumTasks(void)
{
    return task_size;
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
    return task_set[task_id];
}

int tasksTestsIsEnabled(int task_id, int test_id)
{
    if (test_id < 0){
        for (int test_id = 0; test_id < test_set_size; ++test_id){
            if (test_set[test_id].parent < 0
                    && task_test_map[task_id * test_set_size + test_id])
                return 1;
        }
        return 0;
    }

    return task_test_map[task_id * test_set_size + test_id];
}

int tasksTestsNumActiveTasks(void)
{
    int num = 0;
    for (int i = 0; i < task_size; ++i)
        num += tasksTestsIsEnabled(i, -1);
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
