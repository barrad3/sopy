#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char* buf, size_t count) {
    ssize_t c, len = 0;
    do {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0) return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void usage(char* progname) {
    fprintf(stderr, "Usage: %s <file> <number_of_processes>\n", progname);
    fprintf(stderr, "\tfile - file to process\n");
    fprintf(stderr, "\t0 < number_of_processes < 10\n");
    exit(EXIT_FAILURE);
}

volatile sig_atomic_t last_signal = 0;

void child_sigusr1_handler(int sig) {
    last_signal = 1;
}

void child_work(char *buf, ssize_t buf_size, int child_num) {
    sigset_t mask;
    sigemptyset(&mask);
    while (!last_signal) {
        sigsuspend(&mask);  // Czekanie na sygnał SIGUSR1
    }
    printf("Child PID: %d, assigned fragment: %.*s\n", getpid(), (int)buf_size, buf);
    free(buf);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
    if (argc != 3) usage(argv[0]);
    
    int n = atoi(argv[2]);
    if (n <= 0 || n >= 10) usage(argv[0]);

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) ERR("open");

    int file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    size_t buf_size = file_size / n + (file_size % n != 0);
    pid_t children[n];

    sethandler(child_sigusr1_handler, SIGUSR1);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    for (int i = 0; i < n; i++) {
        char* buf = malloc(buf_size);
        if (!buf) ERR("malloc");

        ssize_t child_buf_size = bulk_read(fd, buf, buf_size);
        if (child_buf_size < 0) ERR("bulk_read");

        pid_t pid = fork();
        if (pid == 0) {
            child_work(buf, child_buf_size, i + 1);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            ERR("fork");
        }
        children[i] = pid;
        free(buf);
    }

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    sethandler(SIG_IGN, SIGUSR1);

    for (int i = 0; i < n; i++) {
        kill(children[i], SIGUSR1);
    }

    while (wait(NULL) > 0);
    close(fd);
    return EXIT_SUCCESS;
}
