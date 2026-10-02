#include <stdio.h>
#include <stdlib.h>
#include "gthread.h"

#define BROJ_NITI 8
#define KORACI 200000000L

static void racunaj(void *arg){
    long id = (long)arg;
    long suma = 0;

    for(int i = 0; i < KORACI; i++){
        suma += i;
        // dok preepcija ne radi sa vise workera, prepustamo rucno
        if(i % 1000000 == 0){
            gt_yield();
        }
        
    }

    printf("nit %ld: gotova (suma = %ld)\n", id, suma);
}

int main(int argc, char *argv[]){
    int broj_workera = 1;
    if(argc > 1){
        broj_workera = atoi(argv[1]);
    }

    for(long i = 0; i < BROJ_NITI; i++){
        gt_spawn(racunaj, (void *)i);
    }

    printf("main: %d niti na %d workera\n", BROJ_NITI, broj_workera);
    gt_run_workers(broj_workera);
    printf("main: sve gotovo\n");

    return 0;
}