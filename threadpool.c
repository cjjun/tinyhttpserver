#include "threadpool.h"
#include "lib/list.h"

#include<pthread.h>
#include<stdlib.h>
#include<assert.h>
#include<stdio.h>

static struct list list_all;
static uint32_t alloc_id = 1;

/* Executor list lock*/
static pthread_mutex_t list_lock;

/* Struct for executor */
struct executor {
    executor_t eid;
    executor_task *task;
    void *aux;
    enum executor_status status;
    pthread_t tid;
    bool in_pool;

    /* List management entry */
    struct list_elem elem;
};

static struct executor *get_executor_by_id (executor_t eid) {
    struct executor *ret = NULL;
    struct list_elem *it;

    pthread_mutex_lock (&list_lock);
    for (it = list_begin (&list_all); it != list_end (&list_all); it = list_next (&list_all)) {
        ret = list_entry (it, struct executor, elem);
        if (ret->eid == eid)
            break;
    }
    if (it == list_end (&list_all))
        ret = NULL;
    pthread_mutex_unlock (&list_lock);
    return ret;
}

/* Struct for global thread pool */
struct pool {
    int max_size;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t ok_to_append;
}pool;

/* Initialize a thread pool with max_size */
void pool_init (int max_size) {

    assert (max_size > 0);

    pool.max_size = max_size;
    pool.size = 0;

    pthread_mutex_init (&pool.lock, NULL);
    pthread_cond_init (&pool.ok_to_append, NULL);

    /* Executor management */
    list_init (&list_all);
    pthread_mutex_init (&list_lock, NULL);

}

/* Resize a thread pool. 

However, it will not remove 
or stop running threads even if 
new_size < running threads. */
void pool_resize (int new_size) {
    assert (new_size > 0);

    pthread_mutex_lock (&pool.lock);
    pool.max_size = new_size;
    pthread_mutex_unlock (&pool.lock);

}

/* Get current active number of threads. 

It's only a snapshot without lock. */
int pool_size (void) {
    return pool.size;
}

/* Try to add an executor in the pool. If pool is
currently full, it will wait until free space. */
static void pool_append (struct executor *executor) {
    assert (!executor->in_pool);
    pthread_mutex_lock (&pool.lock);
    while (pool.size >= pool.max_size) {
        pthread_cond_wait (&pool.ok_to_append, &pool.lock);
    }
    pool.size++;
    executor->in_pool = true;
    pthread_mutex_unlock (&pool.lock);
}

/* Pop out an executor. After its leaving if the size
is less than max_size, wakes up a waiting executor. */
static void pool_pop (struct executor *executor) {

    assert (executor != NULL);
    assert (executor->in_pool);
    pthread_mutex_lock (&pool.lock);
    assert (pool.size > 0);

    pool.size--;
    executor->in_pool = false;
    if (pool.size < pool.max_size)
        pthread_cond_signal (&pool.ok_to_append);
    pthread_mutex_unlock (&pool.lock);
}

/* Initialize an executor and return an instance. */
executor_t executor_init (executor_task *task, void *aux) {
    if (task == NULL)
        return EID_ERROR;

    /* Initialize an executor */
    struct executor *executor = (struct executor *) malloc ( sizeof(struct executor) );
    if (executor == NULL)
        return EID_ERROR;

    executor->task = task;
    executor->aux = aux;
    executor->status = EXECUTOR_BLOCKED;
    executor->tid = NULL;
    executor->in_pool = false;

    pthread_mutex_lock (&list_lock);
    executor->eid = ++alloc_id;
    list_push_front (&list_all, &executor->elem);
    pthread_mutex_unlock (&list_lock);

    return executor->eid;
}

/* Entry function for executor to execute its encapsuled task. */
static void *executor_entry (void *aux) {
    struct executor *executor = (struct executor *)aux;

    pool_append (executor);
    executor->status = EXECUTOR_RUNNING;
    executor->task(executor->aux);
    executor->status = EXECUTOR_DYING;
    pool_pop (executor);

    pthread_mutex_lock (&list_lock);
    list_remove (&executor->elem);
    pthread_mutex_unlock (&list_lock);

    free (executor);

    return NULL;
}

/* Start a initialized executor. */
bool executor_start (executor_t eid) {

    struct executor *executor = get_executor_by_id (eid);

    if (executor == NULL || executor->status != EXECUTOR_BLOCKED)
        return false;

    if (pthread_create (&executor->tid, NULL, executor_entry, (void *) executor ) ) 
        return false; 
    else
        return true;
}

/* Return true if the executor has already exited. */
bool is_executor_exit (executor_t eid) {
    struct executor *executor = get_executor_by_id (eid);
    if ( executor == NULL || executor->status == EXECUTOR_DYING )
        return true;
    else 
        return false;
}

/* Return the status of executor. */
enum executor_status executor_status (executor_t eid) {

    struct executor *executor = get_executor_by_id (eid);
    if ( executor == NULL)
        return EXECUTOR_DYING;
    else
        return executor->status;
}

/* Block current thread and wait for exit of executor. */
void executor_wait (executor_t eid) {

    struct executor *executor = get_executor_by_id (eid);
    if (executor == NULL)
        return;
    else {
        pthread_t threadId = executor->tid;
        pthread_join (threadId, NULL);
    }
}