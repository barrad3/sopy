#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define ERR(source) (perror(source), exit(EXIT_FAILURE))

// Funkcja wykonywana przez proces potomny (student)
void child_work() {
    printf("Student: PID = %d\n", getpid());
    exit(EXIT_SUCCESS);
}

// Funkcja wykonywana przez proces główny (nauczyciel)
// Tworzy n procesów potomnych i oczekuje na ich zakończenie
void teacher_work(int n) {
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        else if (pid == 0) {
            // Proces potomny
            child_work();
        }
        // Proces rodzica kontynuuje pętlę tworzenia dzieci
    }

    // Proces nauczyciela oczekuje na zakończenie wszystkich studentów
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    printf("Nauczyciel (PID = %d) zakończył tworzenie procesów.\n", getpid());
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
