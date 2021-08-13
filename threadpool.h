#ifndef __THREADPOOL_H
#define __THREADPOOL_H

/* Implementation of thread pool. */

#include<stdbool.h>

typedef struct executor* executor_t;
typedef void executor_task (void *);

enum executor_status {
    EXECUTOR_BLOCKED,
    EXECUTOR_READY,
    EXECUTOR_RUNNING,
    EXECUTOR_DYING
};
/* Pool functions */
void pool_init (int max_size);
void pool_resize (int new_size);
int pool_size (void);

/* Executor functions */
executor_t executor_init (executor_task *task, void *aux);
bool executor_start (executor_t *);
bool is_executor_exit (executor_t);
enum executor_status executor_status ();

#endif /* threadpool.h */
