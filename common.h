#ifndef COMMON_H
#define COMMON_H

#define WIDTH 800
#define HEIGHT 600

#define PROTOCOL_VERSION 1
#define FORMAT_BGR24 1

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define close_socket close
#endif

static int recv_fully(socket_t sock, char* buffer, int length) {
    int total = 0;
    while (total < length) {
        int received = recv(sock, buffer + total, length - total, 0);
        if (received <= 0)
            return received; 
        total += received;
    }
    return total;
}

#endif