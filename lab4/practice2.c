#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define ERR(source) (perror(source), \
fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAXSIZE 5
#define THREAD_COUNT 10

typedef struct argsSignalHandler {
    pthread_t tid;
    sigset_t *pMask;
} argsSignalHandler_t;

typedef struct workerThread {
    pthread_t tid;
    pthread_mutex_t *mxArray; // tablica mutexów
    int *array;              // tablica liczników
    int size;                // rozmiar tablicy
} worker_t;

void *signal_handling(void *);
void *worker(void *);

int main(int argc, char **argv) {
    // Maskowanie sygnałów w wątku głównym
    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGQUIT);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("pthread_sigmask");

    argsSignalHandler_t args;
    args.pMask = &newMask;

    if (pthread_create(&args.tid, NULL, signal_handling, &args))
        ERR("pthread_create");

    // Etap 2: Inicjalizacja zasobów
    pthread_mutex_t mxArray[MAXSIZE];
    int array[MAXSIZE] = {0}; // Tablica liczników

    // Inicjalizacja mutexów
    for (int i = 0; i < MAXSIZE; i++) {
        if (pthread_mutex_init(&mxArray[i], NULL))
            ERR("pthread_mutex_init");
    }

    // Tworzenie wątków workerów
    worker_t workers[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        workers[i].mxArray = mxArray;
        workers[i].array = array;
        workers[i].size = MAXSIZE;
        if (pthread_create(&workers[i].tid, NULL, worker, &workers[i]))
            ERR("pthread_create");
    }

    // Czekanie na zakończenie wątków workerów
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_join(workers[i].tid, NULL))
            ERR("pthread_join");
    }

    // Czyszczenie zasobów
    for (int i = 0; i < MAXSIZE; i++) {
        if (pthread_mutex_destroy(&mxArray[i]))
            ERR("pthread_mutex_destroy");
    }

    if (pthread_join(args.tid, NULL))
        ERR("Can't join with 'signal handling' thread");
    if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask))
        ERR("SIG_UNBLOCK error");

    return EXIT_SUCCESS;
}

void *signal_handling(void *voidArgs) {
    argsSignalHandler_t *args = voidArgs;
    int signo;
    srand(time(NULL));
    for (;;) {
        if (sigwait(args->pMask, &signo))
            ERR("sigwait failed");
        switch (signo) {
            case SIGINT:
                printf("Received SIGINT\n");
                break;
            case SIGQUIT:
                printf("Received SIGQUIT\n");
                break;
            default:
                printf("unexpected signal %d\n", signo);
                exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

void *worker(void *voidArgs) {
    worker_t *args = (worker_t *)voidArgs;
    srand(pthread_self());
    for (int i = 0; i < 100; i++) { // Każdy wątek wykonuje 100 operacji
        int index = rand() % args->size; // Losowy indeks w tablicy
        if (pthread_mutex_lock(&args->mxArray[index]))
            ERR("pthread_mutex_lock");
        args->array[index]++;
        printf("Thread %lu incremented index %d, new value: %d\n",
               pthread_self(), index, args->array[index]);
        if (pthread_mutex_unlock(&args->mxArray[index]))
            ERR("pthread_mutex_unlock");
    }
    return NULL;
}