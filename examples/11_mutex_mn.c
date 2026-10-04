#include <stdio.h>
#include <stdlib.h>
#include "gthread.h"

#define BROJ_NITI 8
#define PO_NITI 100000

static long brojac;
static gt_mutex_t brava;
static int koristi_bravu;

static void uvecaj(void *arg){
    (void)arg;
    for(int i = 0; i < PO_NITI; i++){
        if(koristi_bravu){
            gt_mutex_lock(&brava);
        }
        brojac++;
        // povremeno prepusti CPU dok drzi bravu da bi ostale niti morale da cekaju
        if(i % 100 == 0){
            gt_yield();
        }
        if(koristi_bravu){
            gt_mutex_unlock(&brava);
        }
    }
}

static void probaj(int sa_bravom, int broj_workera){
    brojac = 0;
    koristi_bravu = sa_bravom;
    for(int i = 0; i < BROJ_NITI; i++){
        gt_spawn(uvecaj, NULL);
    }
    gt_run_workers(broj_workera);
    printf("%s mutex: ocekivano %d, dobijeno %ld\n",
    sa_bravom ? "SA " : "BEZ", BROJ_NITI * PO_NITI, brojac);
}

int main(int argc, char* argv[]){
    int broj_workera = 4;
    if(argc > 1){
        broj_workera = atoi(argv[1]);
    }

    gt_mutex_init(&brava);
    probaj(0, broj_workera);
    probaj(1, broj_workera);
    
    return 0;
}