#include <stdio.h>
#include <pthread.h>

#define BROJ_WORKERA 4

// svaki worker dobija svoj redni broj
static int worker_id[BROJ_WORKERA];

static void *worker(void *arg){
    int id = *(int *)arg;

    printf("worker %d: krecem\n", id);

    // pravi posao, da se vidi da rade u isto vreme
    long suma = 0;
    for(long i = 0; i < 500000000; i++){
        suma += i;
    }

    printf("worker %d: gotov (suma = %ld)\n", id, suma);
    return NULL;
}

int main(void){
    pthread_t niti[BROJ_WORKERA];

    for(int i = 0; i < BROJ_WORKERA; i++){
        worker_id[i] = i;
        pthread_create(&niti[i], NULL, worker, &worker_id[i]);
    }

    printf("mainL pokrenuo %d workera, cekam ih\n", BROJ_WORKERA);

    for(int i = 0; i < BROJ_WORKERA; i++){
        pthread_join(niti[i], NULL);
    }

    printf("main: svi workeri gotovi\n");

    return 0;
}