#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

void *th_work()
{
    char c;
    int fd = open("alphabet.txt", O_RDONLY);

    while (read(fd, &c, 1) == 1 && write(1, &c, 1) == 1);
}

int main() {
    pthread_t th1_id, th2_id;
    pthread_create(&th1_id, NULL, th_work, NULL);
    pthread_create(&th2_id, NULL, th_work, NULL);

    pthread_join(th1_id, NULL);
    pthread_join(th2_id, NULL);

    return 0;
}
