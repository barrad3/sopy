#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_KNIGHT_NAME_LENGTH 20

typedef struct knight
{
    char name[MAX_KNIGHT_NAME_LENGTH];
    int hp;
    int attack;
} knight;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void knight_work()
{
    
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    FILE* f_franci = fopen("franci.txt", "r");
    FILE* f_saraceni = fopen("saraceni.txt", "r");

    if (!f_franci)
    {
        ERR("Franks have not arrived on the battlefield");
    }
    if (!f_saraceni)
    {
        ERR("Saracens have not arrived on the battlefield");
    }
    int count;
    if (fscanf(f_franci, "%d", &count) != 1)
    {
        ERR("Wrong number");
    }
    knight Franks[count];
    for (int i = 0; i < count; i++)
    {
        if (fscanf(f_franci, "%s %d %d", Franks[i].name, &Franks[i].hp, &Franks[i].attack) == 3)
        {
            printf("I am Frankish knight %s. I will serve my king with my %d HP and %d attack.\n", Franks[i].name,
                   Franks[i].hp, Franks[i].attack);
        }
        else
        {
            ERR("Wrong name/hp/attack");
        }
    }
    fclose(f_franci);

    if (fscanf(f_saraceni, "%d", &count) != 1)
    {
        ERR("Wrong number");
    }
    knight Saracens[count];
    for (int i = 0; i < count; i++)
    {
        if (fscanf(f_saraceni, "%s %d %d", Saracens[i].name, &Saracens[i].hp, &Saracens[i].attack) == 3)
        {
            printf("I am Spanish knight %s. I will serve my king with my %d HP and %d attack.\n", Saracens[i].name,
                   Saracens[i].hp, Saracens[i].attack);
        }
        else
        {
            ERR("Wrong name/hp/attack");
        }
    }
    fclose(f_saraceni);

    printf("Opened descriptors: %d\n", count_descriptors());
}
