#include <stdio.h>
#include "gthread.h"

static void worker(void *arg) {
    const char *ime = (const char *)arg;
    printf("\t[nit %s]: zdravo, radim na svom steku\n", ime);
    printf("\t[nit %s]: gotovo\n", ime);
}

int main(void) {
    printf("main: pravim dve niti\n");
    gt_thread_t *t1 = gt_spawn(worker, "A");
    gt_thread_t *t2 = gt_spawn(worker, "B");

    printf("main: pokrecem nit A\n");
    gt_resume(t1);
    printf("main: pokrecem nit B\n");
    gt_resume(t2);

    printf("main: obe gotove, ciscenje\n");
    gt_free(t1);
    gt_free(t2);
    return 0;
}
