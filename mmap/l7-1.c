#define _GNU_SOURCE

#include <errno.h>      // Definicje błędów systemowych
#include <fcntl.h>      // Funkcje do obsługi plików (open, O_CREAT, ...)
#include <stdio.h>      // Standardowe funkcje wejścia/wyjścia (printf, fprintf, perror)
#include <stdlib.h>     // Funkcje standardowe (atoi, exit, srand, rand)
#include <string.h>     // Operacje na łańcuchach i pamięci (memcpy, strerror)
#include <sys/mman.h>   // Funkcje do mapowania pamięci (mmap, munmap, msync)
#include <sys/wait.h>   // Funkcje do obsługi czekania na procesy (wait)
#include <unistd.h>     // Funkcje systemowe (fork, getpid, close, ftruncate)

// Liczba iteracji dla metody Monte Carlo
#define MONTE_CARLO_ITERS 100000
// Długość pojedynczego wpisu w logu (ilość znaków)
#define LOG_LEN 8

// Makro do obsługi błędów, wypisuje komunikat błędu, zabija wszystkie procesy potomne i kończy program
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

/**
 * Funkcja wykonywana przez proces potomny.
 * Oblicza przybliżenie wartości liczby Pi metodą Monte Carlo.
 * Wynik zapisywany jest w tablicy out pod indeksem n, a także logowany do segmentu log.
 */
void child_work(int n, float* out, char* log)
{
    int sample = 0;           // Licznik punktów, które znalazły się wewnątrz ćwiartki koła
    srand(getpid());          // Ustawienie ziarna losowości na podstawie PID procesu
    int iters = MONTE_CARLO_ITERS; // Liczba prób Monte Carlo

    // Pętla wykonująca symulacje losowe
    while (iters-- > 0)
    {
        // Generujemy losowe punkty w zakresie [0, 1]
        double x = ((double)rand()) / RAND_MAX;
        double y = ((double)rand()) / RAND_MAX;
        // Sprawdzamy, czy punkt znajduje się wewnątrz ćwiartki koła o promieniu 1
        if (x * x + y * y <= 1.0)
            sample++;       // Jeśli tak, zwiększamy licznik
    }

    // Obliczamy przybliżenie stosunku punktów wewnątrz koła do ogólnej liczby prób
    out[n] = ((float)sample) / MONTE_CARLO_ITERS;

    // Bufor do formatu logu; dodajemy miejsce na znak końca linii
    char buf[LOG_LEN + 1];

    // Przygotowujemy wpis do logu: skalujemy wartość przez 4, aby przybliżyć wartość liczby Pi
    // Format: 7 znaków, 5 cyfr po przecinku i kończący znak nowej linii (zapisujemy tylko LOG_LEN znaków)
    snprintf(buf, LOG_LEN + 1, "%7.5f\n", out[n] * 4.0f);
    // Kopiujemy przygotowany wpis do segmentu log, w odpowiednią pozycję (dla danego procesu potomnego)
    memcpy(log + n * LOG_LEN, buf, LOG_LEN);
}

/**
 * Funkcja wykonywana przez proces rodzica.
 * Oczekuje na zakończenie wszystkich procesów potomnych, zbiera wyniki i oblicza końcowe przybliżenie liczby Pi.
 */
void parent_work(int n, float* data)
{
    pid_t pid;
    double sum = 0.0;

    // Oczekiwanie na wszystkie procesy potomne
    for (;;)
    {
        pid = wait(NULL);
        if (pid <= 0)
        {
            // Jeśli nie ma już procesów potomnych (errno == ECHILD), wychodzimy z pętli
            if (errno == ECHILD)
                break;
            ERR("waitpid"); // W przypadku innego błędu przerywamy działanie
        }
    }

    // Sumujemy wyniki zwrócone przez każdy proces potomny
    for (int i = 0; i < n; i++)
        sum += data[i];
    // Obliczamy średnią z przybliżeń
    sum = sum / n;

    // Skalujemy wynik przez 4, aby otrzymać końcowe przybliżenie liczby Pi i wypisujemy wynik
    printf("Pi is approximately %f\n", sum * 4);
}

