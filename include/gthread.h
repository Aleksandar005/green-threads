#ifndef GTHREAD_H
#define GTHREAD_H

#include <stdatomic.h>

typedef struct gt_thread gt_thread_t;

gt_thread_t *gt_spawn(void (*fn)(void *), void *arg);
void gt_yield(void);
void gt_join(gt_thread_t *t);
void gt_run(void);
void gt_run_workers(int broj_workera);

// zabrana preempcije oko koda koji nije bezbedam za prekid (printf, malloc, ...)
void gt_preempt_disable(void);
void gt_preempt_enable(void);

// isto kao printf, ali tajmer ne moze da prekine nit usred ispisa
int gt_printf(const char *fmt, ...);

// unutrasnji spinlock biblioteke, korisnik ga ne koristi direktno
typedef struct {
    atomic_flag zauzet;
} gt_spinlock_t;
typedef struct gt_mutex {
    gt_spinlock_t guard;
    int locked;
    gt_thread_t *wait_head;
    gt_thread_t *wait_tail;
} gt_mutex_t;

void gt_mutex_init(gt_mutex_t *m);
void gt_mutex_lock(gt_mutex_t *m);
void gt_mutex_unlock(gt_mutex_t *m);

#endif
