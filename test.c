#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

#include "test.in.c"

const char *TEST_TASK = NULL;

struct task {
    char *task;
    int found_run;
    int *test_run;
    int *test_ignore;
};
typedef struct task task_t;
static task_t *tasks = NULL;
static int tasks_size = 0;
static int tasks_alloc = 0;


static test_test_t *tests;
static int tests_size;
static int *tests_stats;
static int num_tasks_succeeded = 0;
static int num_tasks_failed = 0;

static void freeTasks(void);
static void freeTestTree(void);

static float timeDiffSeconds(const struct timespec *start,
                             const struct timespec *end)
{
    struct timespec diff;
    float sec;

    /* store into t difference between time_start and time_end */
    if (end->tv_nsec > start->tv_nsec){
        diff.tv_nsec = end->tv_nsec - start->tv_nsec;
        diff.tv_sec = end->tv_sec - start->tv_sec;
    }else{
        diff.tv_nsec = end->tv_nsec + 1000000000L - start->tv_nsec;
        diff.tv_sec = end->tv_sec - 1 - start->tv_sec;
    }

    sec  = diff.tv_nsec / 1000000000.f;
    sec += diff.tv_sec;
    return sec;
}

static void fmtOutputFilename(const char *task,
                              const char *test_name,
                              const char *suff,
                              char *filename)
{
    int size = sprintf(filename, "reg/tmp.%s.%s", task, test_name);
    char *c = filename + 8;
    for (; *c != 0x0; ++c){
        if ((*c < '0' || *c > '9')
                && (*c < 'a' || *c > 'z')
                && (*c < 'A' || *c > 'Z')
                && *c != '.'){
            *c = '_';
        }
    }
    sprintf(filename + size, ".%s", suff);
}

static void runTestTree(test_test_t *root,
                        int depth,
                        const int *test_ignore)
{

    int pid = fork();
    if (pid < 0){
        perror("Fork error");
        exit(-1);

    }else if (pid == 0){
        struct timespec time_start, time_end;
        clock_gettime(CLOCK_MONOTONIC, &time_start);

        if (root->test_fn != NULL){

            for (int i = 0; i < depth; ++i)
                fprintf(stdout, "  ");
            fprintf(stdout, "%s ...\n", root->name);
            fflush(stdout);

            char stdout_fn[256];
            fmtOutputFilename(TEST_TASK, root->name, "out", stdout_fn);
            fflush(stdout);
            int fd_stdout = dup(fileno(stdout));
            if (freopen(stdout_fn, "w", stdout) == NULL){
                perror("Redirecting of stdout failed");
                exit(-1);
            }

            char stderr_fn[256];
            fmtOutputFilename(TEST_TASK, root->name, "err", stderr_fn);
            fflush(stderr);
            int fd_stderr = dup(fileno(stderr));
            if (freopen(stderr_fn, "w", stderr) == NULL){
                perror("Redirecting of stderr failed");
                exit(-1);
            }

            root->test_fn();

            fflush(stdout);
            dup2(fd_stdout, fileno(stdout));
            close(fd_stdout);
            clearerr(stdout);


            fflush(stderr);
            dup2(fd_stderr, fileno(stderr));
            close(fd_stderr);
            clearerr(stderr);

            clock_gettime(CLOCK_MONOTONIC, &time_end);
            for (int i = 0; i < depth; ++i)
                fprintf(stdout, "  ");
            fprintf(stdout, "%s DONE [%.2fs]\n",
                    root->name, timeDiffSeconds(&time_start, &time_end));
            fflush(stdout);
        }

        for (int i = 0; i < root->child_size; ++i){
            test_test_t *next = root->child[i];
            if (test_ignore[next->id])
                continue;
            runTestTree(next, depth + 1, test_ignore);
        }

        if (root->test_fn_tear_down != NULL)
            root->test_fn_tear_down();

        clock_gettime(CLOCK_MONOTONIC, &time_end);
        for (int i = 0; i < depth; ++i)
            fprintf(stdout, "  ");
        fprintf(stdout, "%s TearDown [%.2fs]\n",
                root->name, timeDiffSeconds(&time_start, &time_end));
        fflush(stdout);

        freeTasks();
        freeTestTree();
        exit(0);

    }else{ // pid > 0
        int status;
        wait(&status);
        if (!WIFEXITED(status)){ /* if child process ends up abnormaly */
            if (WIFSIGNALED(status)){
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s was terminated by signal %d (%s).\n",
                        root->name, WTERMSIG(status), strsignal(WTERMSIG(status)));
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s FAILED\n", root->name);
            }else{
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s terminated abnormaly!\n", root->name);
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s FAILED\n", root->name);
            }

            tests_stats[root->id] = -1;

        }else{
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0){
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s terminated with exit status %d.\n",
                        root->name, exit_status);
                for (int i = 0; i < depth; ++i)
                    fprintf(stdout, "  ");
                fprintf(stdout, "%s FAILED\n", root->name);
                tests_stats[root->id] = -1;
            }else{
                if (tests_stats[root->id] == 0)
                    tests_stats[root->id] = 1;
            }
        }
    }
}

