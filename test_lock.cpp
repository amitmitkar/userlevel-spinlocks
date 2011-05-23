#include "spinlock.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <string>
#include <sstream>
#include <vector>

const uint32_t MAX_THREADS = 32;

using namespace std;

/* Limits the number of operations.
 * Change rsh value to dilate or constrict
 * the total run time of the program. */ 
const uint32_t TMAXCOUNT = 4294967294UL >> 10;
bool tas = false;
int cache_line = 64;

/*Per thread context structure.*/
struct context{
    string       id;
    spin_lock   *spl;
    uint32_t    *count;
    uint32_t    ops;
    uint64_t    lock_ns;
    uint64_t    unlock_ns;
    struct itimerval itimer;
    struct rusage ru;
};

volatile bool thread_start = false;

uint32_t count = 0;
uint32_t nthreads = 0;
spin_lock spl __attribute__((aligned(128)));
pthread_cond_t start_signal;
pthread_mutex_t start_mut;

int thr_start_count = 0;
pthread_cond_t  all_threads_started;

void wait_for_start(int nthreads)
{
    pthread_mutex_lock(&start_mut);
    thr_start_count++;
    while(1) {
        if (thr_start_count == nthreads)
            pthread_cond_broadcast(&all_threads_started);

        pthread_cond_wait(&start_signal, &start_mut);
        if (thread_start) {
            break;
        }
    }
    pthread_mutex_unlock(&start_mut);
}

#define do_thr_func(locktype)  do {                 \
    wait_for_start(nthreads);                       \
    struct context *pctx = (struct context*)parg;   \
    pctx->lock_ns = pctx->unlock_ns = 0;            \
    while (1) {                                     \
        spl.locktype(&pctx->id);                    \
        if (*(pctx->count) > TMAXCOUNT) {           \
            spl.unlock(&pctx->id);                  \
            break;                                  \
        }                                           \
        (*(pctx->count))++;                         \
        pctx->ops++;                                \
        spl.unlock(&pctx->id);                      \
    }                                               \
    getrusage(RUSAGE_THREAD, &pctx->ru);            \
    return NULL;                                    \
} while(0)


/* Common thread func to test the spinlock implementation. */
void *
thread_func_ttas(void *parg)
{
#ifdef PROF_ENABLED
    setitimer(ITIMER_PROF, &(((struct context *)parg)->itimer), NULL);
#endif
    do_thr_func(lock_ttas);
}

/* Common thread func to test the spinlock implementation. */
void *
thread_func_tas(void *parg)
{
#ifdef PROF_ENABLED
    setitimer(ITIMER_PROF, &(((struct context *)parg)->itimer), NULL);
#endif
    do_thr_func(lock_tas);
}

void usage(void)
{
    printf("USAGE:\n"
           "test_lock <tas|ttas> num_threads\n");
}

void init_threads(const int &ncpus,
                  vector<pthread_t> &tvec,
                  vector<pthread_attr_t> &avec,
                  vector<struct context> &cvec)
{
    unsigned int i;
    pthread_mutex_init(&start_mut, NULL);
    pthread_cond_init(&start_signal, NULL);
    pthread_cond_init(&all_threads_started, NULL);
    struct context ctx;
    ctx.spl= &spl;
    ctx.count = &count;
    ctx.ops = 0;


    for(i=0; i < tvec.size(); ++i) {
        ostringstream str;
        str << "thread " << i;
        ctx.id = str.str();
        cpu_set_t cset;
        pthread_attr_t attr;
        CPU_ZERO(&cset);
        CPU_SET(i % ncpus, &cset);
        pthread_attr_init(&attr);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cset);
        avec[i] = attr;
        cvec[i] = ctx;
    }

    /* Now hold the mutex and start the threads so that they'll all be started up 
     * and waiting for the start_signal.
     * This way threads started late will be equally likely to obtain the spinlock.*/
    for(i=0; i < tvec.size(); ++i) {
#ifdef PROF_ENABLED
        getitimer(ITIMER_PROF, &(cvec[i].itimer));
#endif
        if (tas)
            pthread_create(&tvec[i], &avec[i], thread_func_tas, &cvec[i]);
        else
            pthread_create(&tvec[i], &avec[i], thread_func_ttas, &cvec[i]);
    }
}

void start_threads(int nthreads)
{
   pthread_mutex_lock(&start_mut);
    while (1) {
        if (thr_start_count < nthreads)
            pthread_cond_wait(&all_threads_started, &start_mut);
        else
            break;
    }
    thread_start = true;
    pthread_cond_broadcast(&start_signal);
    pthread_mutex_unlock(&start_mut);
}


void wait_for_threads(vector<pthread_t> &tvec)
{
    unsigned int i = 0;
    printf("%d threads started \n", thr_start_count);
    for(i=0; i < tvec.size(); ++i)
        pthread_join(tvec[i], NULL);
}

void print_stats(int ncpus, vector<struct context> cvec)
{
    unsigned long avg_tvsec = 0;
    unsigned long avg_tvusec = 0;
    uint32_t totops = 0;
    

    printf( "NCPUS : %u\n"
            "CLSZ  : %d\n"
            "MXCnt : %u\n"
            "Count : %u\n"
            ,ncpus, cache_line, TMAXCOUNT, count);
    
    printf("---------\n");

    for (unsigned int i=0; i < cvec.size(); ++i) {
        printf( "Id : %s\n"
                "Ops: %u\n"
                "USG: %lu.%lu\n"
                "VCS: %ld\n"
                "ICS: %ld\n---------\n"
                ,cvec[i].id.c_str(),
                cvec[i].ops, cvec[i].ru.ru_utime.tv_sec, cvec[i].ru.ru_utime.tv_usec,
                cvec[i].ru.ru_nvcsw,
                cvec[i].ru.ru_nivcsw);
                avg_tvsec += cvec[i].ru.ru_utime.tv_sec;
                avg_tvusec += cvec[i].ru.ru_utime.tv_usec;
                totops += cvec[i].ops;
    }

    avg_tvsec /= cvec.size();
    avg_tvusec /= cvec.size();

    if (totops != count) {
        printf("TEST FAILED ops %u != count %u\n", totops, count);
        exit(3);
    }
        
    printf("AVERAGE USAGE %lu.%lu\n", avg_tvsec, avg_tvusec);
}


int main(int argc, char **argv)
{

    char *mode;

    if (argc < 3 || !(mode = strstr(argv[1], "tas"))) {
        usage();
        exit(1);
    }

    tas = (mode == argv[1]);

    int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    nthreads = strtol(argv[2], NULL, 10);
    if (errno) {
        int save_errno = errno;
        printf("Failed to convert %s to an integer. Errno %d", argv[2], save_errno);
        exit(save_errno);
    }

    if ( nthreads > MAX_THREADS || nthreads == 0 ) {
        printf("Only 1-%d threads allowed\n", MAX_THREADS);
        exit(EINVAL);
    }
    

    vector<struct context>  ctx_vec(nthreads);
    vector<pthread_t>       thr_vec(nthreads);
    vector<pthread_attr_t>  attr_vec(nthreads);

    init_threads(ncpus, thr_vec, attr_vec, ctx_vec); 
    printf("using %stas locking\n", (tas ? "":"t-"));
    start_threads(nthreads); 
    wait_for_threads(thr_vec);
    print_stats(ncpus, ctx_vec);

    return 0;
}
