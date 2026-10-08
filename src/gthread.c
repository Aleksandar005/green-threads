#include "gthread.h"
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdarg.h>

#define GT_STACK_SIZE (64 * 1024)
#define GT_TIME_SLICE_US  50000
#define GT_MAX_WORKERS 64

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
    gt_spinlock_t join_lock; // stiti prelazak u GT_FINISHED i polje joiner
    gt_thread_t *all_next; // veza u listi svih niti
    gt_spinlock_t *release_after_switch; // brava koju scheduler otpusta kad sacuva nas context
    int prekinuta; // 1 ako ju je prekinuo signal, pa mora da nastavi na istom workeru
};

// ove tri su po workeru tj. svaka OS nit ima svoju kopiju
static _Thread_local ucontext_t  gt_sched_ctx;
static _Thread_local gt_thread_t *gt_current;
static _Thread_local volatile sig_atomic_t gt_preempt_off = 1; // kad je 1, preempcija zabranjena

// lokalni red workera, niti koje je ovaj worker prekinuo signalom
static _Thread_local gt_thread_t *gt_local_head;
static _Thread_local gt_thread_t *gt_local_tail;
static _Thread_local int gt_prvo_lokalni; // prekidac za naizmenicno biranje reda

// Adresu thread-local promenljive ne smemo da izracunamo pre prebacivanja i koristimo posle,
// jer nit posle swapcontext moze da radi na drugom workeru. Kompajler na macOS to radi
// unutar jedne fje, pa pristup ide kroz funkcije koje se ne ugradjuju
__attribute__((noinline)) static void gt_set_preempt_off(int v){
    gt_preempt_off = v;
}

__attribute__((noinline)) static int gt_get_preempt_off(void){
    return gt_preempt_off;
}

__attribute__((noinline)) static ucontext_t *gt_get_sched_ctx(void){
    return &gt_sched_ctx;
}

static int gt_next_id = 1;

static gt_thread_t *gt_ready_head;
static gt_thread_t *gt_ready_tail;
static gt_thread_t *gt_all_head;
static atomic_int gt_live = 0; // broj niti koje su napravljene a jos nisu zavresene

static pthread_t gt_workers[GT_MAX_WORKERS]; // svaki worker, da bi ticker znao kome salje signal
static int gt_broj_workera;

static pthread_t gt_ticker; // nit koja salje SIGALRM workerima
static atomic_int gt_ticker_radi; // kad je 0, ticker zavrsva

static void gt_spin_lock(gt_spinlock_t *l){
    while(atomic_flag_test_and_set(&l->zauzet)){
        // neko drugi drzi bravu
    }
}

static void gt_spin_unlock(gt_spinlock_t *l){
    atomic_flag_clear(&l->zauzet);
}

// ovo da se zaztite gt_ready_head i gt_ready_tail
static gt_spinlock_t gt_ready_lock = {ATOMIC_FLAG_INIT};
// stiti gt_all_head i gt_next_id
static gt_spinlock_t gt_all_lock = {ATOMIC_FLAG_INIT};

void gt_yield(void) {
    gt_set_preempt_off(1);
    gt_current->state = GT_READY;
    swapcontext(&gt_current->ctx, &gt_sched_ctx);
    gt_set_preempt_off(0);
}

void gt_join(gt_thread_t *t) {
    gt_set_preempt_off(1);
    gt_spin_lock(&t->join_lock);

    if(t->state == GT_FINISHED){
        // vec je gotova, nema sta da se ceka
        gt_spin_unlock(&t->join_lock);
        gt_set_preempt_off(0);
        return;
    }

    gt_thread_t *self = gt_current;
    t->joiner = self;
    self->state = GT_BLOCKED;

    self->release_after_switch = &t->join_lock;
    swapcontext(&self->ctx, &gt_sched_ctx);
    gt_set_preempt_off(0);
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
    gt_spin_lock(&gt_ready_lock);

    t->next = NULL;
    if (gt_ready_tail){
        gt_ready_tail->next = t;
    }
    else{
        gt_ready_head = t;
    }
    gt_ready_tail = t;

    gt_spin_unlock(&gt_ready_lock);
}