static void runTask(const task_t *task)
{
    struct timespec time_start, time_end;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    fprintf(stdout, "Task %s ...\n", task->task);
    fflush(stdout);

    bzero(tests_stats, sizeof(int) * tests_size);
    TEST_TASK = task->task;
    if (!task->found_run){
        for (int i = 0; i < tests_size; ++i){
            if (tests[i].parent == NULL && !tests[i].is_explicit)
                runTestTree(tests + i, 0, task->test_ignore);
        }

    }else{
        for (int i = 0; i < tests_size; ++i){
            if (task->test_run[i])
                runTestTree(tests + i, 0, task->test_ignore);
        }
    }

    int failed = 0;
    for (int i = 0; i < tests_size; ++i){
        if (tests_stats[i] < 0){
            tests[i].num_failed += 1;
            failed = 1;
        }else if (tests_stats[i] > 0){
            tests[i].num_succeeded += 1;
        }
    }
    if (failed){
        num_tasks_failed += 1;
    }else{
        num_tasks_succeeded += 1;
    }


    clock_gettime(CLOCK_MONOTONIC, &time_end);
    fprintf(stdout, "Task %s DONE [%.2fs]\n",
            task->task, timeDiffSeconds(&time_start, &time_end));
    fflush(stdout);
}

static void buildTestTree(void)
{
    tests_size = test_set_size;
    tests = malloc(sizeof(test_test_t) * tests_size);
    bzero(tests, sizeof(test_test_t) * tests_size);
    for (int ti = 0; ti < test_set_size; ++ti){
        tests[ti].id = ti;
        tests[ti].name = test_set[ti].name;
        tests[ti].is_explicit = test_set[ti].is_explicit;
        tests[ti].test_fn = test_set[ti].fn;
        tests[ti].test_fn_tear_down = test_set[ti].fn_tear_down;
        tests[ti].child = malloc(sizeof(test_test_t *) * tests_size);
    }

    for (int ti = 0; ti < test_set_size; ++ti){
        for (int i = 0; i < tests_size; ++i){
            if (strcmp(test_set[ti].parent, tests[i].name) == 0){
                tests[ti].parent = tests + i;
                tests[i].child[tests[i].child_size++] = tests + ti;
                break;
            }
        }
    }
}

static void freeTestTree(void)
{
    for (int i = 0; i < tests_size; ++i)
        free(tests[i].child);
    free(tests);
}

