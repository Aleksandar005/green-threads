#ifndef GTHREAD_H
#define GTHREAD_H

typedef struct gt_thread gt_thread_t;

gt_thread_t *gt_spawn(void (*fn)(void *), void *arg);
void gt_yield(void);
void gt_join(gt_thread_t *t);
void gt_run(void);

#endif
