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

#define MAX_PROGRESS_STEPS 30
#define DEFAULT_PARALLEL 6
#define DEFAULT_TIMEOUT 60

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

static void (*global_tear_down)(void) = NULL;

static int num_tasks;
static int num_tests;
static int *progress_step;
static int *tasks_processed;
static sem_t *lock;
static test_stat_t *test_stat = NULL;
static worker_t *worker;

static int verbose = 0;
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
    fprintf(stderr, "  -a       Use all tasks\n");
    fprintf(stderr, "  -v       Increase logging\n");
    fprintf(stderr, "  -S  str  Restrict tasks to those containing str\n");
    fprintf(stderr, "  -T  str  Restrict tasks to exactly str\n");
    fprintf(stderr, "  -s  str  Restrict tests to those containing str\n");
    fprintf(stderr, "  -t  str  Restrict tests to exactly str\n");
    fprintf(stderr, "  -D       Print tree of tasks and tests\n");
    fprintf(stderr, "  -p  int  Run specified number of tasks in parallel"
            " (default: %d)\n", DEFAULT_PARALLEL);
    fprintf(stderr, "  -m  int  Timeout in seconds (default: %d)\n",
            DEFAULT_TIMEOUT);
    fprintf(stderr, "  -f       Suppress printing failures\n");
    exit(-1);
}

