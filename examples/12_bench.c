#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include "gthread.h"

#define YIELDS 1000000

static atomic_int zavrsene;

static double sada(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// 1) cena prebacivanja: dve niti se smenjuju preko gt_yield
static void pingpong(void *arg){
    (void)arg;
    for(int i = 0; i < YIELDS; i++){
        gt_yield();
    }
}

// 2) mnogo niti: svaka jednom prepusti CPU i zavrsi
static void mala(void *arg){
    (void)arg;
    gt_yield();
    atomic_fetch_add(&zavrsene, 1);
}

int main(int argc, char **argv){
    int broj_niti = 100000;
    int broj_workera = 4;
    if(argc > 1) broj_niti = atoi(argv[1]);
    if(argc > 2) broj_workera = atoi(argv[2]);

    // test 1, jedan worker da merimo cisto prebacivanje
    gt_spawn(pingpong, NULL);
    gt_spawn(pingpong, NULL);
    double t0 = sada();
    gt_run_workers(1);
    double t1 = sada();
    double prebacivanja = 2.0 * YIELDS;
    printf("yield: %.0f ns po yield-u (%d niti x %d)\n",
           (t1 - t0) / prebacivanja * 1e9, 2, YIELDS);

    // test 2, mnogo niti istovremeno
    double t2 = sada();
    for(int i = 0; i < broj_niti; i++){
        if(!gt_spawn(mala, NULL)){
            printf("spawn nije uspeo posle %d niti\n", i);
            break;
        }
    }
    gt_run_workers(broj_workera);
    double t3 = sada();
    printf("niti: %d zavrseno od %d, na %d workera, za %.0f ms\n",
           atomic_load(&zavrsene), broj_niti, broj_workera, (t3 - t2) * 1e3);
    return 0;
}