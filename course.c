#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SERVER_PORT 9000
#define MAX_BUFFER 1024

void respond(int socket, const char *status_code, const char *mime_type, const char *content, size_t content_length) {
    char header[MAX_BUFFER];
    int header_len = snprintf(header, MAX_BUFFER,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "\r\n",
             status_code, mime_type, content_length);
    
    write(socket, header, header_len);
    write(socket, content, content_length);
}

void process_get_request(int socket, const char *requested_path) {
    char full_path[MAX_BUFFER];
    snprintf(full_path, MAX_BUFFER, ".%s", requested_path);

    if (access(full_path, F_OK) == -1) {
        respond(socket, "404 Not Found", "text/plain", "404 Not Found", strlen("404 Not Found"));
        return;
    }

    struct stat st;
    stat(full_path, &st);
    size_t file_size = st.st_size;

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        respond(socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error", strlen("500 Internal Server Error"));
        return;
    }

    int file_descriptor = open(full_path, O_RDONLY);
    if (file_descriptor == -1) {
        free(file_content);
        respond(socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error", strlen("500 Internal Server Error"));
        return;
    }

    ssize_t bytes_read = read(file_descriptor, file_content, file_size);
    close(file_descriptor);
    
    if (bytes_read != file_size) {
        free(file_content);
        respond(socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error", strlen("500 Internal Server Error"));
        return;
    }

    const char *mime_type = "text/plain";
    const char *ext = strrchr(requested_path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) mime_type = "text/html";
        else if (strcmp(ext, ".css") == 0) mime_type = "text/css";
    }

    respond(socket, "200 OK", mime_type, file_content, file_size);
    free(file_content);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_info, client_info;
    socklen_t client_info_len = sizeof(client_info);
    char request_buffer[MAX_BUFFER];
    int opt = 1;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Ошибка установки SO_REUSEADDR");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(SERVER_PORT);

    if (bind(server_sock, (struct sockaddr *)&server_info, sizeof(server_info)) < 0) {
        perror("Ошибка привязки сокета");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Ошибка прослушивания");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("HTTP-сервер запущен на порту %d\n", SERVER_PORT);

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
                if (strcmp(path, "/") == 0) {
                    path = "/index.html";
                }
                process_get_request(client_sock, path);
            }
        }

        close(client_sock);
    }

    close(server_sock);
    return 0;
}