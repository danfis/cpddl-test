#define _DEFAULT_SOURCE
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <dirent.h>
#include <pthread.h>
#include <assert.h>
#include "pddl/pddl.h"
#include "tasks_tests.h"

#define DEFAULT_PARALLEL 6
#define DEFAULT_TIMEOUT 180
#define PROGRESS_COLUMNS 180
#define PROGRESS_STR_MAX_SIZE 50

const char *TEST_TASK = NULL;

struct test_stat {
    int run;
    int succeeded;
    int failed;
    float time;
};
typedef struct test_stat test_stat_t;

struct worker {
    int id;
    pid_t pid;
    int task_id;
};
typedef struct worker worker_t;

struct shared {
    int jobs_done;
    int jobs_tear_down_done;
    int tasks_done;
    sem_t lock;
};
typedef struct shared shared_t;

struct progress_info {
    char task[PROGRESS_STR_MAX_SIZE + 1];
    char test[PROGRESS_STR_MAX_SIZE + 1];
    pddl_timer_t timer;
    int in_progress;
};
typedef struct progress_info progress_info_t;

static void (*global_tear_down)(void) = NULL;

static int num_tasks;
static int num_tests;
static int num_enabled_tasks;
static int num_enabled_jobs;
static shared_t *shared = NULL;
static progress_info_t *progress_info = NULL;
static test_stat_t *test_stat = NULL;
static worker_t *worker;

static int verbose = 0;
static int no_progress = 0;
static int parallel = DEFAULT_PARALLEL;
static int timeout_s = DEFAULT_TIMEOUT;
static int supress_fail = 0;

static void setMemLimit(void)
{
    struct rlimit mem_limit;
    mem_limit.rlim_cur = mem_limit.rlim_max = 4096ul * 1024ul * 1024ul;
    setrlimit(RLIMIT_AS, &mem_limit);
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-a] [-v]"
                    " [-S task_substr] [-T task_name]"
                    " [-s test_substr] [-t test_name]"
                    " [-D] [-p num-parallel] [-m timeout-sec]"
                    " [-f]\n",
                    progname);
    fprintf(stderr, "  -a       Enable all tests\n");
    fprintf(stderr, "  -B       Enable base tasks\n");
    fprintf(stderr, "  -A       Enable all tasks\n");
    fprintf(stderr, "  -v       Increase logging (e.g., -vvv)\n");
    fprintf(stderr, "  -x       Turn off progress\n");
    fprintf(stderr, "  -S  str  Enable tasks containing str\n");
    fprintf(stderr, "  -T  str  Enable tasks exactly equal to str\n");
    fprintf(stderr, "  -s  str  Enable tests containing str\n");
    fprintf(stderr, "  -t  str  Enable tests exactly equal to str\n");
    fprintf(stderr, "  -D       Print tree of tasks and tests\n");
    fprintf(stderr, "  -L       Print tasks\n");
    fprintf(stderr, "  -K       Print tests\n");
    fprintf(stderr, "  -p  int  Run specified number of tasks in parallel"
            " (default: %d)\n", DEFAULT_PARALLEL);
    fprintf(stderr, "  -m  int  Timeout in seconds (default: %d)\n",
            DEFAULT_TIMEOUT);
    fprintf(stderr, "  -f       Suppress printing failures\n");
    exit(-1);
}

