#include "thread_pool.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HTTPO_WTRQ_CAP 32

static bool wtrq_enqueue(WorkerThreadRequestQueue* queue, WorkerData* data) {
    if (queue->size != 0 && queue->write == queue->read) {
        return false;
    }

    assert(pthread_mutex_lock(&queue->mutex) == 0);

    if (queue->size == queue->cap) {
        queue->cap *= 2;
        queue->items = realloc(queue->items, sizeof(WorkerData*) * queue->cap);
    }

    queue->items[queue->write] = data;
    queue->write = (queue->write + 1) % queue->cap;
    queue->size++;

    assert(pthread_mutex_unlock(&queue->mutex) == 0);
    return true;
}

static WorkerData* wtrq_dequeue(WorkerThreadRequestQueue* queue) {
    assert(pthread_mutex_lock(&queue->mutex) == 0);
    assert(atomic_load(&queue->size) != 0);

    WorkerData* result = queue->items[queue->read];
    queue->read = (queue->read + 1) % queue->cap;
    queue->size--;

    assert(pthread_mutex_unlock(&queue->mutex) == 0);
    return result;
}

typedef struct {
    WorkerThread* thread;
} WorkerInitData;

static void* threadpool_worker(void* worker_data) {
    WorkerInitData* init_data = (WorkerInitData*)worker_data;
    WorkerThreadRequestQueue* queue = &init_data->thread->queue;

    while (true) {
        if (atomic_load(&queue->size) == 0) {
            continue;
        }

        WorkerData* data = wtrq_dequeue(queue);
        data->proc(data->arg);
        free(data);
    }

    return NULL;
}

ThreadPool threadpool_init(size_t count) {
    WorkerThread* threads = malloc(count * sizeof(WorkerThread));
    for (size_t i = 0; i < count; i++) {
        WorkerInitData* data = malloc(sizeof(WorkerInitData));
        data->thread = &threads[i];
        WorkerThreadRequestQueue queue = {0};
        queue.cap = HTTPO_WTRQ_CAP;
        queue.items = malloc(sizeof(WorkerData*) * queue.cap);

        pthread_mutexattr_t muattr;
        pthread_mutexattr_init(&muattr);
        pthread_mutexattr_setrobust(&muattr, PTHREAD_MUTEX_ROBUST);
        pthread_mutexattr_settype(&muattr, PTHREAD_MUTEX_ERRORCHECK);

        assert(pthread_mutex_init(&queue.mutex, &muattr) == 0);
        data->thread->queue = queue;

        int status = pthread_create(&threads[i].handle, NULL, threadpool_worker, data);
        assert(status == 0);
    }

    return (ThreadPool){
        .threads = threads,
        .count = count,
    };
}

void threadpool_schedule(ThreadPool* thread_pool, WorkerProc proc, void* arg) {
    int thread_idx = 0;

    for (size_t i = 0; i < thread_pool->count; i++) {
        if (thread_pool->threads[i].queue.size < thread_pool->threads[thread_idx].queue.size) {
            thread_idx = i;
        }
    }

    WorkerThread* thread = &thread_pool->threads[thread_idx];
    WorkerData* data = malloc(sizeof(WorkerData));
    data->thread = thread;
    data->proc = proc;
    data->arg = arg;

    while (!wtrq_enqueue(&thread->queue, data)) {
        // wait until the queue has free space
    }
}

void threadpool_free(ThreadPool const* thread_pool) {
    free(thread_pool->threads);
}