static void readTasks(void)
{
    size_t linesize = 0;
    char *line = NULL;
    ssize_t nread;
    while ((nread = getline(&line, &linesize, stdin)) >= 0){
        char *next = line;
        char *name = strsep(&next, " \n\t,;");
        if (strlen(name) == 0 || name[0] == '#')
            continue;

        if (tasks_size == tasks_alloc){
            if (tasks_alloc == 0)
                tasks_alloc = 8;
            tasks_alloc *= 2;
            tasks = realloc(tasks, sizeof(task_t) * tasks_alloc);
        }
        task_t *task = tasks + tasks_size++;
        task->task = strdup(name);
        task->test_run = calloc(tests_size, sizeof(int));
        task->test_ignore = calloc(tests_size, sizeof(int));
        task->found_run = 0;
        for (int i = 0; i < tests_size; ++i){
            if (tests[i].is_explicit && tests[i].parent == NULL)
                task->test_ignore[i] = 1;
        }
        char *cur;
        while ((cur = strsep(&next, " \n\t,;")) != NULL){
            if (cur[0] == '!'){
                for (int i = 0; i < tests_size; ++i){
                    if (strcmp(cur + 1, tests[i].name) == 0){
                        task->test_ignore[i] = 1;
                        task->test_run[i] = 0;
                    }
                }
            }else{
                for (int i = 0; i < tests_size; ++i){
                    if (strcmp(cur, tests[i].name) == 0){
                        task->test_ignore[i] = 0;
                        task->test_run[i] = 1;
                        task->found_run = 1;
                    }
                }
            }
        }
    }

    if (line != NULL)
        free(line);
}

static void freeTasks(void)
{
    for (int i = 0; i < tasks_size; ++i){
        free(tasks[i].task);
        free(tasks[i].test_run);
        free(tasks[i].test_ignore);
    }
    if (tasks != NULL)
        free(tasks);
}

static void printReportTestTree(test_test_t *t,
                                int name_len,
                                int succ_len,
                                int fail_len)
{
    printf("%s", t->name);
    for (int i = strlen(t->name); i < name_len; ++i)
        printf(" ");

    printf(" | ");
    printf("%*i", succ_len, t->num_succeeded);

    printf(" | ");
    printf("%*i", fail_len, t->num_failed);

    printf("\n");
    for (int i = 0; i < t->child_size; ++i)
        printReportTestTree(t->child[i], name_len, succ_len, fail_len);
}

static void printReport(void)
{
    int name_len = 5;
    for (int i = 0; i < tests_size; ++i){
        if (strlen(tests[i].name) > name_len)
            name_len = strlen(tests[i].name);
    }

    int succ_len = 4;
    int n = num_tasks_succeeded;
    int s = 1;
    while ((n = n / 10))
        ++s;
    if (s > succ_len)
        succ_len = s;
    for (int i = 0; i < tests_size; ++i){
        int n = tests[i].num_succeeded;
        int s = 1;
        while ((n = n / 10))
            ++s;
        if (s > succ_len)
            succ_len = s;
    }

    int fail_len = 4;
    n = num_tasks_failed;
    s = 1;
    while ((n = n / 10))
        ++s;
    if (s > fail_len)
        fail_len = s;
    for (int i = 0; i < tests_size; ++i){
        int n = tests[i].num_failed;
        int s = 1;
        while ((n = n / 10))
            ++s;
        if (s > fail_len)
            fail_len = s;
    }

    printf("\n");
    for (int i = 0; i < name_len; ++i)
        printf(" ");
    printf(" | succ | fail\n");
    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("-------\n");

    for (int i = 0; i < tests_size; ++i){
        if (tests[i].parent == NULL)
            printReportTestTree(tests + i, name_len, succ_len, fail_len);
    }

    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("-------\n");
    printf("tasks");
    for (int i = 5; i < name_len; ++i)
        printf(" ");

    printf(" | ");
    printf("%*i", succ_len, num_tasks_succeeded);

    printf(" | ");
    printf("%*i", fail_len, num_tasks_failed);
    printf("\n");
}

int main(int argc, char *argv[])
{
    buildTestTree();
    readTasks();

    size_t shared_size = sizeof(int) * tests_size;
    void *shared_mem = mmap(NULL, shared_size, PROT_WRITE | PROT_READ,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    tests_stats = shared_mem;

    for (int i = 0; i < tasks_size; ++i)
        runTask(tasks + i);

    munmap(shared_mem, shared_size);

    printReport();
    int ret = (num_tasks_failed == 0);

    freeTasks();
    freeTestTree();
    if (ret)
        return 0;
    return 1;
}