static void parseOptions(int argc, char *argv[])
{
    int print_plan = 0;
    int print_tasks = 0;
    int print_tests = 0;
    int opt;
    while ((opt = getopt(argc, argv, "haBAvxS:T:s:t:p:DLKm:f")) != -1) {
        switch (opt) {
            case 'a':
                tasksTestsEnableAllTests();
                break;
            case 'B':
                tasksTestsEnableAllTasks(1);
                break;
            case 'A':
                tasksTestsEnableAllTasks(0);
                break;
            case 'S':
                tasksTestsEnableTaskMatch(optarg);
                break;
            case 'T':
                tasksTestsEnableTaskEq(optarg);
                break;
            case 's':
                tasksTestsEnableTestMatch(optarg);
                break;
            case 't':
                tasksTestsEnableTestEq(optarg);
                break;
            case 'v':
                verbose++;
                break;
            case 'x':
                no_progress = 1;
                break;
            case 'p':
                parallel = atoi(optarg);
                break;
            case 'D':
                print_plan = 1;
                break;
            case 'L':
                print_tasks = 1;
                break;
            case 'K':
                print_tests = 1;
                break;
            case 'm':
                timeout_s = atoi(optarg);
                break;
            case 'f':
                supress_fail = 1;
                break;
            default:
                usage(argv[0]);
        }
    }
    if (optind != argc)
        usage(argv[0]);

    if (print_plan){
        printf("Enabled Tasks: %d / %d\n",
               tasksTestsNumEnabledTasks(),
               tasksTestsNumTasks());
        printf("Enabled Tests: %d / %d\n",
               tasksTestsNumEnabledTests(),
               tasksTestsNumTests());
        printf("Num jobs: %d\n", tasksTestsNumEnabledJobs());
        tasksTestsPrintPlan();
        exit(0);
    }

    if (print_tasks){
        tasksTestsPrintTasks();
        exit(0);
    }

    if (print_tests){
        tasksTestsPrintTests();
        exit(0);
    }

    if (parallel < 1)
        parallel = DEFAULT_PARALLEL;
    if (timeout_s < 1)
        timeout_s = DEFAULT_TIMEOUT;
}

static void updateTestStatRun(int test_id)
{
    sem_wait(&shared->lock);
    test_stat[test_id].run = 1;
    sem_post(&shared->lock);
}

static void updateTestStatTime(int test_id, float time)
{
    sem_wait(&shared->lock);
    test_stat[test_id].time += time;
    sem_post(&shared->lock);
}

static void updateTestStatFailed(int test_id)
{
    sem_wait(&shared->lock);
    test_stat[test_id].failed += 1;
    sem_post(&shared->lock);
}

static void updateTestStatSucceeded(int test_id)
{
    sem_wait(&shared->lock);
    test_stat[test_id].succeeded += 1;
    sem_post(&shared->lock);
}

static void progress(void)
{
    sem_wait(&shared->lock);
    int cnt = printf("Done: tasks %d / %d, jobs %d / %d, tear-down: %d / %d",
                     shared->tasks_done, num_enabled_tasks,
                     shared->jobs_done, num_enabled_jobs,
                     shared->jobs_tear_down_done, num_enabled_jobs);
    for (; cnt < PROGRESS_COLUMNS; ++cnt)
        printf(" ");
    printf("\n");
    for (int i = 0; i < parallel; ++i){
        if (progress_info[i].in_progress)
            pddlTimerStop(&progress_info[i].timer);
        cnt = printf("%d: %-50s :: %-50s :: %.2fs", i, progress_info[i].task,
                     progress_info[i].test,
                     pddlTimerElapsedInSF(&progress_info[i].timer));
        for (; cnt < PROGRESS_COLUMNS; ++cnt)
            printf(" ");
        printf("\n");
    }
    printf("\033[%dA", parallel + 1);
    fflush(stdout);
    sem_post(&shared->lock);
}

static void reportSignal(const char *task_name,
                         const char *test_name,
                         int status)
{
    sem_wait(&shared->lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated by signal %d (%s).\n",
            task_name, test_name,
            WTERMSIG(status), strsignal(WTERMSIG(status)));
    fflush(stdout);
    sem_post(&shared->lock);
}