static void parseOptions(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "havS:T:s:t:p:Dm:f")) != -1) {
        switch (opt) {
            case 'a':
                tasksTestsSelectAll();
                break;
            case 'S':
                tasksTestsSelectTasksMatch(optarg);
                break;
            case 'T':
                tasksTestsSelectTask(optarg);
                break;
            case 's':
                tasksTestsSelectTestsMatch(optarg);
                break;
            case 't':
                tasksTestsSelectTest(optarg);
                break;
            case 'v':
                verbose++;
                break;
            case 'p':
                parallel = atoi(optarg);
                break;
            case 'D':
                tasksTestsPrintPlan();
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

    if (parallel < 1)
        parallel = DEFAULT_PARALLEL;
    if (timeout_s < 1)
        timeout_s = DEFAULT_TIMEOUT;
}

static void updateTestStatRun(int test_id)
{
    sem_wait(lock);
    test_stat[test_id].run = 1;
    sem_post(lock);
}

static void updateTestStatTime(int test_id, float time)
{
    sem_wait(lock);
    test_stat[test_id].time += time;
    sem_post(lock);
}

static void updateTestStatFailed(int test_id)
{
    sem_wait(lock);
    test_stat[test_id].failed += 1;
    sem_post(lock);
}

static void updateTestStatSucceeded(int test_id)
{
    sem_wait(lock);
    test_stat[test_id].succeeded += 1;
    sem_post(lock);
}

static void progress(void)
{
    sem_wait(lock);
    int i;
    fprintf(stdout, "%d/%d", *tasks_processed, num_tasks);
    for (i = 0; i < *progress_step; ++i)
        fprintf(stdout, "*");
    for (; i < MAX_PROGRESS_STEPS; ++i)
        fprintf(stdout, " ");
    fprintf(stdout, "\r");
    fflush(stdout);
    (*progress_step) = ((*progress_step) + 1) % MAX_PROGRESS_STEPS;
    sem_post(lock);
}

static void reportSignal(const char *task_name,
                         const char *test_name,
                         int status)
{
    sem_wait(lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated by signal %d (%s).\n",
            task_name, test_name,
            WTERMSIG(status), strsignal(WTERMSIG(status)));
    fflush(stdout);
    sem_post(lock);
}

static void reportAbnormal(const char *task_name, const char *test_name)
{
    sem_wait(lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated abnormaly!\n", task_name, test_name);
    fflush(stdout);
    sem_post(lock);
}

static void reportExitStatus(const char *task_name, const char *test_name,
                             int exit_status)
{
    sem_wait(lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: terminated with exit status %d.\n",
            task_name, test_name, exit_status);
    fflush(stdout);
    sem_post(lock);
}

static void reportNonEmptyOut(const char *task_name, const char *test_name)
{
    sem_wait(lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: non-empty tmp.*.out file.\n", task_name, test_name);
    fflush(stdout);
    sem_post(lock);
}

static void reportDiffOut(const char *task_name, const char *test_name)
{
    sem_wait(lock);
    fprintf(stdout, "%s / %s \x1b[31mFAILED\x1b[0m: diff on .out files.\n", task_name, test_name);
    fflush(stdout);
    sem_post(lock);
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

static void runTest(int task_id, const test_def_t *test)
{
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

        test->fn();

        pthread_cancel(thtimeout);
        pthread_join(thtimeout, NULL);

        pddlTimerStop(&timer);
        updateTestStatTime(test->id, pddlTimerElapsedInSF(&timer));

        restoreStdOutErr(fd_stdout, fd_stderr);

        for (int i = 0; i < test->children_size; ++i){
            if (!tasksTestsIsEnabled(task_id, test->children[i]))
                continue;
            runTest(task_id, tasksTestsGetTest(test->children[i]));
        }

        tearDown(test);
        if (global_tear_down != NULL)
            global_tear_down();
        exit(0);

    }else{
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
                runTest(task_id, test);
            }
        }

        exit(0);

    }else{ // pid > 0
        if (verbose >= 1){
            sem_wait(lock);
            printf("Task %s | worker: %d, pid: %d\n",
                   tasksTestsGetTaskName(task_id), worker->id, (int)pid);
            fflush(stdout);
            sem_post(lock);
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
        sem_wait(lock);
        printf("Task %s | worker: %d, pid: %d | DONE\n",
               tasksTestsGetTaskName(worker->task_id), worker->id,
               (int)worker->pid);
        fflush(stdout);
        sem_post(lock);
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
    system("find reg/ -name '*.fail' | sort | xargs -n1 cat");
    /*
    DIR *dp;
    struct dirent *ep;     
    dp = opendir ("reg/");

    if (dp != NULL){
        while ((ep = readdir(dp)) != NULL){
            if (strncmp(ep->d_name, "tmp.", 4) == 0){
                int len = strlen(ep->d_name);
                if (strncmp(ep->d_name + len - 5, ".fail", 5) == 0){
                    char f[512];
                    sprintf(f, "reg/%s", ep->d_name);
                    char buf[512];
                    FILE *fin = fopen(f, "r");
                    if (fin == NULL){
                        perror("Could not open fail file!");
                        exit(-1);
                    }
                    size_t r = fread(buf, 1, 512, fin);
                    while (r > 0){
                        fwrite(buf, 1, r, stdout);
                        r = fread(buf, 1, 512, fin);
                    }
                    fclose(fin);
                }
            }
        }
        closedir(dp);
    }
    */
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

int main(int argc, char *argv[])
{
    tasksTestsInit();
    setMemLimit();
    parseOptions(argc, argv);

    cleanRegDir();
    global_tear_down = tasksTestsGlobalTearDown();

    num_tasks = tasksTestsNumTasks();
    num_tests = tasksTestsNumTests();

    printf("tasks: %d/%d, tests: %d, parallel: %d, timeout: %ds\n",
           tasksTestsNumActiveTasks(), num_tasks, num_tests,
           parallel, timeout_s);
    fflush(stdout);

    size_t shared_size = 0;
    shared_size += sizeof(int);
    shared_size += sizeof(int);
    shared_size += sizeof(sem_t);
    shared_size += sizeof(test_stat_t) * num_tests;
    void *shared_mem = mmap(NULL, shared_size, PROT_WRITE | PROT_READ,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shared_mem != NULL);
    bzero(shared_mem, shared_size);
    progress_step = shared_mem;
    tasks_processed = (int *)(progress_step + 1);
    lock = (sem_t *)(tasks_processed + 1);
    test_stat = (test_stat_t *)(lock + 1);

    int ret;
    ret = sem_init(lock, 1, 1);
    assert(ret == 0);

    worker = alloca(sizeof(worker_t) * parallel);
    for (int w = 0; w < parallel; ++w){
        worker[w].id = w;
        worker[w].pid = -1;
    }

    int num_active_workers = 0;
    for (int task_id = 0; task_id < num_tasks; ++task_id){
        sem_wait(lock);
        *tasks_processed += 1;
        sem_post(lock);
        if (!tasksTestsIsEnabled(task_id, -1))
            continue;

        if (num_active_workers == parallel){
            waitForWorker(worker);
            --num_active_workers;
        }
        num_active_workers += runWorker(worker, task_id);
    }

    while (num_active_workers > 0){
        waitForWorker(worker);
        --num_active_workers;
    }

    printReport();

    ret = sem_destroy(lock);
    assert(ret == 0);
    ret = munmap(shared_mem, shared_size);
    assert(ret == 0);

    return 0;
}
