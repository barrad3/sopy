#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#define ERR(source) (perror(source), exit(EXIT_FAILURE))

// Funkcja wykonywana przez proces potomny (student)
// teacher_to_child_fd – deskryptor do odczytu komunikatu od nauczyciela (indywidualny potok)
// common_write_fd      – deskryptor do zapisu odpowiedzi do wspólnego potoku
void child_work(int teacher_to_child_fd, int common_write_fd) {
    char buffer[256];
    // Odczyt komunikatu od nauczyciela
    ssize_t count = read(teacher_to_child_fd, buffer, sizeof(buffer) - 1);
    if (count < 0) ERR("child read");
    buffer[count] = '\0';
    // Możemy opcjonalnie coś zrobić z odebranym komunikatem (tutaj nie jest to wymagane)

    // Przygotowanie odpowiedzi
    pid_t pid = getpid();
    char response[256];
    snprintf(response, sizeof(response), "Student %d: HERE", pid);
    // Wypisanie odpowiedzi na standardowe wyjście
    printf("%s\n", response);
    fflush(stdout);
    // Wysłanie odpowiedzi do nauczyciela przez wspólny potok
    if (write(common_write_fd, response, strlen(response) + 1) < 0)
        ERR("child write to common pipe");
    
    // Zamykamy deskryptory i kończymy działanie
    close(teacher_to_child_fd);
    close(common_write_fd);
    exit(EXIT_SUCCESS);
}

// Funkcja wykonywana przez proces główny (nauczyciel)
// n – liczba studentów
void teacher_work(int n) {
    int common_pipe[2]; // Wspólny potok: studenci -> nauczyciel
    if (pipe(common_pipe) < 0)
        ERR("pipe common");

    // Alokacja tablic na PID studentów oraz indywidualne potoki (deskryptory do zapisu)
    pid_t *child_pids = malloc(n * sizeof(pid_t));
    if (child_pids == NULL) ERR("malloc");
    int *teacher_to_child_fds = malloc(n * sizeof(int));
    if (teacher_to_child_fds == NULL) ERR("malloc");

    // Tworzymy procesy studentów
    for (int i = 0; i < n; i++) {
        int indiv_pipe[2]; // Indywidualny potok dla danego studenta: [0] – odczyt w dziecku, [1] – zapis w nauczycielu
        if (pipe(indiv_pipe) < 0)
            ERR("pipe individual");

        pid_t pid = fork();
        if (pid < 0) {
            ERR("fork");
        } else if (pid == 0) {
            // Proces potomny (student)
            // W dziecku zamykamy niepotrzebny koniec zapisu indywidualnego potoku oraz odczyt wspólnego potoku
            close(indiv_pipe[1]);
            close(common_pipe[0]); // Dziecko nie odczytuje z potoku wspólnego
            child_work(indiv_pipe[0], common_pipe[1]);
            // Funkcja child_work() nigdy nie zwraca
        } else {
            // Proces nauczyciela
            child_pids[i] = pid;
            teacher_to_child_fds[i] = indiv_pipe[1]; // Zapisujemy koniec zapisu, którym będziemy wysyłać komunikaty
            close(indiv_pipe[0]); // Nauczyciel nie korzysta z końca odczytu indywidualnego potoku
        }
    }
    // Nauczyciel nie potrzebuje zapisywać do wspólnego potoku, dlatego zamykamy jego zapis
    close(common_pipe[1]);

    // Wysyłanie komunikatu sprawdzającego obecność do każdego studenta
    for (int i = 0; i < n; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Teacher: Is %d here?", child_pids[i]);
        // Wypisanie komunikatu na standardowe wyjście
        printf("%s\n", msg);
        fflush(stdout);
        // Wysłanie komunikatu przez indywidualny potok do studenta
        if (write(teacher_to_child_fds[i], msg, strlen(msg) + 1) < 0)
            ERR("teacher write to individual pipe");
        close(teacher_to_child_fds[i]); // Po wysłaniu komunikatu zamykamy deskryptor
    }

    // Odbieramy odpowiedzi od studentów przez wspólny potok
    for (int i = 0; i < n; i++) {
        char response[256];
        ssize_t count = read(common_pipe[0], response, sizeof(response));
        if (count < 0)
            ERR("teacher read common pipe");
        printf("Received: %s\n", response);
    }
    close(common_pipe[0]);

    // Oczekiwanie na zakończenie wszystkich procesów studentów
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    free(child_pids);
    free(teacher_to_child_fds);
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

    teacher_work(n);
    return EXIT_SUCCESS;
}
