#include "gthread.h"
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#define GT_STACK_SIZE (64 * 1024)
#define GT_TIME_SLICE_US  50000

enum gt_state {GT_READY, GT_RUNNING, GT_FINISHED, GT_BLOCKED};

struct gt_thread {
    ucontext_t ctx; // sacuvani context
    void *stack; // stack ove niti, alociran na heap-u
    void (*fn)(void *); // funkcija koju nit izvrsava
    void *arg; // argumenti gornje funkcije
    enum gt_state state; // stanje niti
    int id; // redni broj
    gt_thread_t *next; // veza u redu spremnih
    gt_thread_t *joiner; // nit koja ceka da ova zavrsi
    gt_thread_t *all_next; // veza u listi svih niti
};

static ucontext_t  gt_sched_ctx;
static gt_thread_t *gt_current;
static int gt_next_id = 1;

static gt_thread_t *gt_ready_head;
static gt_thread_t *gt_ready_tail;
static gt_thread_t *gt_all_head;

void gt_yield(void) {
    gt_thread_t *self = gt_current;
    self->state = GT_READY;
    swapcontext(&self->ctx, &gt_sched_ctx);
}

void gt_join(gt_thread_t *t) {
    if (t->state == GT_FINISHED){
        return;
    }
    gt_thread_t *self = gt_current;
    t->joiner = self;
    self->state = GT_BLOCKED;
    swapcontext(&self->ctx, &gt_sched_ctx);
}

static void gt_free_all(void) {
    gt_thread_t *t = gt_all_head;
    while (t) {
        gt_thread_t *next = t->all_next;
        free(t->stack);
        free(t);
        t = next;
    }
    gt_all_head = NULL;
}


static void gt_ready_push(gt_thread_t *t){
    t->next = NULL;
    if (gt_ready_tail){
        gt_ready_tail->next = t;
    }
    else{
        gt_ready_head = t;
    }
    gt_ready_tail = t;
}

static gt_thread_t *gt_ready_pop(void){
    gt_thread_t *t = gt_ready_head;
    if(t){
        gt_ready_head = t->next;
        if(!gt_ready_head){
            gt_ready_tail = NULL;
        }
    }

    return t;
}

static void gt_tick(int sig){
    (void)sig;
    ssize_t n = write(STDOUT_FILENO, ".", 1);
    (void)n;
}

static void gt_timer_start(void){
    struct sigaction sa;
    sa.sa_handler = gt_tick;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval tv;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = GT_TIME_SLICE_US;
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = GT_TIME_SLICE_US;
    setitimer(ITIMER_REAL, &tv, NULL);
}

static void gt_timer_stop(void){
    struct itimerval tv;
    tv.it_value.tv_sec     = 0;
    tv.it_value.tv_usec    = 0;
    tv.it_interval.tv_sec  = 0;
    tv.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &tv, NULL);
}

void gt_run(void){
    gt_timer_start();
    gt_thread_t *t;
    while((t = gt_ready_pop()) != NULL){
        gt_current = t;
        t->state = GT_RUNNING;
        swapcontext(&gt_sched_ctx, &t->ctx);

        if(t->state == GT_READY){
            gt_ready_push(t);
        } else if(t->state == GT_FINISHED && t->joiner){
            t->joiner->state = GT_READY;
            gt_ready_push(t->joiner);
        }
    }

    gt_timer_stop();
    gt_free_all();
}

static void gt_trampoline(void) {
    gt_thread_t *self = gt_current;
    self->fn(self->arg);
    self->state = GT_FINISHED;
}


gt_thread_t *gt_spawn(void (*fn)(void *), void *arg){
    gt_thread_t *t = malloc(sizeof(*t));
    if(!t){
        return NULL;
    }

    t->stack = malloc(GT_STACK_SIZE);
    if(!(t->stack)){
        free(t);
        return NULL;
    }

    t->fn = fn;
    t->arg = arg;
    t->state = GT_READY;
    t->id = gt_next_id++;
    t->joiner = NULL;

    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = GT_STACK_SIZE;
    t->ctx.uc_link = &gt_sched_ctx; // gde se ide kad nit zavrsi
    makecontext(&t->ctx, gt_trampoline, 0);

    t->all_next = gt_all_head;
    gt_all_head = t;

    gt_ready_push(t);
    return t;
}
