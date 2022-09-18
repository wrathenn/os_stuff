#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

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
            perror("Can't fork");
            return fork_failure;
        }
        else if (child_pid == 0) {
            sleep(SLEEP_TIME);
            printf("Для потомка №%d --- PID: %d, PPID: %d, GROUP: %d\n", i + 1, getpid(), getppid(), getpgrp());
            return 0;
        }
        else {
            children[i] = child_pid;
        }
    }

    printf("Процессы, созданные предком:\n");
    for (int i = 0; i < PROC_COUNT; ++i) {
        int status;
        int stat_value = 0;

        pid_t child_pid = wait(&status);
        printf("Потомок с PID = %d завершился. Статус: %d\n", children[i], status);

        if (WIFEXITED(stat_value)) {
            printf("\tПотомок завершился нормально. Код завершения: %d\n", WEXITSTATUS(stat_value));
        }
        else if (WIFSIGNALED(stat_value)) {
            printf("\tПотомок завершился неперехватываемым сигналом. Номер сигнала: %d\n", WTERMSIG(stat_value));
        }
        else if (WIFSTOPPED(stat_value)) {
            printf("\tПотомок остановился. Номер сигнала: %d\n", WSTOPSIG(stat_value));
        }
    }
    printf("Предок завершился\n");

    return no_error;
}