static void reportAbnormal(const char *task_name, const char *test_name)
{
    sem_wait(&shared->lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated abnormaly!\n", task_name, test_name);
    fflush(stdout);
    sem_post(&shared->lock);
}

static void reportExitStatus(const char *task_name, const char *test_name,
                             int exit_status)
{
    sem_wait(&shared->lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated with exit status %d.\n",
            task_name, test_name, exit_status);
    fflush(stdout);
    sem_post(&shared->lock);
}

static void reportNonEmptyOut(const char *task_name, const char *test_name)
{
    sem_wait(&shared->lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: non-empty *.out.tmp file.\n",
            task_name, test_name);
    fflush(stdout);
    sem_post(&shared->lock);
}

static void reportDiffOut(const char *task_name, const char *test_name)
{
    sem_wait(&shared->lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: diff on .out files.\n", task_name, test_name);
    fflush(stdout);
    sem_post(&shared->lock);
}

static int fmtFilename(const char *task_name,
                       const char *test_name,
                       char *filename)
{
    int ret = sprintf(filename, "%s/%s", task_name, test_name);
    /*
    char *c = filename;
    for (; *c != 0x0; ++c){
        if (*c == '/'){
            *c = '-';
        }
    }
    */
    return ret;
}

static void fmtBaseFilename(const char *task,
                            const char *test_name,
                            const char *suff,
                            char *filename)
{
    int siz = sprintf(filename, "reg/");
    siz += fmtFilename(task, test_name, filename + siz);
    sprintf(filename + siz, ".%s", suff);
}

static void fmtOutputFilename(const char *task,
                              const char *test_name,
                              const char *suff,
                              char *filename)
{
    int siz = sprintf(filename, "reg/");
    siz += fmtFilename(task, test_name, filename + siz);
    sprintf(filename + siz, ".%s.tmp", suff);
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

static void redirectStdOutErr(const char *task_name,
                              const char *test_name,
                              int *fd_stdout,
                              int *fd_stderr)
{
    char stdout_fn[256];
    fmtOutputFilename(task_name, test_name, "out", stdout_fn);
    fflush(stdout);
    *fd_stdout = dup(fileno(stdout));
    if (freopen(stdout_fn, "w", stdout) == NULL){
        fprintf(stderr, "F: %s\n", stdout_fn);
        fflush(stderr);
        perror("Redirecting of stdout failed");
        exit(-1);
    }

    char stderr_fn[256];
    fmtOutputFilename(task_name, test_name, "err", stderr_fn);
    fflush(stderr);
    *fd_stderr = dup(fileno(stderr));
    if (freopen(stderr_fn, "w", stderr) == NULL){
        perror("Redirecting of stderr failed");
        exit(-1);
    }
}

static void restoreStdOutErr(int fd_stdout, int fd_stderr)
{
    fflush(stdout);
    dup2(fd_stdout, fileno(stdout));
    close(fd_stdout);
    clearerr(stdout);

    fflush(stderr);
    dup2(fd_stderr, fileno(stderr));
    close(fd_stderr);
    clearerr(stderr);
}

static void writeRet(const char *task_name, const char *test_name, int ret)
{
    char ret_fn[256];
    fmtOutputFilename(task_name, test_name, "ret", ret_fn);
    FILE *retout = fopen(ret_fn, "w");
    if (retout == NULL){
        perror("Opening .ret file failed");
        exit(-1);
    }
    fprintf(retout, "%d", ret);
    fclose(retout);
}

static void addFail(const char *task_name, const char *test_name, int status)
{
    char fail_fn[256];
    fmtOutputFilename(task_name, test_name, "fail", fail_fn);
    FILE *failout = fopen(fail_fn, "w");
    if (failout == NULL){
        perror("Opening file for failures failed");
        exit(-1);
    }
    fprintf(failout, "%s %s --> %d", task_name, test_name, WEXITSTATUS(status));
    if (!WIFEXITED(status)){
        if (WIFSIGNALED(status)){
            fprintf(failout, " | signal %d (%s)",
                    WTERMSIG(status), strsignal(WTERMSIG(status)));
        }else{
            fprintf(failout, " | terminated abnormaly");
        }
    }

    char fn[512];
    fmtOutputFilename(task_name, test_name, "out", fn);
    fprintf(failout, " | %s", fn);
    fmtOutputFilename(task_name, test_name, "err", fn);
    fprintf(failout, " | %s", fn);
    fprintf(failout, "\n");
    fflush(failout);
    fclose(failout);

    char base[512];
    fmtOutputFilename(task_name, test_name, "out", fn);
    fmtBaseFilename(task_name, test_name, "out", base);
    if (access(base, F_OK) == 0){
        char cmd[2048];
        sprintf(cmd, "diff -y --suppress-common-lines -W150 %s %s"
                     " | head -15 | awk '{print \"  >OUT>\", $0}' >>%s",
                base, fn, fail_fn);
        system(cmd);
    }else if (filesize(fn) > 0){
        char cmd[2048];
        sprintf(cmd, "cat %s | head -15 | awk '{print \"  >OUT>\", substr($0, 0, 150)}'>>%s",
                fn, fail_fn);
        system(cmd);
    }

    fmtOutputFilename(task_name, test_name, "err", fn);
    char cmd[2048];
    sprintf(cmd, "cat %s | head -15 | awk '{print \"  >ERR>\", substr($0, 0, 150)}'>>%s",
            fn, fail_fn);
    system(cmd);
}

static void tearDown(const test_def_t *t)
{
    if (t->fn_tear_down != NULL)
        t->fn_tear_down();
    if (t->parent >= 0)
        tearDown(tasksTestsGetTest(t->parent));
}

static void *thTimeout(void *_)
{
    sleep(timeout_s);
    kill(getpid(), SIGALRM);
    return NULL;
}

static void runTest(worker_t *worker, int task_id, const test_def_t *test)
{
    if (verbose == 0 && !no_progress)
        progress();
    fflush(stdout);
    fflush(stderr);
    int pid = fork();
    if (pid < 0){
        perror("Fork error");
        exit(-1);

    }else if (pid == 0){
        TEST_TASK = tasksTestsGetTaskName(task_id);

        int fd_stdout, fd_stderr;
        redirectStdOutErr(tasksTestsGetTaskName(task_id), test->name,
                          &fd_stdout, &fd_stderr);

        pddl_timer_t timer;
        pddlTimerStart(&timer);
        updateTestStatRun(test->id);

        pthread_t thtimeout;
        pthread_create(&thtimeout, NULL, thTimeout, NULL);

        sem_wait(&shared->lock);
        strncpy(progress_info[worker->id].task, TEST_TASK, PROGRESS_STR_MAX_SIZE);
        strncpy(progress_info[worker->id].test, test->name, PROGRESS_STR_MAX_SIZE);
        pddlTimerStart(&progress_info[worker->id].timer);
        progress_info[worker->id].in_progress = 1;
        sem_post(&shared->lock);
        test->fn();

        pthread_cancel(thtimeout);
        pthread_join(thtimeout, NULL);

        pddlTimerStop(&timer);
        updateTestStatTime(test->id, pddlTimerElapsedInSF(&timer));

        restoreStdOutErr(fd_stdout, fd_stderr);
        if (verbose > 3){
            sem_wait(&shared->lock);
            fprintf(stderr, "Time of test %s on task %s: %.2fs\n",
                    test->name, TEST_TASK, pddlTimerElapsedInSF(&timer));
            fflush(stderr);
            sem_post(&shared->lock);
        }

        sem_wait(&shared->lock);
        shared->jobs_done += 1;
        pddlTimerStop(&progress_info[worker->id].timer);
        progress_info[worker->id].in_progress = 0;
        sem_post(&shared->lock);

        for (int i = 0; i < test->children_size; ++i){
            if (!tasksTestsIsEnabled(task_id, test->children[i]))
                continue;
            runTest(worker, task_id, tasksTestsGetTest(test->children[i]));
        }

        if (verbose > 3){
            sem_wait(&shared->lock);
            fprintf(stderr, "Tear-Down of test %s on task %s running in"
                    " process %d (global tear down: %s)\n",
                    test->name, TEST_TASK, (int)getpid(),
                    (global_tear_down != NULL ? "yes" : "no"));
            fflush(stderr);
            sem_post(&shared->lock);
        }
        tearDown(test);
        if (global_tear_down != NULL)
            global_tear_down();

        sem_wait(&shared->lock);
        shared->jobs_tear_down_done += 1;
        sem_post(&shared->lock);
        exit(0);

    }else{
        if (verbose > 2){
            sem_wait(&shared->lock);
            fprintf(stderr, "Test %s on task %s running in process %d\n",
                    test->name, TEST_TASK, (int)pid);
            fflush(stderr);
            sem_post(&shared->lock);
        }

        int status;
        wait(&status);

        const char *task_name = tasksTestsGetTaskName(task_id);
        int failed = 0;
        if (!WIFEXITED(status)){ /* if child process ends up abnormaly */
            if (WIFSIGNALED(status)){
                reportSignal(task_name, test->name, status);
            }else{
                reportAbnormal(task_name, test->name);
            }
            writeRet(task_name, test->name, 1);
            failed = 1;

        }else{
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0){
                reportExitStatus(task_name, test->name, exit_status);
                failed = 1;
            }
            writeRet(task_name, test->name, exit_status);
        }

        char fn[512];
        fmtOutputFilename(task_name, test->name, "out", fn);

        char base[512];
        fmtBaseFilename(task_name, test->name, "out", base);
        if (access(base, F_OK) == 0){
            char cmd[2048];
            sprintf(cmd, "diff -q %s %s", base, fn);
            int ret = system(cmd);
            if (ret > 0){
                reportDiffOut(task_name, test->name);
                failed = 1;
            }

        }else if (filesize(fn) > 0){
            reportNonEmptyOut(task_name, test->name);
            failed = 1;
        }

        if (failed){
            addFail(task_name, test->name, status);
            updateTestStatFailed(test->id);
        }else{
            updateTestStatSucceeded(test->id);
        }
    }
}

static int _runWorker(worker_t *worker, int task_id)
{
    worker->task_id = task_id;
    pid_t pid = fork();

    if (pid < 0){
        perror("Fork error");
        exit(-1);

    }else if (pid == 0){
        for (int ti = 0; ti < num_tests; ++ti){
            const test_def_t *test = tasksTestsGetTest(ti);
            assert(test->id == ti);
            if (test->parent < 0 && tasksTestsIsEnabled(task_id, ti)){
                runTest(worker, task_id, test);
            }
        }

        exit(0);

    }else{ // pid > 0
        if (verbose >= 1){
            sem_wait(&shared->lock);
            printf("Task %s | worker: %d, pid: %d\n",
                   tasksTestsGetTaskName(task_id), worker->id, (int)pid);
            fflush(stdout);
            sem_post(&shared->lock);
        }
        worker->pid = pid;
        return 1;
    }
}

static int runWorker(worker_t *worker, int task_id)
{
    for (int i = 0; i < parallel; ++i){
        if (worker[i].pid == -1)
            return _runWorker(worker + i, task_id);
    }
    assert(0);
}

static int _waitForWorker(worker_t *worker, int status)
{
    assert(worker->task_id >= 0);
    assert(worker->pid >= 0);
    if (verbose >= 1){
        sem_wait(&shared->lock);
        printf("Task %s | worker: %d, pid: %d | DONE\n",
               tasksTestsGetTaskName(worker->task_id), worker->id,
               (int)worker->pid);
        fflush(stdout);
        sem_post(&shared->lock);
    }
    worker->pid = -1;
    worker->task_id = -1;
    return 0;
}

static int waitForWorker(worker_t *worker)
{
    int status;
    pid_t pid = wait(&status);
    for (int i = 0; i < parallel; ++i){
        if (worker[i].pid == pid){
            return _waitForWorker(worker + i, status);
        }
    }
    assert(0);
    return 0;
}

static void printReportFailures(void)
{
    system("find reg/ -name '*.fail.tmp' | sort | xargs -n1 cat");
}

static void printReportTest(const char *test_name,
                            const test_stat_t *stat,
                            int name_len,
                            int succ_len,
                            int fail_len)
{
    printf("%s", test_name);
    for (int i = strlen(test_name); i < name_len; ++i)
        printf(" ");

    printf(" | ");
    printf("%*i", succ_len, stat->succeeded);

    printf(" | ");
    printf("%*i", fail_len, stat->failed);

    printf(" | ");
    printf("%7.2fs", stat->time);

    printf("\n");
}

static void printReport(void)
{
    int name_len = 5;
    int num_succeeded = 0;
    int num_failed = 0;
    float time = 0;
    for (int i = 0; i < num_tests; ++i){
        const test_def_t *test = tasksTestsGetTest(i);
        if (!test_stat[i].run)
            continue;
        if (strlen(test->name) > name_len)
            name_len = strlen(test->name);
        num_succeeded += test_stat[i].succeeded;
        num_failed += test_stat[i].failed;
        time += test_stat[i].time;
    }

    int succ_len = 4;
    int n = num_succeeded;
    int s = 1;
    while ((n = n / 10))
        ++s;
    if (s > succ_len)
        succ_len = s;

    int fail_len = 4;
    n = num_failed;
    s = 1;
    while ((n = n / 10))
        ++s;
    if (s > fail_len)
        fail_len = s;

    printf("\n");
    for (int i = 0; i < name_len; ++i)
        printf(" ");
    printf(" | succ | fail | time\n");
    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("------------------\n");

    for (int i = 0; i < num_tests; ++i){
        if (!test_stat[i].run)
            continue;
        const test_def_t *test = tasksTestsGetTest(i);
        printReportTest(test->name, test_stat + i, name_len, succ_len, fail_len);
    }

    for (int i = 0; i < name_len + succ_len + fail_len; ++i)
        printf("-");
    printf("------------------\n");
    printf("sum");
    for (int i = 3; i < name_len; ++i)
        printf(" ");

    printf(" | ");
    printf("%*i", succ_len, num_succeeded);

    printf(" | ");
    printf("%*i", fail_len, num_failed);
    printf(" | %7.2fs", time);
    printf("\n");

    fflush(stdout);
    fflush(stderr);

    if (!supress_fail)
        printReportFailures();
}

static void cleanRegDir(void)
{
    system("find reg/ -name '*.tmp' -printf 'Cleaning reg/: %-100p\r' -exec rm '{}' ';'");
    printf("\n");
}

int main(int argc, char *argv[])
{
    tasksTestsInit();
    setMemLimit();
    parseOptions(argc, argv);

    cleanRegDir();
    global_tear_down = tasksTestsGlobalTearDown();

    num_tasks = tasksTestsNumTasks();
    num_tests = tasksTestsNumTests();
    num_enabled_tasks = tasksTestsNumEnabledTasks();
    num_enabled_jobs = tasksTestsNumEnabledJobs();

    printf("tasks: %d/%d, tests: %d/%d, jobs: %d, parallel: %d, timeout: %ds\n",
           tasksTestsNumEnabledTasks(),
           tasksTestsNumTasks(),
           tasksTestsNumEnabledTests(),
           tasksTestsNumTests(),
           tasksTestsNumEnabledJobs(),
           parallel, timeout_s);
    fflush(stdout);

    size_t shared_size = 0;
    shared_size += sizeof(shared_t);
    shared_size += sizeof(progress_info_t) * parallel;
    shared_size += sizeof(test_stat_t) * num_tests;
    void *shared_mem = mmap(NULL, shared_size, PROT_WRITE | PROT_READ,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shared_mem != NULL);
    bzero(shared_mem, shared_size);
    shared = shared_mem;
    progress_info = (progress_info_t *)(shared + 1);
    test_stat = (test_stat_t *)(progress_info + parallel);

    int ret;
    ret = sem_init(&shared->lock, 1, 1);
    assert(ret == 0);

    worker = alloca(sizeof(worker_t) * parallel);
    for (int w = 0; w < parallel; ++w){
        worker[w].id = w;
        worker[w].pid = -1;
    }

    int num_active_workers = 0;
    for (int task_id = 0; task_id < num_tasks; ++task_id){
        if (!tasksTestsTaskIsEnabled(task_id))
            continue;

        if (num_active_workers == parallel){
            waitForWorker(worker);
            --num_active_workers;
        }
        num_active_workers += runWorker(worker, task_id);

        sem_wait(&shared->lock);
        shared->tasks_done += 1;
        sem_post(&shared->lock);
    }

    while (num_active_workers > 0){
        waitForWorker(worker);
        --num_active_workers;
    }

    printReport();

    int exit_code = 0;
    for (int i = 0; i < num_tests; ++i){
        if (test_stat[i].failed){
            exit_code = 1;
            break;
        }
    }

    ret = sem_destroy(&shared->lock);
    assert(ret == 0);
    ret = munmap(shared_mem, shared_size);
    assert(ret == 0);

    return exit_code;
}
