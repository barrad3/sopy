#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>

// Makro do obsługi błędów
#define ERR(source) (perror(source), exit(EXIT_FAILURE))

// Liczby punktowe dla kolejnych etapów
const int stage_points[4] = {3, 6, 7, 5};

// Struktura komunikatu wysyłanego przez studenta do nauczyciela
struct stage_msg {
    pid_t pid;
    int stage;    // numer etapu (1..4)
    int attempt;  // wynik próby = k + q
};

// Funkcja pomocnicza generująca liczbę losową z zakresu [a, b]
int rand_range(int a, int b) {
    return a + rand() % (b - a + 1);
}

// Funkcja wykonywana przez proces potomny (studenta) w etapie 3
// teacher_fd - deskryptor indywidualnego potoku (od nauczyciela do studenta)
// common_fd  - deskryptor zapisu do wspólnego potoku (student -> nauczyciel)
void child_work_stage3(int teacher_fd, int common_fd) {
    // Ustawienie losowości (seed na podstawie PID)
    srand(getpid());
    // Losujemy poziom umiejętności k [3,9]
    int k = rand_range(3, 9);
    
    // Dla każdego etapu zadania (1..4)
    for (int stage = 1; stage <= 4; stage++) {
        int passed = 0;
        // Student powtarza próbę, dopóki nie zda danego etapu
        while (!passed) {
            // Losowanie czasu oczekiwania t w milisekundach [100,500]
            int t = rand_range(100, 500);
            // Usypianie (konwersja na mikrosekundy)
            usleep(t * 1000);
            // Losowanie q z zakresu [1,20]
            int q = rand_range(1, 20);
            int attempt = k + q;
            
            // Przygotowanie komunikatu do nauczyciela
            struct stage_msg msg;
            msg.pid = getpid();
            msg.stage = stage;
            msg.attempt = attempt;
            
            // Wysłanie komunikatu do nauczyciela przez wspólny potok
            if (write(common_fd, &msg, sizeof(msg)) != sizeof(msg))
                ERR("child write common");
            
            // Czekanie na odpowiedź od nauczyciela przez indywidualny potok
            char response[32];
            ssize_t r = read(teacher_fd, response, sizeof(response)-1);
            if (r < 0)
                ERR("child read teacher");
            response[r] = '\0';
            
            // Wypisanie otrzymanej odpowiedzi (można także wypisać na stdout, by odzwierciedlić komunikację)
            printf("%s\n", response);
            fflush(stdout);
            
            // Jeśli odpowiedź zaczyna się od 'f' (finished), etap został zaliczony
            if (strncmp(response, "finished", 8) == 0)
                passed = 1;
            // W przeciwnym razie student powtarza próbę tego etapu
        }
    }
    
    // Po ukończeniu wszystkich etapów student wypisuje komunikat końcowy
    printf("Student %d: I NAILED IT!\n", getpid());
    fflush(stdout);
    
    close(teacher_fd);
    close(common_fd);
    exit(EXIT_SUCCESS);
}

// Funkcja wykonywana przez proces główny (nauczyciela) w etapie 3
// n - liczba studentów
// child_pids - tablica PID-ów studentów
// teacher_to_child_stage - tablica deskryptorów zapisu do indywidualnych potoków (nauczyciel -> student)
// common_stage_fd - deskryptor odczytu ze wspólnego potoku (student -> nauczyciel)
void teacher_work_stage3(int n, pid_t *child_pids, int *teacher_to_child_stage, int common_stage_fd) {
    // Ustawienie losowości w procesie nauczyciela
    srand(getpid());
    
    // Dla każdego etapu (1..4)
    for (int stage = 1; stage <= 4; stage++) {
        // Tablica śledząca, którzy studenci zaliczyli dany etap
        int finished[n];
        for (int i = 0; i < n; i++) finished[i] = 0;
        int finished_count = 0;
        
        // Dla każdego etapu czekamy, aż wszyscy studenci zaliczą próbę
        while (finished_count < n) {
            // Odczyt komunikatu od jakiegokolwiek studenta
            struct stage_msg msg;
            ssize_t r = read(common_stage_fd, &msg, sizeof(msg));
            if (r < 0)
                ERR("teacher read common");
            if (r != sizeof(msg))
                continue; // pomijamy niepełne komunikaty
            
            // Jeśli komunikat dotyczy bieżącego etapu
            if (msg.stage != stage)
                continue;
            
            // Znajdź indeks studenta wg PID
            int idx = -1;
            for (int i = 0; i < n; i++) {
                if (child_pids[i] == msg.pid) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1)
                continue; // nieznany PID – pomijamy
            
            // Jeżeli student już zaliczył etap, pomijamy ten komunikat
            if (finished[idx])
                continue;
            
            // Obliczenie poziomu trudności: d = (punkty etapu) + losowa liczba z [1,20]
            int d = stage_points[stage - 1] + rand_range(1, 20);
            char reply[32];
            if (msg.attempt >= d) {
                // Student zalicza etap
                finished[idx] = 1;
                finished_count++;
                snprintf(reply, sizeof(reply), "finished stage %d", stage);
                printf("Teacher: Student %d finished stage %d\n", msg.pid, stage);
            } else {
                // Student nie zalicza etapu – musi powtórzyć próbę
                snprintf(reply, sizeof(reply), "fix stage %d", stage);
                printf("Teacher: Student %d needs to fix stage %d\n", msg.pid, stage);
            }
            fflush(stdout);
            // Wysłanie odpowiedzi do studenta przez jego indywidualny potok
            if (write(teacher_to_child_stage[idx], reply, strlen(reply) + 1) < 0)
                ERR("teacher write to student");
        } // koniec while dla etapu
    } // koniec for dla etapów
    
    printf("Teacher: IT'S FINALLY OVER!\n");
    fflush(stdout);
    close(common_stage_fd);
}

