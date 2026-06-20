#include <stdio.h>
#include "gthread.h"

static void brojac(void *arg) {
    const char *ime = (const char *)arg;
    for (int i = 1; i <= 3; i++) {
        printf("  nit %s: korak %d\n", ime, i);
        gt_yield();
    }
    printf("  nit %s: gotova\n", ime);
}

int main(void) {
    gt_spawn(brojac, "A");
    gt_spawn(brojac, "B");

    printf("main: pustam rasporedjivac\n");
    gt_run();
    printf("main: kraj\n");
    return 0;
}
