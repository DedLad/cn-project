#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>
#include "../common.h"

#define SWAP_CHANNELS 1
#define ROW_INVERSION 0

unsigned char* capture_screen() {
    Display* disp = XOpenDisplay(NULL);
    if (!disp) {
        fprintf(stderr, "Cannot open display\n");
        return NULL;
    }
    
    Window root = DefaultRootWindow(disp);
    XImage* img = XGetImage(disp, root, 0, 0, WIDTH, HEIGHT, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "Failed to capture screen image\n");
        XCloseDisplay(disp);
        return NULL;
    }
    
    printf("bits_per_pixel = %d\n", img->bits_per_pixel);
    printf("bytes_per_line = %d\n", img->bytes_per_line);
    printf("img->width = %d, img->height = %d\n", img->width, img->height);
    fflush(stdout);
    
    int size = WIDTH * HEIGHT * 3;
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for screen buffer\n");
        XDestroyImage(img);
        XCloseDisplay(disp);
        return NULL;
    }
    
    for (int y = 0; y < HEIGHT; y++) {
        int src_y = (ROW_INVERSION ? (HEIGHT - 1 - y) : y);
        unsigned char* src = (unsigned char*)img->data + src_y * img->bytes_per_line;
        unsigned char* dst = buffer + y * WIDTH * 3;
        for (int x = 0; x < WIDTH; x++) {
            if (SWAP_CHANNELS) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
            } else {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            }
            src += 4;
            dst += 3;
        }
    }
    
    XDestroyImage(img);
    XCloseDisplay(disp);
    return buffer;
}
