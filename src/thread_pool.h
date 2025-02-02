#include <pthread.h>
#include <stdatomic.h>

typedef void* (*WorkerProc)(void*);

struct WorkerThread;

typedef struct {
    struct WorkerThread* thread;
    WorkerProc proc;
    void* arg;
} WorkerData;

typedef struct {
    WorkerData** items;
    atomic_size_t size;
    atomic_size_t cap;
    atomic_size_t write;
    atomic_size_t read;

    pthread_mutex_t mutex;
} WorkerThreadRequestQueue;

typedef struct WorkerThread {
    pthread_t handle;
    WorkerThreadRequestQueue queue;
    pthread_cond_t cond_var;
    pthread_mutex_t cond_mu;
} WorkerThread;

typedef struct {
    WorkerThread* threads;
    size_t count;
} ThreadPool;

ThreadPool threadpool_init(size_t count);
void threadpool_schedule(ThreadPool* thread_pool, WorkerProc proc, void* arg);
void threadpool_free(ThreadPool const* thread_pool);
