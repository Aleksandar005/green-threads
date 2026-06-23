#include <stdio.h>
#include "gthread.h"

static void racunar(void *arg) {
    const char *ime = (const char *)arg;
    long suma = 0;
    long prag = 100000000L;
    for (long i = 1; i <= 600000000L; i++) {
        suma += i;
        if (i == prag) {
            printf("  %s: prosao %ld iteracija\n", ime, i);
            prag += 100000000L;
        }
    }
    printf("%s: gotova (suma = %ld)\n", ime, suma);
}

int main(void) {
    gt_spawn(racunar, "A");
    gt_spawn(racunar, "B");

    printf("main: dve niti racunaju, NIJEDNA ne poziva yield\n");
    gt_run();
    printf("main: obe gotove\n");
    return 0;
}
