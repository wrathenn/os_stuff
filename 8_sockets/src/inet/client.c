#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define sock_port 8000
#define buf_size 256

int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("Не удалось создать сокет");
        return errno;
    }

    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(sock_port)
    };
    memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

    if (connect(sfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("Ошибка при вызове connect()");
        return errno;
    }

    char msg[buf_size];

    ssize_t bytes_recv;
    char resp[buf_size];

    snprintf(msg, buf_size, "[PID] %d", getpid());
    if (send(sfd, msg, strlen(msg), 0) == -1) {
        perror("Не удалось отправить сообщение серверу");
        return errno;
    }
    printf("Сообщение серверу: %s\n", msg);
    bytes_recv = recv(sfd, resp, sizeof(resp), 0);

    if (bytes_recv < 0) {
        perror("Вызов recv() завершился с ошибкой");
    } else {
        printf("Ответ от сервера: %s\n", resp);
    }

    printf("Вызов sleep\n\n");
    sleep(2);
}
