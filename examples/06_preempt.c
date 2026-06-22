#include <stdio.h>
#include "gthread.h"

static void busy(void *arg) {
    (void)arg;
    printf("nit: krecem u dugu petlju, bez ijednog yield-a\n");
    fflush(stdout);

    for (volatile long i = 0; i < 5000000000L; i++) {
        // samo trosi procesor, nikada ne pozivam gt_yield
    }

    printf("\nnit: petlja gotova\n");
}

int main(void) {
    gt_spawn(busy, NULL);

    printf("main: pustam rasporedjivac (tajmer radi u pozadini)\n");
    gt_run();
    printf("main: kraj\n");
    return 0;
}
