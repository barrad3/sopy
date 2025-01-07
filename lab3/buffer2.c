#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 10 // Rozmiar bufora
#define SLEEP_TIME_MS 5 // Czas uśpienia podczas busy waiting

typedef struct {
    int data; // Przykładowe dane w klatce
} Frame;

typedef struct {
    Frame *buffer[BUFFER_SIZE]; // Tablica wskaźników na struktury Frame
    int head;                  // Wskaźnik na miejsce wstawienia nowego elementu
    int tail;                  // Wskaźnik na najstarszy element
    int count;                 // Liczba elementów w buforze
    pthread_mutex_t mutex;     // Mutex dla synchronizacji
} CircularBuffer;

// Funkcja inicjalizująca bufor cykliczny
void initBuffer(CircularBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
    pthread_mutex_init(&cb->mutex, NULL);
}

// Funkcja dodająca element do bufora
void push(CircularBuffer *cb, Frame *frame) {
    while (1) {
        pthread_mutex_lock(&cb->mutex);
        if (cb->count < BUFFER_SIZE) {
            cb->buffer[cb->head] = frame;
            cb->head = (cb->head + 1) % BUFFER_SIZE;
            cb->count++;
            pthread_mutex_unlock(&cb->mutex);
            return;
        }
        pthread_mutex_unlock(&cb->mutex);
        usleep(SLEEP_TIME_MS * 1000); // Busy waiting
    }
}

// Funkcja pobierająca element z bufora
Frame *pop(CircularBuffer *cb) {
    while (1) {
        pthread_mutex_lock(&cb->mutex);
        if (cb->count > 0) {
            Frame *frame = cb->buffer[cb->tail];
            cb->tail = (cb->tail + 1) % BUFFER_SIZE;
            cb->count--;
            pthread_mutex_unlock(&cb->mutex);
            return frame;
        }
        pthread_mutex_unlock(&cb->mutex);
        usleep(SLEEP_TIME_MS * 1000); // Busy waiting
    }
}

// Funkcja zwalniająca zasoby bufora
void destroyBuffer(CircularBuffer *cb) {
    pthread_mutex_destroy(&cb->mutex);
}

// Przykładowy wątek producenta
void *producer(void *arg) {
    CircularBuffer *cb = (CircularBuffer *)arg;
    for (int i = 0; i < 20; i++) {
        Frame *frame = malloc(sizeof(Frame));
        frame->data = i;
        printf("Producent: Dodaję klatkę %d\n", i);
        push(cb, frame);
        usleep(100 * 1000); // Symulacja opóźnienia
    }
    return NULL;
}

// Przykładowy wątek konsumenta
void *consumer(void *arg) {
    CircularBuffer *cb = (CircularBuffer *)arg;
    for (int i = 0; i < 20; i++) {
        Frame *frame = pop(cb);
        printf("Konsument: Pobieram klatkę %d\n", frame->data);
        free(frame); // Zwolnienie pamięci
        usleep(150 * 1000); // Symulacja opóźnienia
    }
    return NULL;
}

int main() {
    CircularBuffer cb;
    initBuffer(&cb);

    pthread_t producerThread, consumerThread;

    // Tworzenie wątków producenta i konsumenta
    pthread_create(&producerThread, NULL, producer, &cb);
    pthread_create(&consumerThread, NULL, consumer, &cb);

    // Oczekiwanie na zakończenie wątków
    pthread_join(producerThread, NULL);
    pthread_join(consumerThread, NULL);

    destroyBuffer(&cb);
    return 0;
}
