#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define BROJ_WORKERA 4
#define KORACI 1000000

static long brojac = 0;
static pthread_mutex_t brava = PTHREAD_MUTEX_INITIALIZER;
static atomic_long brojac_atomic = 0;

// 1) bez ikakve zastite
static void *bez_zastite(void *arg){
    (void)arg;
    for(int i = 0; i < KORACI; i++){
        brojac++;
    }
    return NULL;
}

// 2) sa pthread mutexom
static void *sa_mutexom(void *arg){
    (void)arg;
    for(int i = 0; i < KORACI; i++){
        pthread_mutex_lock(&brava);
        brojac++;
        pthread_mutex_unlock(&brava);
    }
    return NULL;
}

// 3) sa atomicnim sabiranjem
static void *sa_atomic(void *arg){
    (void)arg;
    for(int i = 0; i < KORACI; i++){
        atomic_fetch_add(&brojac_atomic, 1);
    }
    return NULL;
}

// pokrece BROJ_WORKERA niti koje sve izvrsavaju istu fju i ceka ih
static void pokreni(void *(*funkcija)(void *)){
    pthread_t niti[BROJ_WORKERA];

    for(int i = 0; i < BROJ_WORKERA; i++){
        pthread_create(&niti[i], NULL, funkcija, NULL);
    }

    for(int i = 0; i < BROJ_WORKERA; i++){
        pthread_join(niti[i], NULL);
    }
}

int main(void){
    long ocekivano = (long)BROJ_WORKERA * KORACI;

    brojac = 0;
    pokreni(bez_zastite);
    printf("bez zastite: ocekivano %ld, dobijeno %ld\n", ocekivano, brojac);

    brojac = 0;
    pokreni(sa_mutexom);
    printf("sa mutexom: ocekivano %ld, dobijeno %ld\n", ocekivano, brojac);

    pokreni(sa_atomic);
    printf("sa atomic: ocekivano %ld, dobijeno %ld\n", ocekivano, atomic_load(&brojac_atomic));

    return 0;
}

