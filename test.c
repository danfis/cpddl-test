#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include "test.h"

#include "test.in.c"

#define TIMEOUT 0

static const char *FAIL_FILENAME = "reg/tmp.failed";

const char *TEST_TASK = NULL;

typedef struct test_test test_test_t;
struct test_test {
    int id;
    const char *name;
    int is_explicit;
    void (*test_fn)(void);
    void (*test_fn_tear_down)(void);
    int enabled;

    test_test_t *parent;
    test_test_t **child;
    int child_size;

    int num_failed;
    int num_succeeded;
    float time_sum;
};

struct task {
    char *task;
    int disabled;
    int found_run;
    int *test_run;
    int *test_ignore;
};
typedef struct task task_t;

struct tasks {
    task_t *tasks;
    int size;
    int alloc;
};
typedef struct tasks tasks_t;

static tasks_t *tasks = NULL;


static test_test_t *tests;
static int tests_size;
static int *tests_stats;
static float *tests_time_sum;
static int tests_enabled = 0;

#ifdef USE_GLOBAL_TEAR_DOWN
static void (*global_tear_down)(void) = __test_global_tear_down;
#else /* USE_GLOBAL_TEAR_DOWN */
static void (*global_tear_down)(void) = NULL;
#endif /* USE_GLOBAL_TEAR_DOWN */
static int num_tasks_succeeded = 0;
static int num_tasks_failed = 0;
static struct timespec g_time_start, g_time_end;

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

static int fmtFilename(const char *task,
                       const char *test_name,
                       char *filename)
{
    int ret = sprintf(filename, "%s.%s", task, test_name);
    char *c = filename;
    for (; *c != 0x0; ++c){
        if (*c == '/'){
            *c = '-';
        }
    }
    return ret;
}

static void fmtBaseFilename(const char *task,
                            const char *test_name,
                            const char *suff,
                            char *filename)
{
    sprintf(filename, "reg/");
    int siz = fmtFilename(task, test_name, filename + 4);
    sprintf(filename + 4 + siz, ".%s", suff);
}

static void fmtOutputFilename(const char *task,
                              const char *test_name,
                              const char *suff,
                              char *filename)
{
    sprintf(filename, "reg/tmp.");
    int siz = fmtFilename(task, test_name, filename + 8);
    sprintf(filename + 8 + siz, ".%s", suff);
}

static size_t filesize(const char *fn)
{
    FILE *fp = fopen(fn, "r");
    if (fp == NULL)
        return 0;
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fclose(fp);
    return sz;
}

static void addFailure(const test_test_t *t, int status)
{
    FILE *failout = fopen(FAIL_FILENAME, "a");
    if (failout == NULL){
        perror("Opening file for failures failed");
        exit(-1);
    }
    fprintf(failout, "%s %s --> %d", TEST_TASK, t->name, WEXITSTATUS(status));
    if (!WIFEXITED(status)){
        if (WIFSIGNALED(status)){
            fprintf(failout, " | signal %d (%s)",
                    WTERMSIG(status), strsignal(WTERMSIG(status)));
        }else{
            fprintf(failout, " | terminated abnormaly");
        }
    }

    char fn[512];
    fmtOutputFilename(TEST_TASK, t->name, "out", fn);
    fprintf(failout, " | %s", fn);
    fprintf(failout, "\n");
    fflush(failout);
    fclose(failout);

    char base[512];
    fmtBaseFilename(TEST_TASK, t->name, "out", base);
    if (access(base, F_OK) == 0){
        char cmd[2048];
        sprintf(cmd, "diff -y -W %s %s | head -15 | awk '{print \"  >OUT>\", $0}' >>%s",
                base, fn, FAIL_FILENAME);
        system(cmd);
    }else if (filesize(fn) > 0){
        char cmd[2048];
        sprintf(cmd, "cat %s | head -15 | awk '{print \"  >OUT>\", substr($0, 0, 150)}'>>%s",
                fn, FAIL_FILENAME);
        system(cmd);
    }

    fmtOutputFilename(TEST_TASK, t->name, "err", fn);
    fmtBaseFilename(TEST_TASK, t->name, "err", base);
    if (access(base, F_OK) == 0){
        char cmd[2048];
        sprintf(cmd, "diff -y -W %s %s | head -15 | awk '{print \"  >ERR>\", $0}' >>%s",
                base, fn, FAIL_FILENAME);
        system(cmd);
    }else if (filesize(fn) > 0){
        char cmd[2048];
        sprintf(cmd, "cat %s | head -15 | awk '{print \"  >ERR>\", substr($0, 0, 150)}'>>%s",
                fn, FAIL_FILENAME);
        system(cmd);
    }
}

