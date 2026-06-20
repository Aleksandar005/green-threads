#include <stdio.h>
#include "gthread.h"

static void posao(void *arg) {
    const char *ime = (const char *)arg;
    printf("  nit %s: krecem\n", ime);
    printf("  nit %s: zavrsavam\n", ime);
}

int main(void) {
    gt_spawn(posao, "prva");
    gt_spawn(posao, "druga");
    gt_spawn(posao, "treca");

    printf("main: pustam rasporedjivac\n");
    gt_run();
    printf("main: sve niti zavrsene\n");
    return 0;
}
