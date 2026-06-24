#ifndef GTHREAD_H
#define GTHREAD_H

typedef struct gt_thread gt_thread_t;

gt_thread_t *gt_spawn(void (*fn)(void *), void *arg);
void gt_yield(void);
void gt_join(gt_thread_t *t);
void gt_run(void);

typedef struct gt_mutex {
    int locked;
    gt_thread_t *wait_head;
    gt_thread_t *wait_tail;
} gt_mutex_t;

void gt_mutex_init(gt_mutex_t *m);
void gt_mutex_lock(gt_mutex_t *m);
void gt_mutex_unlock(gt_mutex_t *m);

#endif
