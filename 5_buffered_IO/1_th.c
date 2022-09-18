#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>

int fd;

void *th_work()
{
    FILE *fs = fdopen(fd, "r");
    char buf[20];
    setvbuf(fs, buf, _IOFBF, 20);

    char c;
    while((fscanf(fs, "%c", &c)) == 1)
        fprintf(stdout, "%c", c);
}

int main()
{
    fd = open("alphabet.txt", O_RDONLY);

    pthread_t th1_id, th2_id;
    pthread_create(&th1_id, NULL, th_work, NULL);
    pthread_create(&th2_id, NULL, th_work, NULL);
    pthread_join(th1_id, NULL);
    pthread_join(th2_id, NULL);
}
