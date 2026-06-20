#include "gthread.h"
#include <stdlib.h>
#include <ucontext.h>

#define GT_STACK_SIZE (64 * 1024)

enum gt_state {GT_READY, GT_RUNNING, GT_FINISHED};

struct gt_thread {
    ucontext_t ctx; // sacuvani context
    void *stack; // stack ove niti, alociran na heap-u
    void (*fn)(void *); // funkcija koju nit izvrsava
    void *arg; // argumenti gornje funkcije
    enum gt_state state; // stanje niti
    int id; // redni broj
    gt_thread_t *next;
};

static ucontext_t gt_sched_ctx; // context pozivaoca (npr. main-a)
static gt_thread_t *gt_current; // nit koja trenutno radi
static int gt_next_id = 1;

static gt_thread_t *gt_ready_head;
static gt_thread_t *gt_ready_tail;

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
            gt_ready_head = NULL;
        }
    }

    return t;
}

void gt_run(void){
    gt_thread_t *t;
    while((t = gt_ready_pop()) != NULL){
        gt_current = t;
        swapcontext(&gt_sched_ctx, &t->ctx);
        gt_free(t);
    }
}

static void gt_trampoline(void){
    gt_thread_t *self = gt_current;
    self->state = GT_RUNNING;
    self->fn(self->arg); // uradi posao niti
    self->state = GT_FINISHED;
    // funkcija se vraca pa nas uc_link vraca u pozivaoca
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

    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = GT_STACK_SIZE;
    t->ctx.uc_link = &gt_sched_ctx; // gde se ide kad nit zavrsi
    makecontext(&t->ctx, gt_trampoline, 0);

    gt_ready_push(t);
    return t;
}

void gt_resume(gt_thread_t *t){
    if(t->state == GT_FINISHED){
        return;
    }

    gt_current = t;
    swapcontext(&gt_sched_ctx, &t->ctx); // udji u nit i vrati se kad zavrsi
}

void gt_free(gt_thread_t *t) {
    free(t->stack);
    free(t);
}
