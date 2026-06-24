#include <stdio.h>
#include "gthread.h"

#define BROJ_NITI  3
#define PO_NITI    1000

static long brojac;
static gt_mutex_t brava;
static int koristi_bravu;

static void uvecaj(void *arg) {
    (void)arg;
    for (int i = 0; i < PO_NITI; i++) {
        if (koristi_bravu) gt_mutex_lock(&brava);
        long t = brojac;        // procitaj
        gt_yield();             // prepusti CPU usred razmene
        brojac = t + 1;         // upisi na osnovu stare procitane vrednosti
        if (koristi_bravu) gt_mutex_unlock(&brava);
    }
}

static void probaj(int sa_bravom) {
    brojac = 0;
    koristi_bravu = sa_bravom;
    for (int i = 0; i < BROJ_NITI; i++){
        gt_spawn(uvecaj, NULL);
    }
    gt_run();
    printf("%s mutex: ocekivano %d, dobijeno %ld\n",
           sa_bravom ? "SA " : "BEZ", BROJ_NITI * PO_NITI, brojac);
}

int main(void) {
    gt_mutex_init(&brava);
    probaj(0);   // pogresan zbir, bez brave
    probaj(1);   // tacan zbir, sa bravom
    return 0;
}
