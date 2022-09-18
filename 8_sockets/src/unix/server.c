#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

const char *serv_sock_name = "server.soc";
const int recv_buf_len = 64;

static int sock;

void clean(int sock_fd) {
    close(sock_fd);
    unlink(serv_sock_name);
}

void sighandler(int signum) {
    printf("\nЗавершение работы сервера...");
    clean(sock);
    exit(0);
}

int main() {
    struct sockaddr_un server_address = {};
    socklen_t client_length = sizeof(struct sockaddr_un);

    struct sockaddr_un client_address = {};
    socklen_t address_length = sizeof(struct sockaddr_un);

    ssize_t bytes_recv;

    char buffer[recv_buf_len];
    char resp[2 * recv_buf_len];

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Не удалось создать сокет");
        return 1;
    }

    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, serv_sock_name, strlen(serv_sock_name));

    if (bind(sock,(struct sockaddr *) &server_address,address_length) == -1) {
        perror("Ошибка при вызове bind()");
        return 1;
    }

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    printf("Сервер запущен.\n(Нажмите Ctrl + C, чтобы завершить работу сервера)\n");

    socklen_t cli_len = client_length;
    for (;;) {
        bytes_recv = recvfrom(
                sock,
                buffer,
                sizeof(buffer),
                0,
                (struct sockaddr *) &client_address,
                &cli_len
        );

        if (bytes_recv < 0) {
            perror("Вызов recvfrom() завершился с ошибкой");
            clean(sock);
            return 1;
        }

        buffer[bytes_recv] = '\0';
        printf("Сообщение от клиента: %s\n", buffer);
        snprintf(resp, 2 * recv_buf_len, "Сервер отправил обратно: \"%s\"", buffer);

        if (sendto(sock, resp, strlen(resp), 0, (struct sockaddr *) &client_address, client_length) < 0) {
            perror("Не удалось отправить сообщение клиенту");
        } else {
            printf("Сообщение клиенту: %s\n", resp);
        }

        cli_len = client_length;
    }
}
