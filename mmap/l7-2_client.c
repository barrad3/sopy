#define _GNU_SOURCE

#include <errno.h>       // Definicje komunikatów błędów
#include <fcntl.h>       // Funkcje do obsługi plików (open, O_RDWR itd.)
#include <pthread.h>     // Funkcje i typy związane z wątkami i synchronizacją
#include <signal.h>      // Definicje i funkcje obsługi sygnałów
#include <stdio.h>       // Standardowe funkcje wejścia/wyjścia (printf, fprintf, perror)
#include <stdlib.h>      // Standardowe funkcje (atoi, exit, srand, rand)
#include <string.h>      // Funkcje do operacji na łańcuchach i pamięci (memcpy, itp.)
#include <sys/mman.h>    // Funkcje do mapowania pamięci (mmap, munmap)
#include <sys/stat.h>    // Funkcje do obsługi statystyk plików
#include <sys/types.h>   // Definicje typów systemowych
#include <unistd.h>      // Funkcje systemowe (fork, getpid, close itd.)

// Makro obsługujące błędy: wypisuje informację o błędzie, zabija wszystkie procesy potomne i kończy działanie
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

// Rozmiar segmentu pamięci współdzielonej
#define SHM_SIZE 1024

/**
 * Funkcja wypisująca sposób użycia programu i kończąca działanie, jeśli argumenty są niepoprawne.
 */
void usage(char* name)
{
    fprintf(stderr, "USAGE: %s server_pid\n", name);
    exit(EXIT_FAILURE);
}

/**
 * Funkcja główna klienta, który łączy się z już utworzonym przez serwer segmentem pamięci współdzielonej.
 * W pamięci tej znajduje się mutex synchronizujący dostęp oraz plansza z punktami.
 */
int main(int argc, char **argv) 
{
    // Sprawdzenie, czy podany został dokładnie jeden argument (PID serwera)
    if(argc != 2)
        usage(argv[0]);

    // Konwersja argumentu na liczbę (PID serwera)
    const int server_pid = atoi(argv[1]);
    if(server_pid == 0)
        usage(argv[0]);

    // Ustawienie ziarna dla generatora liczb losowych na podstawie PID, aby wyniki były unikalne
    srand(getpid());

    int shm_fd;
    char shm_name[32];
    // Utworzenie nazwy segmentu pamięci współdzielonej, który został utworzony przez serwer.
    sprintf(shm_name, "/%d-board", server_pid);

    // Otwieranie segmentu pamięci współdzielonej w trybie odczytu i zapisu.
    if((shm_fd = shm_open(shm_name, O_RDWR, 0666)) == -1)
        ERR("shm_open");

    // Mapowanie segmentu pamięci współdzielonej do przestrzeni adresowej klienta.
    char* shm_ptr;
    if((shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        ERR("mmap");

    // Pierwszy fragment pamięci zawiera mutex, który synchronizuje dostęp do planszy.
    pthread_mutex_t* mutex = (pthread_mutex_t*)shm_ptr;
    // Następny bajt w pamięci zawiera rozmiar planszy (N) zapisany jako pojedynczy bajt.
    char* N_shared = (shm_ptr + sizeof(pthread_mutex_t));
    // Kolejna część pamięci zawiera samą planszę, gdzie każdy element reprezentuje wartość punktową pola.
    char* board = (shm_ptr + sizeof(pthread_mutex_t)) + 1;
    // Odczytujemy rozmiar planszy zapisany w pamięci (zakładamy wartość od 0 do 255).
    const int N = N_shared[0];

    int score = 0;  // Zmienna przechowująca aktualny wynik (zdobyte punkty)

    // Główna pętla klienta – wykonywana do momentu zakończenia gry
    while(1) 
    {
        int error;
        // Blokowanie mutexa, aby uzyskać wyłączny dostęp do planszy (sekcja krytyczna)
        if((error = pthread_mutex_lock(mutex)) != 0)
        {
            // W przypadku, gdy poprzedni właściciel mutexa przestał działać, przywracamy spójność mutexa
            if(error == EOWNERDEAD)
                pthread_mutex_consistent(mutex);
            else
                ERR("pthread_mutex_lock");
        }

        // Losowy wybór akcji: generujemy liczbę z zakresu 1-9
        const int D = 1 + rand() % 9;
        // Jeśli wylosowana liczba wynosi 1, symulujemy sytuację awaryjną i kończymy działanie gry
        if(D == 1) 
        {
            printf("Oops...\n");
            exit(EXIT_SUCCESS);
        }

        // Losowy wybór współrzędnych (x, y) na planszy o rozmiarze N x N
        int x = rand() % N, y = rand() % N;
        printf("trying to search field (%d, %d)\n", x, y);

        // Pobieramy wartość punktową z wylosowanego pola
        const int p = board[N * y + x];
        // Jeśli pole jest już puste (wartość 0), gra się kończy
        if(p == 0)
        {
            printf("GAME OVER: score %d\n", score);
            // Odblokowanie mutexa przed zakończeniem działania
            pthread_mutex_unlock(mutex);
            break;
        }
        else 
        {
            // Jeśli pole zawiera punkty, dodajemy je do wyniku i "czyszczymy" pole (ustawiając jego wartość na 0)
            printf("found %d points\n", p);
            score += p;
            board[N * y + x] = 0;
        }

        // Odblokowanie mutexa, by umożliwić dostęp do planszy innym klientom/procesom
        pthread_mutex_unlock(mutex);

        // Opóźnienie o 1 sekundę przed kolejną iteracją, by symulować czas potrzebny na wykonanie akcji
        struct timespec t = { 1, 0 };
        nanosleep(&t, 0);
    }

    // Zwalnianie zmapowanego segmentu pamięci współdzielonej
    munmap(shm_ptr, SHM_SIZE);

    return EXIT_SUCCESS;
}
