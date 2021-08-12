#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include<stdbool.h>

enum executor_status {
    EXECUTOR_BLOCKED,
    EXECUTOR_READY,
    EXECUTOR_RUNNING,
    EXECUTOR_DYING
};

void pool_init (int max_size);
void pool_resize (int new_size);

typedef void executor_task (void *);

struct executor *executor_init (executor_task *task, void *aux);
bool executor_start (struct executor *);
void executor_exit (struct executor *);
enum executor_status executor_status ();

#endif /* threads/vaddr.h */