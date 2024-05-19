#include <pthread.h>
#include <stdatomic.h>

typedef void* (*WorkerProc)(void*);

struct WorkerThread;

typedef struct {
    struct WorkerThread* thread;
    WorkerProc proc;
    void* arg;
} WorkerData;


typedef struct WTRNode {
    WorkerData* data;
    struct WTRNode* next;
} WTRNode;

typedef struct {
    WTRNode* head;
    WTRNode* tail;
    atomic_size_t size;

    pthread_mutex_t mutex;
} WorkerThreadRequestQueue;

typedef struct WorkerThread {
    pthread_t handle;
    WorkerThreadRequestQueue queue;
} WorkerThread;

typedef struct {
    WorkerThread* threads;
    size_t count;
} ThreadPool;

ThreadPool threadpool_init(size_t count);
void threadpool_schedule(ThreadPool* thread_pool, WorkerProc proc, void* arg);
void threadpool_free(ThreadPool const* thread_pool);