static gt_thread_t *gt_ready_pop(void){
    gt_spin_lock(&gt_ready_lock);

    gt_thread_t *t = gt_ready_head;
    if(t){
        gt_ready_head = t->next;
        if(!gt_ready_head){
            gt_ready_tail = NULL;
        }
    }

    gt_spin_unlock(&gt_ready_lock);
    return t;
}

static void gt_tick(int sig){
    (void)sig;
    if(gt_get_preempt_off()){
        return;
    }
    gt_set_preempt_off(1);
    gt_current->state = GT_READY;
    gt_current->prekinuta = 1;
    swapcontext(&gt_current->ctx, &gt_sched_ctx);
    gt_set_preempt_off(0);
}

// ticker na svakih GT_TIME_SLICE_US posalje SIGALRM svakom workeru posebno
static void *gt_ticker_main(void *arg){
    (void)arg;
    while(atomic_load(&gt_ticker_radi)){
        usleep(GT_TIME_SLICE_US);
        for(int i = 0; i < gt_broj_workera; i++){
            pthread_kill(gt_workers[i], SIGALRM);
        }
    }

    return NULL;
}

static void gt_preempt_start(void){
    struct sigaction sa;
    sa.sa_handler = gt_tick;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NODEFER;
    sigaction(SIGALRM, &sa, NULL);

    atomic_store(&gt_ticker_radi, 1);
    pthread_create(&gt_ticker, NULL, gt_ticker_main, NULL);
}

static void gt_preempt_stop(void){
    atomic_store(&gt_ticker_radi, 0);
    pthread_join(gt_ticker, NULL);
}

static void gt_local_push(gt_thread_t *t){
    t->next = NULL;
    if(gt_local_tail){
        gt_local_tail->next = t;
    } else {
        gt_local_head = t;
    }
    gt_local_tail = t;
}

static gt_thread_t *gt_local_pop(void){
    gt_thread_t *t = gt_local_head;
    if(t){
        gt_local_head = t->next;
        if(!gt_local_head){
            gt_local_tail = NULL;
        }
    }

    return t;
}

// bira sledecu nit, naizmenicno prvo iz lokalnog pa iz zajednickog reda
static gt_thread_t *gt_next_thread(void){
    gt_thread_t *t;
    gt_prvo_lokalni = !gt_prvo_lokalni;

    if(gt_prvo_lokalni){
        t = gt_local_pop();
        if(!t){
            t = gt_ready_pop();
        }
    } else {
        t = gt_ready_pop();
        if(!t){
            t = gt_local_pop();
        }
    }

    return t;
}

// petlja koju vrti svaki worker
static void gt_scheduler_loop(void){
    gt_set_preempt_off(1);

    while(atomic_load(&gt_live) > 0){
        gt_thread_t *t = gt_next_thread();
        if(!t){
            // red je prazank, ali neke niti rade na drugim workerima
            sched_yield();
            continue;
        }

        gt_current = t;
        t->state = GT_RUNNING;
        t->prekinuta = 0;
        swapcontext(&gt_sched_ctx, &t->ctx);

        // stanje citamo jednom, odmah cim nit vratimo u red ili otpustimo bravu
        // drugi worker moze da je pokrene i promeni joj stanje
        enum gt_state stanje = t->state;
        gt_spinlock_t *brava = t->release_after_switch;
        t->release_after_switch = NULL;

        if(stanje == GT_READY){
            if(t->prekinuta){
                gt_local_push(t);
            } else {
                gt_ready_push(t);
            }
        } else if(stanje == GT_FINISHED && t->joiner){
            t->joiner->state = GT_READY;
            gt_ready_push(t->joiner);
        }

        // context niti je sad sacuvan, otpustamo bravu
        if(brava){
            gt_spin_unlock(brava);
        }

        if(stanje == GT_FINISHED){
            atomic_fetch_sub(&gt_live, 1);
        }
    }
    
}

// pocetna funkcija za dodatne workere
static void *gt_worker_main(void *arg){
    (void)arg;
    gt_scheduler_loop();
    return NULL;
}

