#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_AMOUNT 4

#define WRITERS_AMOUNT 3
#define READERS_AMOUNT 5

#define ACTIVE_READERS 0
#define ACTIVE_WRITERS 1
#define WAIT_READERS 2
#define WAIT_WRITERS 3

struct sembuf start_write[] = {
        {WAIT_WRITERS,   1,  0},
        {ACTIVE_READERS, 0,  0},
        {ACTIVE_WRITERS, 0,  0},
        {ACTIVE_WRITERS, 1,  0},
        {WAIT_WRITERS,   -1, 0}
};

struct sembuf stop_write[] = {
        {ACTIVE_WRITERS, -1, 0}
};

struct sembuf start_read[] = {
        {WAIT_READERS,   1,  0},
        {ACTIVE_WRITERS, 0,  0},
        {WAIT_WRITERS,   0,  0},
        {ACTIVE_READERS, 1,  0},
        {WAIT_READERS,   -1, 0}
};

struct sembuf stop_read[] = {
        {ACTIVE_READERS, -1, 0}
};

int *shm = NULL;

void writer(int semID, int writer_id) {
    if (semop(semID, start_write, 5) == -1) {
        perror("Semop start write error");
        exit(-1);
    }

    (*shm)++;
    printf("Writer[ID = %d]: writes value %d\n", writer_id, *shm);

    if (semop(semID, stop_write, 1) == -1) {
        perror("Semop stop write error");
        exit(-1);
    }

    srand(time(NULL));
    sleep(rand() % 3 + 1);
}

void reader(int semID, int readerID) {
    if (semop(semID, start_read, 5) == -1) {
        perror("Semop start read error");
        exit(-1);
    }

    printf("Reader[ID = %d]: reads value %d\n", readerID, *shm);

    if (semop(semID, stop_read, 1) == -1) {
        perror("Semop end read error");
        exit(-1);
    }

    srand(time(NULL));
    sleep(rand() % 3 + 1);
}

int sig_status = 0;

void set_status(int sigint) {
    sig_status = 1;
}

int main() {
    int perms = S_IRWXU | S_IRWXG | S_IRWXO;
    int semID = semget(IPC_PRIVATE, SEM_AMOUNT, IPC_CREAT | perms);
    if (semID == -1) {
        perror("Semaphore creation error");
        exit(-1);
    }

    if (semctl(semID, WAIT_READERS, SETVAL, 1) == -1) {
        perror("Semaphore control error");
        exit(-1);
    }

    int shmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | perms);
    if (shmID == -1) {
        perror("Shared memory creation error");
        exit(-1);
    }

    shm = shmat(shmID, 0, 0);
    if (*shm == -1) {
        perror("Memory attach error");
        exit(-1);
    }

    signal(SIGINT, set_status);

    pid_t childID;
    for (int i = 0; i < WRITERS_AMOUNT; i++) {
        if ((childID = fork()) == -1) {
            perror("Writer fork error");
            exit(-1);
        }
        else if (childID == 0) {
            while (!sig_status) {
                writer(semID, i);
            }
            exit(0);
        }
    }

    for (int i = 0; i < READERS_AMOUNT; i++) {
        if ((childID = fork()) == -1) {
            perror("Reader fork error");
            exit(-1);
        }
        else if (childID == 0) {
            while (!sig_status) {
                reader(semID, i);
            }
            exit(0);
        }
    }

    int status;
    for (int i = 0; i < WRITERS_AMOUNT + READERS_AMOUNT; i++)
        wait(&status);

    if (shmdt(shm) == -1) {
        perror("shmdt error");
        exit(-1);
    }

    if (shmctl(shmID, IPC_RMID, NULL) == -1) {
        perror("shmctl error");
        exit(-1);
    }

    return 0;
}
