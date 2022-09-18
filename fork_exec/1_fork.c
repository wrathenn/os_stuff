#include <stdio.h>
#include <stdlib.h>

enum error_t {
    no_error,
    fork_failure
};

#define PROC_COUNT 2
#define SLEEP_TIME 1

int main() {
    int children[PROC_COUNT];
    printf("\nПредок --- PID: %d, GROUP: %d\n", getpid(), getpgrp());

    for (int i = 0; i < PROC_COUNT; ++i) {
        int child_pid = fork();

        if (child_pid == -1) {
            perror("Ошибка fork\n");
            return fork_failure;
        }
        else if (child_pid == 0) {
            printf("Для потомка до усыновления №%d --- PID: %d, PPID: %d, GROUP: %d\n", i + 1, getpid(), getppid(), getpgrp());
            sleep(SLEEP_TIME * 4);
            printf("\nДля потомка после усыновления №%d --- PID: %d, PPID: %d, GROUP: %d\n", i + 1, getpid(), getppid(), getpgrp());
            return no_error;
        }
        else {
            sleep(SLEEP_TIME);
            children[i] = child_pid;
        }
    }

    printf("Процессы, созданные предком:\n");
    for (int i = 0; i < PROC_COUNT; ++i) {
        printf("Потомок №%d --- PID: %d, ", i + 1, children[i]);
    }
    printf("\nПредок завершился\n");

    return no_error;
}
