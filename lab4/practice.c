#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAXLINE 4096
#define DEFAULT_ARRAYSIZE 10
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct argsSignalHandler {
    pthread_t tid;
    int *pArray;
    int arraySize;
    pthread_mutex_t *pmxArray;
    sigset_t *pMask;
    bool *pQuitFlag;
    pthread_mutex_t *pmxQuitFlag;
} argsSignalHandler_t;

typedef struct argsWorker {
    pthread_t tid;
    int *pArray;
    int arraySize;
    pthread_mutex_t *pmxArray;
    bool *pQuitFlag;
    pthread_mutex_t *pmxQuitFlag;
} argsWorker_t;

void ReadArguments(int argc, char **argv, int *arraySize);
void printArray(int *array, int arraySize);
void *signal_handling(void *args);
void *worker_function(void *args);

int main(int argc, char **argv) {
    int arraySize;
    ReadArguments(argc, argv, &arraySize);

    int *array = (int *)malloc(sizeof(int) * arraySize);
    if (array == NULL)
        ERR("Malloc error for array!");

    for (int i = 0; i < arraySize; i++)
        array[i] = i + 1;

    bool quitFlag = false;
    pthread_mutex_t mxQuitFlag = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mxArray = PTHREAD_MUTEX_INITIALIZER;

    // Konfiguracja obsługi sygnałów
    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGQUIT);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    // Inicjalizacja wątku do obsługi sygnałów
    argsSignalHandler_t signalArgs;
    signalArgs.pArray = array;
    signalArgs.arraySize = arraySize;
    signalArgs.pmxArray = &mxArray;
    signalArgs.pMask = &newMask;
    signalArgs.pQuitFlag = &quitFlag;
    signalArgs.pmxQuitFlag = &mxQuitFlag;

    if (pthread_create(&signalArgs.tid, NULL, signal_handling, &signalArgs))
        ERR("Couldn't create signal handling thread!");

    // Inicjalizacja wątków pracujących na tablicy
    int numThreads = 4;
    argsWorker_t workers[numThreads];

    for (int i = 0; i < numThreads; i++) {
        workers[i].pArray = array;
        workers[i].arraySize = arraySize;
        workers[i].pmxArray = &mxArray;
        workers[i].pQuitFlag = &quitFlag;
        workers[i].pmxQuitFlag = &mxQuitFlag;

        if (pthread_create(&workers[i].tid, NULL, worker_function, &workers[i]))
            ERR("Couldn't create worker thread!");
    }

    while (true) {
        pthread_mutex_lock(&mxQuitFlag);
        if (quitFlag == true) {
            pthread_mutex_unlock(&mxQuitFlag);
            break;
        }
        pthread_mutex_unlock(&mxQuitFlag);

        pthread_mutex_lock(&mxArray);
        printArray(array, arraySize);
        pthread_mutex_unlock(&mxArray);

        sleep(1);
    }

    if (pthread_join(signalArgs.tid, NULL))
        ERR("Can't join with 'signal handling' thread");

    for (int i = 0; i < numThreads; i++) {
        if (pthread_join(workers[i].tid, NULL))
            ERR("Can't join with worker thread");
    }

    free(array);
    if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask))
        ERR("SIG_UNBLOCK error");

    exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char **argv, int *arraySize) {
    *arraySize = DEFAULT_ARRAYSIZE;

    if (argc >= 2) {
        *arraySize = atoi(argv[1]);
        if (*arraySize <= 0) {
            printf("Invalid value for 'array size'\n");
            exit(EXIT_FAILURE);
        }
    }
}

void printArray(int *array, int arraySize) {
    printf("[");
    for (int i = 0; i < arraySize; i++)
        printf(" %d", array[i]);
    printf(" ]\n");
}

void *signal_handling(void *voidArgs) {
    argsSignalHandler_t *args = (argsSignalHandler_t *)voidArgs;
    int signo;

    for (;;) {
        if (sigwait(args->pMask, &signo))
            ERR("sigwait failed.");

        switch (signo) {
            case SIGINT:
                pthread_mutex_lock(args->pmxArray);
                for (int i = 0; i < args->arraySize; i++) {
                    args->pArray[i] -= 1;  // Dekrementacja wszystkich wartości w tablicy
                }
                pthread_mutex_unlock(args->pmxArray);
                break;
            case SIGQUIT:
                pthread_mutex_lock(args->pmxQuitFlag);
                *args->pQuitFlag = true;
                pthread_mutex_unlock(args->pmxQuitFlag);
                return NULL;
            default:
                printf("Unexpected signal %d\n", signo);
                exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

void *worker_function(void *voidArgs) {
    argsWorker_t *args = (argsWorker_t *)voidArgs;

    while (true) {
        pthread_mutex_lock(args->pmxQuitFlag);
        if (*args->pQuitFlag) {
            pthread_mutex_unlock(args->pmxQuitFlag);
            break;
        }
        pthread_mutex_unlock(args->pmxQuitFlag);

        pthread_mutex_lock(args->pmxArray);
        int index = rand() % args->arraySize;
        args->pArray[index] += 1;  // Inkrementacja losowego elementu tablicy
        pthread_mutex_unlock(args->pmxArray);

        usleep(500000);  // Symulacja pracy
    }

    return NULL;
}
