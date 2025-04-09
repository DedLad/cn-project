#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include "platform/win_screen.h"
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include "platform/linux_screen.h"
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
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
    
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    int sendbuf = 1024 * 1024; 
    setsockopt(server, SOL_SOCKET, SO_SNDBUF, (const char*)&sendbuf, sizeof(sendbuf));
    
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

    unsigned char header[8];
    header[0] = 'S'; //sync magic byte
    header[1] = 'C'; 
    header[2] = 'R';
    header[3] = 'N';
    header[4] = PROTOCOL_VERSION;
    header[5] = FORMAT_BGR24; 
    header[6] = WIDTH >> 8; 
    header[7] = WIDTH & 0xFF;
    
    if (send(client, (const char*)header, sizeof(header), 0) != sizeof(header)) {
        perror("send header");
        goto cleanup;
    }

    int frame_count = 0;
    int total_size = WIDTH * HEIGHT * 3;
    time_t last_stats = time(NULL);
    
    while (1) {
        unsigned char* data = capture_screen();
        if (!data) {
            fprintf(stderr, "Failed to capture screen\n");
            SLEEP_MS(100); 
            continue;
        }
        
        unsigned char frame_header[5];
        frame_header[0] = 'F';
        frame_header[1] = 'R';
        frame_header[2] = 'M';
        frame_header[3] = frame_count & 0xFF;
        frame_header[4] = (frame_count >> 8) & 0xFF;
        
        if (send(client, (const char*)frame_header, sizeof(frame_header), 0) != sizeof(frame_header)) {
            perror("send frame header");
            free(data);
            goto cleanup;
        }
        
        int sent = 0;
        while (sent < total_size) {
            int bytes = send(client, (const char*)data + sent, total_size - sent, 0);
            if (bytes <= 0) {
                perror("send");
                free(data);
                goto cleanup;
            }
            sent += bytes;
        }
        
        free(data);
        frame_count++;
        
        time_t now = time(NULL);
        if (now - last_stats >= 5) {
            //dont forget to remove prints here
            printf("Sent %d frames in %ld seconds (%.2f FPS)\n", 
                frame_count, (long)(now - last_stats), 
                (float)frame_count / (now - last_stats));
            frame_count = 0;
            last_stats = now;
        }
        
        SLEEP_MS(16); 
    }
    
cleanup:
    close_socket(client);
    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}