static void printNameTree(const test_test_t *t)
{
    if (t->parent != NULL)
        printNameTree(t->parent);
    if (t->parent != NULL)
        fprintf(stdout, ".");
    fprintf(stdout, "%s", t->name);
}

static void *thTimeout(void *_)
{
    sleep(TIMEOUT);
    kill(getpid(), SIGTERM);
    return NULL;
}

static void runTestTree(test_test_t *root,
                        int depth,
                        const int *test_ignore)
{
    if (tests_enabled && !root->enabled)
        return;

    int pid = fork();
    if (pid < 0){
        perror("Fork error");
        exit(-1);

    }else if (pid == 0){
        struct timespec time_start, time_end;
        clock_gettime(CLOCK_MONOTONIC, &time_start);

        if (root->test_fn != NULL){

            fprintf(stdout, "%s ", TEST_TASK);
            printNameTree(root);
            fprintf(stdout, " ...\n");
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

            pthread_t thtimeout;
            if (TIMEOUT > 0)
                pthread_create(&thtimeout, NULL, thTimeout, NULL);
            root->test_fn();
            if (TIMEOUT > 0)
                pthread_cancel(thtimeout);

            fflush(stdout);
            dup2(fd_stdout, fileno(stdout));
            close(fd_stdout);
            clearerr(stdout);


            fflush(stderr);
            dup2(fd_stderr, fileno(stderr));
            close(fd_stderr);
            clearerr(stderr);

            clock_gettime(CLOCK_MONOTONIC, &time_end);
            fprintf(stdout, "%s ", TEST_TASK);
            printNameTree(root);
            float elapsed = timeDiffSeconds(&time_start, &time_end);
            tests_time_sum[root->id] += elapsed;
            fprintf(stdout, " DONE [%.2fs / %.2fs]\n",
                    elapsed, tests_time_sum[root->id]);
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
        if (global_tear_down != NULL)
            global_tear_down();

        clock_gettime(CLOCK_MONOTONIC, &time_end);
        fprintf(stdout, "%s ", TEST_TASK);
        printNameTree(root);
        fprintf(stdout, " TearDown [%.2fs]\n",
                timeDiffSeconds(&time_start, &time_end));
        fflush(stdout);

        freeTasks();
        freeTestTree();

        exit(0);

    }else{ // pid > 0
        int status;
        wait(&status);

        char ret_fn[256];
        fmtOutputFilename(TEST_TASK, root->name, "ret", ret_fn);
        FILE *retout = fopen(ret_fn, "w");
        if (retout == NULL){
            perror("Opening .ret file failed");
            exit(-1);
        }

        int failed = 0;
        if (!WIFEXITED(status)){ /* if child process ends up abnormaly */
            if (WIFSIGNALED(status)){
                fprintf(stdout, "%s was terminated by signal %d (%s).\n",
                        root->name, WTERMSIG(status), strsignal(WTERMSIG(status)));
                fprintf(stdout, "%s FAILED\n", root->name);
            }else{
                fprintf(stdout, "%s terminated abnormaly!\n", root->name);
                fprintf(stdout, "%s FAILED\n", root->name);
            }

            tests_stats[root->id] = -1;
            fprintf(retout, "1\n");
            failed = 1;

        }else{
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0){
                fprintf(stdout, "%s terminated with exit status %d.\n",
                        root->name, exit_status);
                fprintf(stdout, "%s FAILED\n", root->name);
                tests_stats[root->id] = -1;
                failed = 1;
            }else{
                if (tests_stats[root->id] == 0)
                    tests_stats[root->id] = 1;
            }
            fprintf(retout, "%d\n", exit_status);
        }

        fclose(retout);

        // TODO: diff -q

        if (failed)
            addFailure(root, status);
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
        tests[i].time_sum = tests_time_sum[i];
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
    for (int i = 0; i < tests_size; ++i){
        free(tests[i].child);
    }
    free(tests);
}

static void enableTest(test_test_t *t)
{
    t->enabled = 1;
    if (t->parent != NULL)
        enableTest(t->parent);
}

static void filterTestsSubstr(const char *name)
{
    tests_enabled = 1;
    for (int i = 0; i < tests_size; ++i){
        if (strstr(tests[i].name, name) != NULL)
            enableTest(tests + i);
    }
}

static void filterTests(const char *name)
{
    tests_enabled = 1;
    for (int i = 0; i < tests_size; ++i){
        if (strcmp(tests[i].name, name) == 0)
            enableTest(tests + i);
    }
}

static task_t *addTask(tasks_t *tasks, const char *name)
{
    if (tasks->size == tasks->alloc){
        if (tasks->alloc == 0)
            tasks->alloc = 8;
        tasks->alloc *= 2;
        tasks->tasks = realloc(tasks->tasks, sizeof(task_t) * tasks->alloc);
    }

    task_t *task = tasks->tasks + tasks->size++;
    bzero(task, sizeof(*task));
    task->task = strdup(name);
    task->test_run = calloc(tests_size, sizeof(int));
    task->test_ignore = calloc(tests_size, sizeof(int));
    task->found_run = 0;
    for (int i = 0; i < tests_size; ++i){
        if (tests[i].is_explicit && tests[i].parent == NULL)
            task->test_ignore[i] = 1;
    }
    return task;
}

static void disableTaskTest(task_t *task, const char *test_name)
{
    for (int i = 0; i < tests_size; ++i){
        if (strcmp(test_name, tests[i].name) == 0){
            task->test_ignore[i] = 1;
            task->test_run[i] = 0;
        }
    }
}

static void enableTaskTest(task_t *task, const char *test_name)
{
    for (int i = 0; i < tests_size; ++i){
        if (strcmp(test_name, tests[i].name) == 0){
            task->test_ignore[i] = 0;
            task->test_run[i] = 1;
            task->found_run = 1;
        }
    }
}

static void _freeTasks(tasks_t *tasks)
{
    for (int i = 0; i < tasks->size; ++i){
        free(tasks->tasks[i].task);
        free(tasks->tasks[i].test_run);
        free(tasks->tasks[i].test_ignore);
    }
    if (tasks->tasks != NULL)
        free(tasks->tasks);
}

#include "test.tasks.base.in.c"
#include "test.tasks.all.in.c"

static void setUpTasks(void)
{
    tasks = &tasks_base;
    addTasks_base();
    addTasks_all();
}


static void freeTasks(void)
{
    _freeTasks(&tasks_base);
}

static void filterTasksSubstr(const char *s)
{
    for (int i = 0; i < tasks->size; ++i){
        if (strstr(tasks->tasks[i].task, s) == NULL)
            tasks->tasks[i].disabled = 1;
    }
}

static void filterTasks(const char *s)
{
    for (int i = 0; i < tasks->size; ++i){
        if (strcmp(tasks->tasks[i].task, s) != 0)
            tasks->tasks[i].disabled = 1;
    }
}

static void printReportFailures(void)
{
    if (num_tasks_failed == 0)
        return;

    FILE *fin = fopen(FAIL_FILENAME, "r");
    if (fin == NULL)
        return;

    printf("\n");

    char buf[512];
    size_t r;
    while ((r = fread(buf, sizeof(char), 512, fin)) > 0)
        fwrite(buf, sizeof(char), r, stdout);
    fclose(fin);
}

static void printReportTest(test_test_t *t,
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

    printf(" | ");
    printf("%7.2fs", t->time_sum);

    printf("\n");
}

static void printReport(void)
{
    int name_len = 5;
    for (int i = 0; i < tests_size; ++i){
        if (tests[i].num_succeeded == 0 && tests[i].num_failed == 0)
            continue;
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
        if (tests[i].num_succeeded == 0 && tests[i].num_failed == 0)
            continue;
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
    printf(" | succ | fail | time\n");
    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("------------------\n");

    for (int i = 0; i < tests_size; ++i){
        if (tests[i].num_succeeded == 0 && tests[i].num_failed == 0)
            continue;
        printReportTest(tests + i, name_len, succ_len, fail_len);
    }

    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("------------------\n");
    printf("tasks");
    for (int i = 5; i < name_len; ++i)
        printf(" ");

    printf(" | ");
    printf("%*i", succ_len, num_tasks_succeeded);

    printf(" | ");
    printf("%*i", fail_len, num_tasks_failed);
    printf(" | %7.2fs", timeDiffSeconds(&g_time_start, &g_time_end));
    printf("\n");

    printReportFailures();
}

static void cleanRegDir(void)
{
    DIR *dp;
    struct dirent *ep;     
    dp = opendir ("reg/");

    if (dp != NULL){
        while ((ep = readdir(dp)) != NULL){
            if (strncmp(ep->d_name, "tmp.", 4) == 0){
                char f[512];
                sprintf(f, "reg/%s", ep->d_name);
                unlink(f);
            }
        }
        closedir(dp);
    }
}

static void usage(char *p)
{
    fprintf(stderr, "Usage: %s [-a]"
                    " [-S task_substr] [-T task_name]"
                    " [-s test_substr] [-t test_name]\n", p);
    exit(-1);
}

int main(int argc, char *argv[])
{
    struct rlimit mem_limit;
    mem_limit.rlim_cur = mem_limit.rlim_max = 4096ul * 1024ul * 1024ul;
    setrlimit(RLIMIT_AS, &mem_limit);

    cleanRegDir();
    buildTestTree();
    setUpTasks();

    int opt;
    while ((opt = getopt(argc, argv, "aS:T:s:t:")) != -1) {
        switch (opt) {
            case 'a':
                tasks = &tasks_all;
                break;
            case 'S':
                filterTasksSubstr(optarg);
                break;
            case 'T':
                filterTasks(optarg);
                break;
            case 's':
                filterTestsSubstr(optarg);
                break;
            case 't':
                filterTests(optarg);
                break;
            default: /* '?' */
                usage(argv[0]);
        }
    }
    if (optind != argc)
        usage(argv[0]);


    size_t shared_size = sizeof(int) * tests_size;
    shared_size += sizeof(float) * tests_size;
    void *shared_mem = mmap(NULL, shared_size, PROT_WRITE | PROT_READ,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    tests_stats = shared_mem;
    tests_time_sum = (float *)(tests_stats + tests_size);

    clock_gettime(CLOCK_MONOTONIC, &g_time_start);
    for (int i = 0; i < tasks->size; ++i){
        if (!tasks->tasks[i].disabled)
            runTask(tasks->tasks + i);
    }
    clock_gettime(CLOCK_MONOTONIC, &g_time_end);

    munmap(shared_mem, shared_size);

    printReport();
    int ret = (num_tasks_failed == 0);

    freeTasks();
    freeTestTree();
    if (ret)
        return 0;
    return 1;
}
