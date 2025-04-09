#ifdef _WIN32
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
unsigned char* g_framebuffer = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    char server_ip[64] = "127.0.0.1";
    if(lpCmdLine && lpCmdLine[0] != '\0') {
        strncpy(server_ip, lpCmdLine, sizeof(server_ip)-1);
        server_ip[sizeof(server_ip)-1] = '\0';
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        MessageBox(NULL, "WSAStartup failed", "Error", MB_OK);
        return 1;
    }

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        MessageBox(NULL, "Socket creation failed", "Error", MB_OK);
        return 1;
    }
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        char errMsg[128];
        sprintf(errMsg, "Connect failed to %s", server_ip);
        MessageBox(NULL, errMsg, "Error", MB_OK);
        return 1;
    }

    g_framebuffer = malloc(WIDTH * HEIGHT * 3);
    if (!g_framebuffer) {
        MessageBox(NULL, "Failed to allocate framebuffer", "Error", MB_OK);
        return 1;
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RemoteDesktopClient";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, "RemoteDesktopClient", "Remote Desktop Viewer",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               WIDTH, HEIGHT, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION);
        return 1;
    }
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (1) {
        int received = recv(sock, (char*)g_framebuffer, WIDTH * HEIGHT * 3, 0);
        if (received <= 0)
            break;
        InvalidateRect(hwnd, NULL, FALSE);
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    free(g_framebuffer);
    closesocket(sock);
    WSACleanup();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT && g_framebuffer) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        BITMAPINFO bi = {0};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = WIDTH;
        bi.bmiHeader.biHeight = -HEIGHT; 
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 24;
        bi.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(hdc, 0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, 
                      g_framebuffer, &bi, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    } else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#else
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(int argc, char *argv[]) {
    char server_ip[64] = "127.0.0.1";
    if(argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip)-1);
        server_ip[sizeof(server_ip)-1] = '\0';
    }

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr(server_ip);
    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect");
        exit(1);
    }

    unsigned char* framebuffer = malloc(WIDTH * HEIGHT * 3);
    if(!framebuffer) {
        perror("malloc");
        exit(1);
    }

    Display *dpy = XOpenDisplay(NULL);
    if(!dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0,
                                     WIDTH, HEIGHT, 1, BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));
    XMapWindow(dpy, win);
    XSelectInput(dpy, win, ExposureMask);

    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, screen), 24, ZPixmap, 0,
                               malloc(WIDTH * HEIGHT * 4), WIDTH, HEIGHT, 32, 0);
    if (!img) {
        fprintf(stderr, "Failed to create XImage\n");
        exit(1);
    }

    while (1) {
        int received = recv(sock, (char*)framebuffer, WIDTH * HEIGHT * 3, MSG_WAITALL);
        if (received <= 0)
            break;
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int i = (y * WIDTH + x) * 3;
                unsigned char r = framebuffer[i];
                unsigned char g = framebuffer[i+1];
                unsigned char b = framebuffer[i+2];
                unsigned long pixel = (b << 16) | (g << 8) | r;
                XPutPixel(img, x, y, pixel);
            }
        }
        XPutImage(dpy, win, DefaultGC(dpy, screen), img, 0, 0, 0, 0, WIDTH, HEIGHT);
        XFlush(dpy);
    }

    free(framebuffer);
    XDestroyImage(img);
    XCloseDisplay(dpy);
    close(sock);
    return 0;
}
#endif
