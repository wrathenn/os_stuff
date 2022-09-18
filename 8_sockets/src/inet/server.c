#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 10

const int sock_port = 8000;
const int buf_size = 256;

int main(void)
{
    int clients[MAX_CLIENTS] = {0};

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        perror("Не удалось создать сокет");
        return errno;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(sock_port)
    };
    memset(serv_addr.sin_zero, 0, sizeof(serv_addr.sin_zero));

    if (bind(sfd, (struct sockaddr *)&serv_addr, sizeof serv_addr) == -1)
    {
        close(sfd);
        perror("Ошибка при вызове bind()");
        return errno;
    }

    if (listen(sfd, MAX_CLIENTS) == -1)
    {
        close(sfd);
        perror("Ошибка при вызове listen()");
        return errno;
    }

    printf("Сервер запущен на %s:%d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));

    char buf_send[buf_size];
    sprintf(buf_send, "[PID] %d", getpid());

    char msg[buf_size];

    fd_set readfds;
    FD_ZERO(&readfds);

    for (;;)
    {
        FD_SET(sfd, &readfds);
        int max_sd = sfd;

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i] > 0)
                FD_SET(clients[i], &readfds);

            if (clients[i] > max_sd)
                max_sd = clients[i];
        }

        if (pselect(max_sd + 1, &readfds, NULL, NULL, NULL, NULL) == -1)
        {
            close(sfd);
            perror("Ошибка при вызове pselect()");
            return errno;
        }

        if (FD_ISSET(sfd, &readfds))
        {
            struct sockaddr_in cli_addr;
            socklen_t len;
            memset(cli_addr.sin_zero, 0, sizeof(cli_addr.sin_zero));

            const int client_fd = accept(sfd, (struct sockaddr *)&cli_addr, &len);

            printf("Новое подключение [%s:%d]\n", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);

            if (client_fd == -1)
            {
                close(sfd);
                perror("Ошибка при вызове accept()");
                exit(errno);
            }

            int not_set = 1;
            for (int i = 0; not_set && i < MAX_CLIENTS; ++i)
            {
                if (!clients[i])
                {
                    clients[i] = client_fd;
                    not_set = 0;
                }
            }

            if (not_set)
            {
                close(sfd);
                fprintf(stderr, "Достигнуто максимальное количество одновременных подключений");
                exit(1);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i] && FD_ISSET(clients[i], &readfds))
            {
                size_t bytes = recv(clients[i], &msg, buf_size, 0);
                if (!bytes)
                {
                    fprintf(stderr, "Клиент (fd = %d) разорвал подключение\n", clients[i]);
                    FD_CLR(clients[i], &readfds);
                    close(clients[i]);
                    clients[i] = 0;
                }
                else
                {
                    msg[bytes] = '\0';
                    printf("Получено сообщение от клиента (fd = %d): %s\n", clients[i], msg);

                    if (send(clients[i], buf_send, strlen(buf_send) + 1, 0) < 0)
                    {
                        perror("Не удалось отправить сообщение клиенту");
                        return -1;
                    }
                }
            }
        }
    }
}
