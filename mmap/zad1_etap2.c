#include <errno.h>      // Definicje błędów systemowych
#include <fcntl.h>      // Funkcje do operacji na plikach (np. open)
#include <stdio.h>      // Standardowe funkcje wejścia/wyjścia (printf, perror)
#include <stdlib.h>     // Funkcje standardowe (exit)
#include <string.h>     // Funkcje operujące na łańcuchach znakowych
#include <sys/mman.h>   // Funkcje do mapowania pamięci (mmap, munmap)
#include <sys/wait.h>   // Funkcje do obsługi procesów potomnych (wait)
#include <unistd.h>     // Funkcje systemowe (close, etc.)

// Makro obsługujące błędy: wypisuje komunikat z informacją o błędzie, zabija procesy potomne i kończy działanie programu.
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

// Rozmiar mapowanej pamięci
#define SHM_SIZE 1024
// Liczba liter alfabetu (od a do z)
#define ALF_SIZE 26

int main(int argc, char **argv) 
{
    int fd;
    // Otwieramy plik "file.txt" w trybie do odczytu i zapisu
    if((fd = open("./file.txt", O_RDWR)) == -1)
        ERR("open");

    // Mapujemy zawartość pliku do pamięci
    // Używamy trybu PROT_READ aby tylko odczytywać dane, MAP_SHARED - zmiany są widoczne dla innych
    char *data;
    if((data = (char*)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)
        ERR("mmap");

    // Tablica do zliczania wystąpień liter alfabetu
    int char_count[ALF_SIZE] = { 0 };

    int i = 0;
    // Przechodzimy przez zmapowane dane aż do napotkania znaku końca łańcucha ('\0')
    while(1)
    {
        char c = data[i++];
        if(c == '\0')
            break;

        // Jeśli znak mieści się w zakresie 'a' do 'z', zliczamy wystąpienie
        if(c >= 'a' && c <= 'z')
            char_count[c - 'a']++;  // Indeks tablicy odpowiada literom: 'a' -> indeks 0, 'b' -> indeks 1, itd.
    }

    // Wypisujemy wynik zliczania – dla każdej litery, która wystąpiła przynajmniej raz, wypisujemy literę i liczbę wystąpień
    for(int i = 0; i < ALF_SIZE; i++)
    {
        if(char_count[i] > 0)
        {
            printf("%c: %d\n", 'a' + i, char_count[i]);
        }
    }
    
    // Zwalniamy zmapowaną pamięć (można dodać munmap oraz close, jeśli chcemy poprawnie sprzątać zasoby)
    // munmap(data, SHM_SIZE);
    // close(fd);
    
    return EXIT_SUCCESS;
}
