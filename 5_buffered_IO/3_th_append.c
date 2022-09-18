#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

struct th_data {
    int symb_start, symb_end;
};

void *th_work(struct th_data *data) {
    FILE *f = fopen("output.txt", "a");
    if (f == NULL) {
        printf("Error while fopen\n");
        exit(1);
    }

    struct stat s;
    int res = stat("output.txt", &s);
    if (res == 1)
        exit(1);

    int pos = ftell(f);
    printf("fopen: inode = %ld, size = %ld, pos = %d\n", s.st_ino, s.st_size, pos);

    for (int i = data->symb_start; i <= data->symb_end; i++)
        fprintf(f, "%c", i);

    pos = ftell(f);

    fclose(f);
    res = stat("output.txt", &s);
    if (res == 1)
        exit(1);

    printf("fclose: inode = %ld, size = %ld, pos = %d\n", s.st_ino, s.st_size, pos);
}

int main() {
    pthread_t th1_id, th2_id;

    struct th_data th1_data = {
            .symb_start = '0',
            .symb_end = '9'
    }, th2_data = {
            .symb_start = 'a',
            .symb_end = 'z'
    };

    pthread_create(&th1_id, NULL, (void *(*)(void *)) th_work, &th1_data);
    pthread_create(&th2_id, NULL, (void *(*)(void *)) th_work, &th2_data);

    pthread_join(th1_id, NULL);
    pthread_join(th2_id, NULL);

    return 0;
}
