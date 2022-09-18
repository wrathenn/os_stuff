#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <string.h>
#include <signal.h>

enum error_t {
    no_error,
    fork_fail,
    exec_fail,
    pipe_fail
};

#define PROC_COUNT 2
#define SLEEP_TIME 2
#define STR_BUFF_SIZE 64

void do_nothing(int sigint) {
    printf("\n");
}

static int sig_status = 0;

void inc_status(int sigint) {
    sig_status++;
    printf("Статус увеличен - %d\n", sig_status);
}

int main() {
    int pipefd[2];  // [0] - чтение, [1] - запись
    if (pipe(pipefd) == -1) {
        perror("Ошибка pipe\n");
        return pipe_fail;
    }

    char *msgs[PROC_COUNT] = {"First msg\n", "Second msg\n", "Third message\n"};
    char str_buff[STR_BUFF_SIZE] = {0};

    int children[PROC_COUNT];
    printf("Предок --- PID: %d, GROUP: %d\n", getpid(), getpgrp());
    signal(SIGINT, do_nothing);

    for (int i = 0; i < PROC_COUNT; ++i) {
        int child_pid = fork();

        if (child_pid == -1) {
            perror("Ошибка fork'а");
            return fork_fail;
        }
        else if (child_pid == 0) {
            printf("Для потомка №%d --- PID: %d, PPID: %d, GROUP: %d\n", i + 1, getpid(), getppid(), getpgrp());

            signal(SIGINT, inc_status);
            sleep(SLEEP_TIME);

            if (sig_status != 0) {
                close(pipefd[0]);
                write(pipefd[1], msgs[i], strlen(msgs[i]));
                printf("Сообщение №%d отправлено потомку\n\n", i + 1);
            }
            else {
                printf("Сигнала не было\n\n");
            }

            return no_error;
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
            printf("\tПотомок завершился нормально. Код завершения: %d\n\n", WEXITSTATUS(stat_value));
        }
        else if (WIFSIGNALED(stat_value)) {
            printf("\tПотомок завершился неперехватываемым сигналом. Номер сигнала: %d\n\n", WTERMSIG(stat_value));
        }
        else if (WIFSTOPPED(stat_value)) {
            printf("\tПотомок остановился. Номер сигнала: %d\n\n", WSTOPSIG(stat_value));
        }
    }

    close(pipefd[1]);
    read(pipefd[0], str_buff, STR_BUFF_SIZE);
    printf("Сообщения, полученные предком: %s", str_buff);

    printf("Предок завершился\n");
    return no_error;
}