/**
 * Funkcja tworząca n procesów potomnych. Każdy proces wywołuje funkcję child_work.
 */
void create_children(int n, float* data, char* log)
{
    for (int i = 0; i < n; i++)
    {
        switch (fork())
        {
            case 0:
                // Proces potomny: wykonuje obliczenia i wychodzi po zakończeniu
                child_work(i, data, log);
                exit(EXIT_SUCCESS);
            case -1:
                // W przypadku błędu podczas fork(), wypisujemy komunikat błędu i kończymy program
                perror("Fork:");
                exit(EXIT_FAILURE);
            // Proces rodzica kontynuuje pętlę, tworząc kolejne procesy potomne
        }
    }
}

/**
 * Funkcja wyświetlająca instrukcję użycia programu oraz kończąca działanie.
 */
void usage(char* name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "10000 >= n > 0 - number of children\n");
    exit(EXIT_FAILURE);
}

/**
 * Funkcja główna programu.
 * Przyjmuje jako argument liczbę procesów potomnych (n), tworzy segment pamięci współdzielonej do logowania i tablicę wyników,
 * uruchamia procesy potomne oraz zbiera ich wyniki i wyświetla końcowe przybliżenie liczby Pi.
 */
int main(int argc, char** argv)
{
    int n;
    // Sprawdzamy, czy podano dokładnie jeden argument (oprócz nazwy programu)
    if (argc != 2)
        usage(argv[0]);
    n = atoi(argv[1]);
    // Walidacja: n musi być większe od 0 i nie większe niż 30 (choć komunikat sugeruje 10000, warunek jest inny)
    if (n <= 0 || n > 30)
        usage(argv[0]);

    int log_fd;
    // Otwieramy plik log.txt, w którym będziemy zapisywać logi; używamy O_CREAT do tworzenia, O_RDWR do odczytu i zapisu,
    // oraz O_TRUNC do wyczyszczenia pliku, jeśli już istnieje.
    if ((log_fd = open("./log.txt", O_CREAT | O_RDWR | O_TRUNC, -1)) == -1)
        ERR("open");
    // Ustawiamy rozmiar pliku log.txt na n * LOG_LEN bajtów
    if (ftruncate(log_fd, n * LOG_LEN))
        ERR("ftruncate");

    // Mapujemy plik log.txt do pamięci współdzielonej, aby procesy potomne mogły zapisywać logi bez konieczności używania pliku
    char* log;
    if ((log = (char*)mmap(NULL, n * LOG_LEN, PROT_WRITE | PROT_READ, MAP_SHARED, log_fd, 0)) == MAP_FAILED)
        ERR("mmap");
    // Zamykamy deskryptor pliku, bo nie jest już potrzebny – mamy mapowanie pamięci
    if (close(log_fd))
        ERR("close");

    // Tworzymy anonimowy fragment pamięci współdzielonej dla przechowywania wyników obliczeń z procesów potomnych
    float* data;
    if ((data = (float*)mmap(NULL, n * sizeof(float), PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
        ERR("mmap");

    // Tworzymy procesy potomne, które wykonają obliczenia
    create_children(n, data, log);
    // Proces rodzica oczekuje na zakończenie procesów potomnych i zbiera wyniki
    parent_work(n, data);

    // Sprzątanie: odmapowujemy pamięć dla wyników
    if (munmap(data, n * sizeof(float)))
        ERR("munmap");
    // Synchronizujemy zapisany log (zapewniamy, że dane zostaną zapisane do pliku)
    if (msync(log, n * LOG_LEN, MS_SYNC))
        ERR("msync");
    // Odmapowujemy pamięć dla logu
    if (munmap(log, n * LOG_LEN))
        ERR("munmap");

    return EXIT_SUCCESS;
}