void gt_run_workers(int broj_workera){
    if(broj_workera < 1){
        broj_workera = 1;
    }

    if(broj_workera > GT_MAX_WORKERS){
        broj_workera = GT_MAX_WORKERS;
    }

    // main nije laka nit, pa signal ne sme da je prekine
    gt_set_preempt_off(1);

    gt_broj_workera = broj_workera;

    // main nije worker, svi workeri su posebne pthread niti
    for(int i = 0; i < broj_workera; i++){
        pthread_create(&gt_workers[i], NULL, gt_worker_main, NULL);
    }

    gt_preempt_start();

    // main samo ceka da sve lake niti zavrse
    while(atomic_load(&gt_live) > 0){
        usleep(1000);
    }

    gt_preempt_stop(); // ticker gasimo pre join-a, kao i pre

    for(int i = 0; i < broj_workera; i++){
        pthread_join(gt_workers[i], NULL);
    }

    gt_free_all();
}

void gt_run(void){
    gt_run_workers(1);
}

static void gt_trampoline(void) {
    gt_thread_t *self = gt_current;
    gt_set_preempt_off(0);
    self->fn(self->arg);
    gt_set_preempt_off(1);
    
    // oznaka kraja mora ici pod istom bravom kojom gt_join proverava stanje
    gt_spin_lock(&self->join_lock);
    self->state = GT_FINISHED;

    // scheduler prvo cita joinera pa onda otpusta bravu
    self->release_after_switch = &self->join_lock;

    // skacemo u scheduler worker na kom se sad izvrsava
    setcontext(gt_get_sched_ctx());
}


gt_thread_t *gt_spawn(void (*fn)(void *), void *arg){
    // malloc ne sme biti prekinut, pa zabranu ukljucujemo odmah
    gt_set_preempt_off(1);
    
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
    t->joiner = NULL;
    t->release_after_switch = NULL;
    t->prekinuta = 0;
    atomic_flag_clear(&t->join_lock.zauzet);

    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = GT_STACK_SIZE;
    t->ctx.uc_link = NULL; // trampolina se nikad ne vraca, sama skace u scheduler
    makecontext(&t->ctx, gt_trampoline, 0);

    gt_set_preempt_off(1);

    gt_spin_lock(&gt_all_lock);
    t->id = gt_next_id++;
    t->all_next = gt_all_head;
    gt_all_head = t;
    gt_spin_unlock(&gt_all_lock);

    atomic_fetch_add(&gt_live, 1);
    gt_ready_push(t);
    gt_set_preempt_off(0);

    return t;
}

void gt_mutex_init(gt_mutex_t *m) {
    atomic_flag_clear(&m->guard.zauzet);
    m->locked = 0;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

void gt_mutex_lock(gt_mutex_t *m){
    gt_set_preempt_off(1);
    gt_spin_lock(&m->guard);

    if(!m->locked){
        m->locked = 1;
        gt_spin_unlock(&m->guard);
        gt_set_preempt_off(0);
        return;
    }

    gt_thread_t *self = gt_current;
    self->next = NULL;
    if(m->wait_tail){
        m->wait_tail->next = self;
    } else {
        m->wait_head = self;
    }

    m->wait_tail = self;
    self->state = GT_BLOCKED;
    // guard se ne otpusta ovde, jer ce ga scheduler otpustiti kasnije nakon cuvanja konteksta
    self->release_after_switch = &m->guard;
    swapcontext(&self->ctx, &gt_sched_ctx);
    gt_set_preempt_off(0);
}

void gt_mutex_unlock(gt_mutex_t *m) {
    gt_set_preempt_off(1);
    gt_spin_lock(&m->guard);

    gt_thread_t *t = m->wait_head;
    if (t) {
        m->wait_head = t->next;
        if (!m->wait_head){
            m->wait_tail = NULL;
        }
    } else {
        m->locked = 0;
    }

    gt_spin_unlock(&m->guard);

    if(t){
        t->state = GT_READY;
        gt_ready_push(t);
    }

    gt_set_preempt_off(0);
}

void gt_preempt_disable(void){
    gt_set_preempt_off(1);
}

void gt_preempt_enable(void){
    gt_set_preempt_off(0);
}

int gt_printf(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    gt_preempt_disable();
    int n = vprintf(fmt, args);
    gt_preempt_enable();

    va_end(args);

    return n;
}
