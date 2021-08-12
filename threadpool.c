#include "threadpool.h"

#include<pthread.h>
#include<stdlib.h>
#include<assert.h>

struct executor {
    executor_task *task;
    void *aux;
    enum executor_status status;
    pthread_t tid;
    bool in_pool;
};

struct pool {
    int max_size;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t ok_to_append;
}*pool;

void pool_init (int max_size) {

    assert (max_size > 0);

    struct pool *pool = (struct pool *) malloc ( sizeof(struct pool) );
    pool->max_size = max_size;
    pool->size = 0;
    pthread_mutex_init (&pool->lock, NULL);
    pthread_cond_init (&pool->ok_to_append, NULL);

    return pool;
}

void pool_resize (int new_size) {
    assert (new_size > 0);

    pthread_mutex_lock (&pool->lock);
    pool->max_size = new_size;
    pthread_mutex_unlock (&pool->lock);

}

static void pool_append (struct executor *executor) {
    pthread_mutex_lock (&pool->lock);
    while (pool->size >= pool->max_size) {
        pthread_cond_wait (&pool->ok_to_append, &pool->lock);
    }
    pool->size++;
    executor->in_pool = true;
    pthread_mutex_unlock (&pool->lock);
}

static void pool_pop (struct executor *executor) {
    assert (executor != NULL);
    assert (executor->in_pool);

    pthread_mutex_lock (&pool->lock);
    assert (pool->size > 0);
    pool->size--;
    executor->in_pool = false;
    if (pool->size < pool->max_size)
        pthread_cond_signal (&pool->ok_to_append);
    pthread_mutex_unlock (&pool->lock);
}

struct executor *executor_init (executor_task *task, void *aux) {
    if (task == NULL)
        return NULL;

    /* Initialize an executor */
    struct executor *executor = (struct executor *) malloc ( sizeof(struct executor) );
    if (executor == NULL)
        return NULL;

    executor->task = task;
    executor->aux = aux;
    executor->status = EXECUTOR_BLOCKED;
    executor->tid = -1;
    executor->in_pool = false;

    return executor;
}

static void *executor_entry (void *aux) {
    struct executor *executor = (struct executor *)aux;
    pool_append (executor);
    executor->status = EXECUTOR_RUNNING;
    executor->task(executor->aux);
    executor->status = EXECUTOR_DYING;
    pool_pop (executor);
}

bool executor_start (struct executor *executor) {
    assert (executor != NULL);

    if (pthread_create (&executor->tid, NULL, executor_entry, (void *)executor ) ) 
        return false; 
    else
        return true;
}

void executor_exit (struct executor *executor) {
    assert ( executor != NULL && executor->tid != -1 );

    pthread_join (executor->tid, NULL);
    free (executor);
}

enum executor_status executor_status (struct executor *executor) {
    assert (executor != NULL);

    return executor->status;
}