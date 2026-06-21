#include <stdio.h>
#include "gthread.h"

static void worker(void *arg){
    const char *ime = (const char*)arg;
    printf("\tworker %s: korak 1\n", ime);
    gt_yield();
    printf("\tworker %s: korak 2\n", ime);
    printf("\tworker %s: kraj\n", ime);
}

static void boss(void *arg){
    (void)arg;
    printf("boss: pravim radnike\n");
    gt_thread_t *a = gt_spawn(worker, "A");
    gt_thread_t *b = gt_spawn(worker, "B");
    printf("boss: cekam da zavrse\n");
    gt_join(a);
    gt_join(b);
    printf("boss: oba radnika gotova, nastavljam\n");
}

int main(void){
    gt_spawn(boss, NULL);
    gt_run();
    printf("main: kraj\n");

    return 0;
}