// Główna funkcja nauczyciela – tworzy procesy studentów, ustawia potoki dla etapu 3 i uruchamia symulację
void teacher_main(int n) {
    pid_t *child_pids = malloc(n * sizeof(pid_t));
    if (child_pids == NULL)
        ERR("malloc child_pids");
    // Tablica deskryptorów indywidualnych potoków (nauczyciel -> student) dla etapu 3
    int *teacher_to_child_stage = malloc(n * sizeof(int));
    if (teacher_to_child_stage == NULL)
        ERR("malloc teacher_to_child_stage");
    
    // Wspólny potok dla komunikatów od studentów (etap 3)
    int common_stage_pipe[2];
    if (pipe(common_stage_pipe) < 0)
        ERR("pipe common_stage");
    
    // Tworzenie procesów studentów
    for (int i = 0; i < n; i++) {
        // Indywidualny potok dla komunikacji nauczyciel -> student (etap 3)
        int indiv_pipe[2];
        if (pipe(indiv_pipe) < 0)
            ERR("pipe individual stage");
        
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        else if (pid == 0) {
            // Proces potomny (student)
            // W dziecku: zamykamy nieużywane końce potoków
            close(indiv_pipe[1]); // dziecko czyta z indiv_pipe[0]
            close(common_stage_pipe[0]); // dziecko pisze do common_stage_pipe[1]
            // Funkcja dziecka – etap 3. Przekazujemy deskryptory:
            //   indywidualny: do odczytu (indiv_pipe[0])
            //   wspólny: do zapisu (common_stage_pipe[1])
            child_work_stage3(indiv_pipe[0], common_stage_pipe[1]);
            // Funkcja child_work_stage3 nie wraca
        } else {
            // Proces nauczyciela
            child_pids[i] = pid;
            teacher_to_child_stage[i] = indiv_pipe[1]; // nauczyciel pisze przez indiv_pipe[1]
            close(indiv_pipe[0]); // nauczyciel nie czyta z indywidualnego potoku
        }
    }
    // Nauczyciel nie korzysta z zapisu we wspólnym potoku
    close(common_stage_pipe[1]);
    
    // Opcjonalnie – delay, aby studenci zdążyli rozpocząć pracę i ewentualnie wypisać swoje PID
    sleep(1);
    
    // Wypisanie identyfikatora nauczyciela (dla porządku)
    printf("Teacher: %d\n", getpid());
    fflush(stdout);
    
    // Rozpoczęcie etapu rozwiązywania zadań
    teacher_work_stage3(n, child_pids, teacher_to_child_stage, common_stage_pipe[0]);
    
    // Oczekiwanie na zakończenie wszystkich studentów
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    free(child_pids);
    free(teacher_to_child_stage);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s n (gdzie 3 <= n <= 20)\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int n = atoi(argv[1]);
    if (n < 3 || n > 20) {
        fprintf(stderr, "Błędna wartość n. Wartość musi być z przedziału 3-20.\n");
        exit(EXIT_FAILURE);
    }
    
    teacher_main(n);
    return EXIT_SUCCESS;
}
