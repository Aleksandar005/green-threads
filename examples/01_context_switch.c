#include <stdio.h>
#include <ucontext.h>

#define STACK_SIZE (64 * 1024) // 64 KB steka po jednoj niti

static ucontext_t ctx_main; // cuvamo context main-a
static ucontext_t ctx_a; // context niti A
static ucontext_t ctx_b; // context niti B

static char stack_a[STACK_SIZE]; // zaseban stack za nit A
static char stack_b[STACK_SIZE]; // zaseban stack za nit B

static void thread_a(void){
    printf("A: Zdravo, ja sam nit A\n");
    swapcontext(&ctx_a, &ctx_b); // predaj kontrolu niti B
    printf("A: Vratio sam se, zavrsavam\n");
    swapcontext(&ctx_a, &ctx_main); // vracanje u main
}

static void thread_b(void){
    printf("B: Zdravo, ja sam nit B\n");
    swapcontext(&ctx_b, &ctx_a); // vrati kontrolu niti B
}

int main(void){
    // Pripremanje konteksta niti A
    getcontext(&ctx_a);
    ctx_a.uc_stack.ss_sp = stack_a;
    ctx_a.uc_stack.ss_size = sizeof(stack_a);
    ctx_a.uc_link = NULL;
    makecontext(&ctx_a, thread_a, 0);

    // Pripremanje konteksta niti B
    getcontext(&ctx_b);
    ctx_b.uc_stack.ss_sp = stack_b;
    ctx_b.uc_stack.ss_size = sizeof(stack_b);
    ctx_b.uc_link = NULL;
    makecontext(&ctx_b, thread_b, 0);


    printf("main: pokrecem nit A\n");
    swapcontext(&ctx_main, &ctx_a); // cuva se main i skace u A
    printf("main: sve niti gotove, izlazim\n");

    return 0;
}
