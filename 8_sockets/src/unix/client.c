#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

//const char *sock_name = "server.soc";
const char *serv_sock_name = "server.soc";
const int buf_size = 64;

int socket_fd;

void clean(int sock_fd, char *sock_name) {
    close(sock_fd);
    unlink(sock_name);
}

int main(int argc, char *argv[]) {
    char *sock_name = argv[1];

    struct sockaddr_un client_address, server_address;
    socklen_t address_length = sizeof(struct sockaddr_un);
    char buffer[buf_size];
    ssize_t bytes_recv;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("Не удалось создать сокет");
        return 1;
    }

    client_address.sun_family = AF_UNIX;
    strncpy(client_address.sun_path, sock_name, strlen(sock_name));

    if (bind(socket_fd, (const struct sockaddr *) &client_address, address_length) == -1) {
        perror("Ошибка при вызове bind()");
        return 1;
    }

    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, serv_sock_name, strlen(serv_sock_name));

    snprintf(buffer, buf_size, "[PID] %d", getpid());
    if (sendto(socket_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &server_address, address_length) == -1) {
        perror("Не удалось отправить сообщение серверу");
        return 1;
    }

    printf("Сообщение серверу: %s\n", buffer);
    bytes_recv = recvfrom(
            socket_fd,
            buffer,
            sizeof(buffer),
            0,
            (struct sockaddr *) &server_address,
            &address_length
    );

    if (bytes_recv < 0) {
        perror("Вызов recvfrom() завершился с ошибкой");
        clean(socket_fd, sock_name);
        return 1;
    }

    buffer[bytes_recv] = '\0';
    printf("Ответ от сервера: %s\n", buffer);
    clean(socket_fd, sock_name);

    return 0;
}
