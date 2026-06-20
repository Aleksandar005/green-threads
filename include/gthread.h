#ifndef GTHREAD_H
#define GTHREAD_H

typedef struct gt_thread gt_thread_t;

gt_thread_t *gt_spawn(void (*fn)(void *), void *arg);
void gt_resume(gt_thread_t *t); // ovo ce kasnije biti rasporedjivac
void gt_free(gt_thread_t *t);

#endif
