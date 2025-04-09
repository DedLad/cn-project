#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include "../common.h"

unsigned char* capture_screen() {
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        fprintf(stderr, "GetDC failed: %lu\n", GetLastError());
        return NULL;
    }
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    if (WIDTH > screenWidth || HEIGHT > screenHeight) {
        fprintf(stderr, "Warning: Capture size (%dx%d) exceeds screen dimensions (%dx%d)\n",
                WIDTH, HEIGHT, screenWidth, screenHeight);
    }
    
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        fprintf(stderr, "CreateCompatibleDC failed: %lu\n", GetLastError());
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, WIDTH, HEIGHT);
    if (!hBitmap) {
        fprintf(stderr, "CreateCompatibleBitmap failed: %lu\n", GetLastError());
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    HGDIOBJ hOldBitmap = SelectObject(hdcMem, hBitmap);
    
    if (!BitBlt(hdcMem, 0, 0, WIDTH, HEIGHT, hdcScreen, 0, 0, SRCCOPY)) {
        fprintf(stderr, "BitBlt failed: %lu\n", GetLastError());
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = WIDTH;
    bi.biHeight = -HEIGHT;  
    bi.biPlanes = 1;
    bi.biBitCount = 24;     
    bi.biCompression = BI_RGB; 
    
    int stride = ((WIDTH * 3 + 3) & ~3);  
    int size = stride * HEIGHT;
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for screen buffer\n");
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    memset(buffer, 0, size);
    
    if (!GetDIBits(hdcMem, hBitmap, 0, HEIGHT, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        fprintf(stderr, "GetDIBits failed with error: %lu\n", GetLastError());
        free(buffer);
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    printf("Windows capture first 3 pixels (BGR): ");
    for (int i = 0; i < 3; i++) {
        printf("[%02X,%02X,%02X] ", buffer[i*3], buffer[i*3+1], buffer[i*3+2]);
    }
    printf("\n");
    
    if (stride != WIDTH * 3) {
        unsigned char* contiguous = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
        if (contiguous) {
            for (int y = 0; y < HEIGHT; y++) {
                memcpy(contiguous + y * WIDTH * 3, buffer + y * stride, WIDTH * 3);
            }
            free(buffer);
            buffer = contiguous;
        } else {
            fprintf(stderr, "Warning: Could not remove stride padding\n");
        }
    }
    
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return buffer;
}