#ifdef _WIN32
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
unsigned char* g_framebuffer = NULL;

//disable debug if u don wan to see debug messages
#define DEBUG_MODE 1

void debug_log(const char* format, ...) {
#if DEBUG_MODE
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsprintf(buffer, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
#endif
}

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
    
    int recvbuf = 1024 * 1024; 
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&recvbuf, sizeof(recvbuf));
    
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

    unsigned char header[8];
    if (recv_fully(sock, (char*)header, sizeof(header)) != sizeof(header)) {
        MessageBox(NULL, "Failed to receive protocol header", "Error", MB_OK);
        return 1;
    }
    
    if (header[0] != 'S' || header[1] != 'C' || header[2] != 'R' || header[3] != 'N') {
        MessageBox(NULL, "Invalid protocol header magic bytes", "Error", MB_OK);
        return 1;
    }
    
    debug_log("Connected to server using protocol v%d, format %d", 
              header[4], header[5]);
              
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
    int frame_size = WIDTH * HEIGHT * 3;
    int frames_received = 0;
    int last_seq = -1;
    
    while (1) {
        unsigned char frame_header[5];
        if (recv_fully(sock, (char*)frame_header, sizeof(frame_header)) != sizeof(frame_header)) {
            debug_log("Failed to receive frame header or connection closed");
            break;
        }
        
        if (frame_header[0] != 'F' || frame_header[1] != 'R' || frame_header[2] != 'M') {
            debug_log("Invalid frame header magic bytes: %c%c%c", 
                     frame_header[0], frame_header[1], frame_header[2]);
            continue; 
        }
        
        int seq = frame_header[3] | (frame_header[4] << 8);
        if (last_seq != -1 && seq != (last_seq + 1) % 65536) {
            debug_log("Frame sequence mismatch: expected %d, got %d", 
                     (last_seq + 1) % 65536, seq);
        }
        last_seq = seq;
        
        if (recv_fully(sock, (char*)g_framebuffer, frame_size) != frame_size) {
            debug_log("Failed to receive complete frame data");
            break;
        }
        
        frames_received++;
        if (frames_received % 30 == 0) {
            debug_log("Received %d frames", frames_received);
        }
        
        InvalidateRect(hwnd, NULL, FALSE);
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

cleanup:
    free(g_framebuffer);
    closesocket(sock);
    WSACleanup();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT && g_framebuffer) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        static int debug_printed = 0;
        if (!debug_printed) {
            debug_log("Windows first 3 pixels (BGR): [%02X,%02X,%02X] [%02X,%02X,%02X] [%02X,%02X,%02X]",
                g_framebuffer[0], g_framebuffer[1], g_framebuffer[2],
                g_framebuffer[3], g_framebuffer[4], g_framebuffer[5],
                g_framebuffer[6], g_framebuffer[7], g_framebuffer[8]);
            debug_printed = 1;
        }
        
        BITMAPINFO bi = {0};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = WIDTH;
        bi.bmiHeader.biHeight = -HEIGHT; 
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 24;
        bi.bmiHeader.biCompression = BI_RGB;
        
        int result = StretchDIBits(hdc, 0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, 
                              g_framebuffer, &bi, DIB_RGB_COLORS, SRCCOPY);
        
        if (result == 0 || result == GDI_ERROR) {
            debug_log("StretchDIBits failed: %lu", GetLastError());
        }
        
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

#include <stdarg.h>  
#include <X11/Xutil.h>

#define DEBUG_MODE 1

void debug_log(const char* format, ...) {
#if DEBUG_MODE
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
#endif
}

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
    
    int recvbuf = 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&recvbuf, sizeof(recvbuf));
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr(server_ip);
    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect");
        exit(1);
    }
    
    unsigned char header[8];
    if (recv_fully(sock, (char*)header, sizeof(header)) != sizeof(header)) {
        fprintf(stderr, "Failed to receive protocol header\n");
        exit(1);
    }
    
    if (header[0] != 'S' || header[1] != 'C' || header[2] != 'R' || header[3] != 'N') {
        fprintf(stderr, "Invalid protocol header magic bytes\n");
        exit(1);
    }
    
    debug_log("Connected to server using protocol v%d, format %d", 
             header[4], header[5]);
    
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
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);

    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, screen), 24, ZPixmap, 0,
                               malloc(WIDTH * HEIGHT * 4), WIDTH, HEIGHT, 32, 0);
    if (!img) {
        fprintf(stderr, "Failed to create XImage\n");
        exit(1);
    }
    
    int frames_received = 0;
    int last_seq = -1;
    int frame_size = WIDTH * HEIGHT * 3;

    while (1) {
        XEvent event;
        while (XPending(dpy) > 0) {
            XNextEvent(dpy, &event);
            if (event.type == KeyPress) {
                goto cleanup;
            }
        }
        
        unsigned char frame_header[5];
        if (recv_fully(sock, (char*)frame_header, sizeof(frame_header)) != sizeof(frame_header)) {
            debug_log("Failed to receive frame header or connection closed");
            break;
        }
        
        if (frame_header[0] != 'F' || frame_header[1] != 'R' || frame_header[2] != 'M') {
            debug_log("Invalid frame header magic bytes: %c%c%c",
                     frame_header[0], frame_header[1], frame_header[2]);
            continue;
        }
        
        int seq = frame_header[3] | (frame_header[4] << 8);
        if (last_seq != -1 && seq != (last_seq + 1) % 65536) {
            debug_log("Frame sequence mismatch: expected %d, got %d", 
                     (last_seq + 1) % 65536, seq);
        }
        last_seq = seq;
        
        if (recv_fully(sock, (char*)framebuffer, frame_size) != frame_size) {
            debug_log("Failed to receive complete frame data");
            break;
        }
        
        frames_received++;
        if (frames_received % 30 == 0) {
            debug_log("Received %d frames", frames_received);
        }
        
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int i = (y * WIDTH + x) * 3;
                
                if (y == 0 && x < 3 && frames_received == 1) {
                    debug_log("Linux pixel %d: BGR=[%02X,%02X,%02X]", 
                             x, framebuffer[i], framebuffer[i+1], framebuffer[i+2]);
                }
                
                unsigned char b = framebuffer[i];
                unsigned char g = framebuffer[i+1];
                unsigned char r = framebuffer[i+2];
                

                XPutPixel(img, x, y, (r << 16) | (g << 8) | b);
            }
        }
        XPutImage(dpy, win, DefaultGC(dpy, screen), img, 0, 0, 0, 0, WIDTH, HEIGHT);
        XFlush(dpy);
    }

cleanup:
    free(framebuffer);
    XDestroyImage(img);
    XCloseDisplay(dpy);
    close(sock);
    return 0;
}
#endif