#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define SERVER_PORT 8083
#define MAX_BUFFER 1024

// Функция для формирования и отправки HTTP-ответа
void respond(int socket, const char *status_code, const char *mime_type, const char *content) {
    char http_response[MAX_BUFFER];
    snprintf(http_response, MAX_BUFFER,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n"
             "%s",
             status_code, mime_type, strlen(content), content);
    write(socket, http_response, strlen(http_response));
}

// Функция для обработки GET-запросов
void process_get_request(int socket, const char *requested_path) {
    char full_path[MAX_BUFFER];
    snprintf(full_path, MAX_BUFFER, ".%s", requested_path);

    // Проверка существования файла
    if (access(full_path, F_OK) == -1) {
        respond(socket, "404 Not Found", "text/plain", "404 Not Found");
        return;
    }

    // Открытие файла
    int file_descriptor = open(full_path, O_RDONLY);
    if (file_descriptor == -1) {
        respond(socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error");
        return;
    }

    // Чтение содержимого файла
    char file_content[MAX_BUFFER];
    ssize_t bytes_read = read(file_descriptor, file_content, MAX_BUFFER);
    if (bytes_read == -1) {
        close(file_descriptor);
        respond(socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error");
        return;
    }

    // Отправка содержимого файла в ответ
    respond(socket, "200 OK", "text/html", file_content);
    close(file_descriptor);
}

// Основная функция сервера
int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_info, client_info;
    socklen_t client_info_len = sizeof(client_info);
    char request_buffer[MAX_BUFFER];

    // Создание сокета
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(SERVER_PORT);

    // Привязка сокета к адресу
    if (bind(server_sock, (struct sockaddr *)&server_info, sizeof(server_info)) < 0) {
        perror("Ошибка привязки сокета");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Начало прослушивания
    if (listen(server_sock, 10) < 0) {
        perror("Ошибка прослушивания");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("HTTP-сервер запущен на порту %d\n", SERVER_PORT);

    // Основной цикл обработки запросов
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_info, &client_info_len);
        if (client_sock < 0) {
            perror("Ошибка принятия соединения");
            continue;
        }

        ssize_t received_bytes = read(client_sock, request_buffer, MAX_BUFFER - 1);
        if (received_bytes > 0) {
            request_buffer[received_bytes] = '\0';
            if (strncmp(request_buffer, "GET ", 4) == 0) {
                char *path = strtok(request_buffer + 4, " ");
                process_get_request(client_sock, path);
            }
        }

        close(client_sock);
    }

    close(server_sock);
    return 0;
}