#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include "../common.h"

#define USE_BGR_ORDER 1

unsigned char* capture_screen() {
    Display* disp = XOpenDisplay(NULL);
    if (!disp) {
        fprintf(stderr, "Cannot open display\n");
        return NULL;
    }
    
    Window root = DefaultRootWindow(disp);
    
    Screen* screen = DefaultScreenOfDisplay(disp);
    if (screen->width < WIDTH || screen->height < HEIGHT) {
        fprintf(stderr, "Warning: Screen resolution (%dx%d) is smaller than capture size (%dx%d)\n", 
                screen->width, screen->height, WIDTH, HEIGHT);
    }
    
    XImage* img = XGetImage(disp, root, 0, 0, WIDTH, HEIGHT, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "Failed to capture screen image\n");
        XCloseDisplay(disp);
        return NULL;
    }
    
    printf("X11 Capture: bits_per_pixel=%d, bytes_per_line=%d, width=%d, height=%d\n", 
           img->bits_per_pixel, img->bytes_per_line, img->width, img->height);
    fflush(stdout);
    
    int size = WIDTH * HEIGHT * 3;
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for screen buffer\n");
        XDestroyImage(img);
        XCloseDisplay(disp);
        return NULL;
    }
    
    memset(buffer, 0, size);
    
    int bytes_per_pixel = img->bits_per_pixel / 8;
    
    for (int y = 0; y < HEIGHT; y++) {
        unsigned char* src = (unsigned char*)img->data + y * img->bytes_per_line;
        unsigned char* dst = buffer + y * WIDTH * 3;
        
        for (int x = 0; x < WIDTH; x++) {
            if (img->bits_per_pixel == 32) {
                if (USE_BGR_ORDER) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                } else {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                }
            } else if (img->bits_per_pixel == 24) {
                if (USE_BGR_ORDER) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                } else {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                }
            } else if (img->bits_per_pixel == 16) {
                unsigned short pixel = *(unsigned short*)src;
                if (USE_BGR_ORDER) {
                    dst[0] = (pixel & 0x1F) << 3;       
                    dst[1] = ((pixel >> 5) & 0x3F) << 2;
                    dst[2] = ((pixel >> 11) & 0x1F) << 3;
                } else {
                    dst[0] = ((pixel >> 11) & 0x1F) << 3; 
                    dst[1] = ((pixel >> 5) & 0x3F) << 2;  
                    dst[2] = (pixel & 0x1F) << 3;     
                }
            } else {

                unsigned long pixel = XGetPixel(img, x, y);
                if (USE_BGR_ORDER) {
                    dst[0] = pixel & 0xFF;         
                    dst[1] = (pixel >> 8) & 0xFF;  
                    dst[2] = (pixel >> 16) & 0xFF; 
                } else {
                    dst[0] = (pixel >> 16) & 0xFF; 
                    dst[1] = (pixel >> 8) & 0xFF;  
                    dst[2] = pixel & 0xFF;         
                }
            }
            
            src += bytes_per_pixel;
            dst += 3;
        }
    }
    
    printf("Linux capture first 3 pixels (BGR): ");
    for (int i = 0; i < 3; i++) {
        printf("[%02X,%02X,%02X] ", buffer[i*3], buffer[i*3+1], buffer[i*3+2]);
    }
    printf("\n");
    fflush(stdout);
    
    XDestroyImage(img);
    XCloseDisplay(disp);
    return buffer;
}