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
    // Studenci najpierw wypisują swój PID
    printf("Student: %d\n", getpid());
    fflush(stdout);

    // Odczytanie zapytania od nauczyciela z indywidualnego potoku
    ssize_t count = read(teacher_to_child_fd, buffer, sizeof(buffer) - 1);
    if (count < 0)
        ERR("child read");
    buffer[count] = '\0';
    // Możemy opcjonalnie przetworzyć odebrany komunikat, ale w tym zadaniu nie jest to konieczne

    // Przygotowanie odpowiedzi
    char response[256];
    snprintf(response, sizeof(response), "Student %d: HERE!", getpid());
    // Wypisanie odpowiedzi na standardowe wyjście
    printf("%s\n", response);
    fflush(stdout);
    // Wysłanie odpowiedzi do nauczyciela przez wspólny potok
    if (write(common_write_fd, response, strlen(response) + 1) < 0)
        ERR("child write to common pipe");

    close(teacher_to_child_fd);
    close(common_write_fd);
    exit(EXIT_SUCCESS);
}

// Funkcja wykonywana przez proces główny (nauczyciel)
void teacher_work(int n) {
    int common_pipe[2]; // Wspólny potok: studenci -> nauczyciel
    if (pipe(common_pipe) < 0)
        ERR("pipe common");

    // Alokacja tablic na PID studentów oraz indywidualne deskryptory zapisu do potoków
    pid_t *child_pids = malloc(n * sizeof(pid_t));
    if (child_pids == NULL)
        ERR("malloc child_pids");
    int *teacher_to_child_fds = malloc(n * sizeof(int));
    if (teacher_to_child_fds == NULL)
        ERR("malloc teacher_to_child_fds");

    // Tworzenie procesów studentów
    for (int i = 0; i < n; i++) {
        int indiv_pipe[2]; // Indywidualny potok dla danego studenta:
                           // [0] – odczyt w procesie dziecka, [1] – zapis w procesie nauczyciela
        if (pipe(indiv_pipe) < 0)
            ERR("pipe individual");

        pid_t pid = fork();
        if (pid < 0) {
            ERR("fork");
        } else if (pid == 0) {
            // Proces potomny (student)
            close(indiv_pipe[1]);      // Dziecko nie potrzebuje końca zapisu indywidualnego
            close(common_pipe[0]);       // Dziecko nie odczytuje z wspólnego potoku
            child_work(indiv_pipe[0], common_pipe[1]);
            // child_work() nigdy nie zwróci
        } else {
            // Proces nauczyciela
            child_pids[i] = pid;
            teacher_to_child_fds[i] = indiv_pipe[1]; // Zapamiętujemy koniec zapisu, przez który będziemy wysyłać zapytania
            close(indiv_pipe[0]); // Nauczyciel nie korzysta z końca odczytu indywidualnego potoku
        }
    }
    // Nauczyciel nie korzysta z zapisu we wspólnym potoku
    close(common_pipe[1]);

    // Opcjonalny delay, aby studenci zdążyli wypisać swoje PID
    sleep(1);

    // Nauczyciel wypisuje swój PID
    printf("Teacher: %d\n", getpid());
    fflush(stdout);

    char query[256];
    char response[256];

    // Dla każdego studenta wysyłamy zapytanie i odbieramy odpowiedź
    for (int i = 0; i < n; i++) {
        snprintf(query, sizeof(query), "Teacher: Is %d here?", child_pids[i]);
        // Wypisanie zapytania na standardowe wyjście
        printf("%s\n", query);
        fflush(stdout);
        // Wysłanie zapytania do konkretnego studenta przez indywidualny potok
        if (write(teacher_to_child_fds[i], query, strlen(query) + 1) < 0)
            ERR("teacher write to individual pipe");
        close(teacher_to_child_fds[i]);

        // Odbieranie odpowiedzi ze wspólnego potoku
        ssize_t count = read(common_pipe[0], response, sizeof(response) - 1);
        if (count < 0)
            ERR("teacher read common pipe");
        response[count] = '\0';
        printf("Received: %s\n", response);
        fflush(stdout);
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
