#define _GNU_SOURCE

#include <errno.h>       // Definicje standardowych komunikatów błędów
#include <fcntl.h>       // Funkcje i stałe związane z operacjami na plikach (np. shm_open)
#include <pthread.h>     // Funkcje i typy związane z wątkami oraz synchronizacją (mutexy)
#include <signal.h>      // Definicje i funkcje związane z obsługą sygnałów
#include <stdio.h>       // Standardowe funkcje wejścia/wyjścia (printf, fprintf, perror)
#include <stdlib.h>      // Funkcje standardowe (atoi, exit, srand, rand)
#include <string.h>      // Funkcje do operacji na pamięci (memcpy, memset, itp.)
#include <sys/mman.h>    // Funkcje do mapowania pamięci (mmap, munmap)
#include <sys/stat.h>    // Funkcje i typy związane z operacjami na plikach (ftruncate)
#include <sys/types.h>   // Definicje typów systemowych (pid_t)
#include <unistd.h>      // Funkcje systemowe (getpid, close, etc.)

// Makro do obsługi błędów: wypisuje informacje o pliku i linii, komunikat błędu,
// wysyła sygnał SIGKILL wszystkim procesom w grupie i kończy działanie programu.
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

// Rozmiar segmentu pamięci współdzielonej (1 KB)
#define SHM_SIZE 1024

/**
 * Funkcja wypisująca instrukcję użycia programu,
 * gdzie argumentem jest rozmiar planszy (N) spełniający warunki: 3 < N <= 30.
 */
void usage(char* name)
{
    fprintf(stderr, "USAGE: %s N\n", name);
    fprintf(stderr, "3 < N <= 30 - board size\n");
    exit(EXIT_FAILURE);
}

/**
 * Struktura przekazywana do wątku obsługującego sygnały.
 * - running: flaga informująca, czy program powinien dalej działać.
 * - mutex: mutex używany do synchronizacji zmiany flagi.
 * - old_mask, new_mask: zestawy sygnałów używane do blokowania i oczekiwania na sygnał.
 */
typedef struct 
{
    int running;              // Flaga, która po ustawieniu na 0 sygnalizuje zakończenie pracy programu
    pthread_mutex_t mutex;    // Mutex do synchronizacji przy zmienianiu flagi
    sigset_t old_mask, new_mask;  // Zestawy sygnałów - nowy (blokowany) i stary
} sighandling_args_t;

/**
 * Funkcja wątku odpowiedzialnego za obsługę sygnału SIGINT.
 * Wątek oczekuje na sygnał, a gdy go otrzyma, ustawia flagę "running" na 0,
 * co sygnalizuje głównej pętli zakończenie działania programu.
 */
void* signal_handling(void *args) 
{
    // Rzutowanie argumentu na właściwy typ
    sighandling_args_t* sighandling_args = args;
    int signo;  // Zmienna przechowująca odebrany sygnał

    // Oczekiwanie na sygnał z zestawu new_mask (tu: SIGINT)
    if(sigwait(&sighandling_args->new_mask, &signo))
        ERR("sigwait failed");

    // Sprawdzenie, czy odebrany sygnał to SIGINT
    if(signo != SIGINT)
        ERR("unexpected signal");

    // Odblokowanie mutexa, aby umożliwić zmianę flagi running
    pthread_mutex_unlock(&sighandling_args->mutex);
    // Ustawienie flagi running na 0 - sygnalizuje to zakończenie pętli głównej
    sighandling_args->running = 0;
    // Ponowne zablokowanie mutexa (synchronizacja z główną pętlą)
    pthread_mutex_lock(&sighandling_args->mutex);

    return NULL;
}

/**
 * Funkcja główna programu.
 * Program tworzy segment pamięci współdzielonej, inicjuje planszę o rozmiarze N x N,
 * uruchamia mutex współdzielony (z atrybutem PTHREAD_PROCESS_SHARED i PTHREAD_MUTEX_ROBUST)
 * oraz wątek do obsługi sygnału SIGINT. Następnie w pętli co 3 sekundy wypisuje stan planszy,
 * dopóki nie nastąpi przerwanie przez SIGINT.
 */
