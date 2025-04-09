#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include "platform/win_screen.h"
#else
    #include "platform/linux_screen.h"
#endif

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    if (listen(server, 1) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Waiting for client...\n");
    socket_t client = accept(server, NULL, NULL);
    if (client < 0) {
        perror("accept");
        exit(1);
    }
    printf("Client connected!\n");

    while (1) {
        unsigned char* data = capture_screen();
        if (!data) {
            fprintf(stderr, "Failed to capture screen\n");
            break;
        }
        if (send(client, (const char*)data, WIDTH * HEIGHT * 3, 0) < 0) {
            perror("send");
            free(data);
            break;
        }
        free(data);
    }

    close_socket(client);
    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
