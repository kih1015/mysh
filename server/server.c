#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mysh.h"

#define PORT 8080
#define BUFFER_SIZE 1024

extern char *current_dir;
extern char *chroot_path;
int client_fd;

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // 주소 설정
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 소켓 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

listen:

    // 연결 대기
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d...\n", PORT);

    // 클라이언트 연결 허용
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    init();

    while (1) {
        // 클라이언트로부터 메시지 수신
        ssize_t bytesRead = read(client_fd, buffer, BUFFER_SIZE);
        if (bytesRead <= 0) {
            printf("Client disconnected\n");
            goto listen;
        }

        buffer[bytesRead] = '\0'; // Null-terminate the string
        printf("Received: %s\n", buffer);

        execute(buffer);
    
        // 종료 조건
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exiting...\n");
            break;
        } 
    }

    close(client_fd);
    close(server_fd);

    return 0;
}