int main(int argc, char **argv)
{
    // Sprawdzenie liczby argumentów, oczekujemy jednego argumentu: N (rozmiar planszy)
    if(argc != 2)
        usage(argv[0]);

    // Konwersja argumentu na liczbę całkowitą (rozmiar planszy)
    const int N = atoi(argv[1]);
    // Walidacja: N musi być co najmniej 3 i mniejsze niż 100
    if(N < 3 || N >= 100)
        usage(argv[0]);

    // Pobranie PID (identyfikatora) procesu i ustawienie ziarna generatora losowego na podstawie PID
    const pid_t pid = getpid();
    srand(pid);

    // Wypisanie PID do stdout, co ułatwia identyfikację serwera (używane przez klientów)
    printf("My PID is %d\n", pid);

    int shm_fd;
    char shm_name[32];
    // Utworzenie unikalnej nazwy segmentu pamięci na podstawie PID, np.: "/12345-board"
    sprintf(shm_name, "/%d-board", pid);

    // Utworzenie segmentu pamięci współdzielonej przy użyciu shm_open z flagami O_CREAT i O_EXCL
    if((shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666)) == -1) 
        ERR("shm_open");
    // Ustawienie rozmiaru segmentu pamięci na SHM_SIZE bajtów
    if(ftruncate(shm_fd, SHM_SIZE) == -1) 
        ERR("ftruncate");

    // Mapowanie segmentu pamięci współdzielonej do przestrzeni adresowej procesu
    char* shm_ptr;
    if((shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        ERR("mmap");

    // Pierwsza część zmapowanej pamięci przeznaczona jest na mutex synchronizujący dostęp do planszy
    pthread_mutex_t *mutex = (pthread_mutex_t*)shm_ptr;
    // Następny bajt pamięci przechowuje rozmiar planszy (N), zapisany jako pojedynczy bajt
    char* N_shared = (shm_ptr + sizeof(pthread_mutex_t));
    // Kolejna część pamięci jest przeznaczona na planszę; elementy planszy będą miały wartości punktowe
    char* board = (shm_ptr + sizeof(pthread_mutex_t) + 1);
    // Zapisanie rozmiaru planszy do pamięci współdzielonej
    N_shared[0] = N; // przechowujemy N jako jeden bajt (wartość od 0 do 255)

    // Inicjalizacja planszy: dla każdego pola losowo przypisujemy wartość od 1 do 9
    for(int i = 0; i < N; i++) 
    {
        for(int j = 0; j < N; j++) 
        {
            board[i * N + j] = 1 + rand() % 9;    
        }
    }

    // Inicjalizacja atrybutów mutexa
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    // Ustawienie flagi umożliwiającej współdzielenie mutexa między procesami
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    // Ustawienie atrybutu "robust", co pozwala na wykrycie sytuacji, gdy właściciel mutexa uległ awarii
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
    // Inicjalizacja mutexa znajdującego się w pamięci współdzielonej z ustawionymi wcześniej atrybutami
    pthread_mutex_init(mutex, &mutex_attr);

    // Inicjalizacja struktury argumentów dla wątku obsługi sygnałów
    sighandling_args_t sighandling_args = { 1, PTHREAD_MUTEX_INITIALIZER };

    // Konfiguracja zestawu sygnałów - tworzymy pusty zestaw i dodajemy do niego SIGINT
    sigemptyset(&sighandling_args.new_mask);
    sigaddset(&sighandling_args.new_mask, SIGINT);
    // Blokujemy sygnał SIGINT dla głównego wątku, aby został przechwycony przez wątek signal_handling
    if(pthread_sigmask(SIG_BLOCK, &sighandling_args.new_mask, &sighandling_args.old_mask))
        ERR("SIG_BLOCK error");

    // Tworzenie wątku do obsługi sygnału SIGINT
    pthread_t sighandling_thread;
    pthread_create(&sighandling_thread, NULL, signal_handling, &sighandling_args);

    // Główna pętla serwera: wyświetlanie planszy co 3 sekundy
    while(1) 
    {
        // Blokujemy mutex związanego z flagą running, aby sprawdzić stan działania
        pthread_mutex_lock(&sighandling_args.mutex);
        // Jeśli flaga running została ustawiona na 0 (np. przez odebranie SIGINT), wychodzimy z pętli
        if(!sighandling_args.running)
            break;
        // Odblokowujemy mutex z flagą running przed przejściem do dalszych operacji
        pthread_mutex_unlock(&sighandling_args.mutex);

        int error;
        // Próba zablokowania współdzielonego mutexa, aby uzyskać wyłączny dostęp do planszy
        if((error = pthread_mutex_lock(mutex)) != 0)
        {
            // Jeśli mutex został porzucony przez poprzedniego właściciela (EOWNERDEAD), przywracamy jego spójność
            if(error == EOWNERDEAD)
                pthread_mutex_consistent(mutex);
            else
                ERR("pthread_mutex_lock");
        }

        // Wyświetlanie planszy - iteracja po wszystkich wierszach i kolumnach
        for(int i = 0; i < N; i++) 
        {
            for(int j = 0; j < N; j++)
            {
                // Wypisujemy wartość punktową pola bez odstępów między cyframi
                printf("%d", board[i * N + j]);
            }
            // Po zakończeniu wiersza wypisujemy znak nowej linii
            putchar('\n');
        }

        // Dodatkowy znak nowej linii, aby oddzielić kolejne wyświetlenie planszy
        putchar('\n');

        // Odblokowanie mutexa, aby inni procesy lub wątki mogły korzystać z planszy
        pthread_mutex_unlock(mutex);

        // Opóźnienie 3-sekundowe przed kolejnym wyświetleniem planszy
        struct timespec t = { 3, 0 };
        nanosleep(&t, NULL);
    }

    // Oczekiwanie na zakończenie wątku obsługi sygnału SIGINT
    pthread_join(sighandling_thread, NULL);

    // Sprzątanie zasobów: niszczenie atrybutów mutexa i samego mutexa
    pthread_mutexattr_destroy(&mutex_attr);
    pthread_mutex_destroy(mutex);

    // Odmapowanie segmentu pamięci współdzielonej
    munmap(shm_ptr, SHM_SIZE);
    // Usunięcie segmentu pamięci współdzielonej (shm_unlink) na podstawie nazwy
    shm_unlink(shm_name);

    return EXIT_SUCCESS;
}
