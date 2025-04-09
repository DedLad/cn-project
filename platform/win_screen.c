#include <windows.h>
#include <stdlib.h>
#include "../common.h"

unsigned char* capture_screen() {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, WIDTH, HEIGHT);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, WIDTH, HEIGHT, hdcScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = WIDTH;
    bi.biHeight = -HEIGHT;  
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    int size = WIDTH * HEIGHT * 3;
    unsigned char* buffer = (unsigned char*)malloc(size);
    GetDIBits(hdcMem, hBitmap, 0, HEIGHT, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return buffer;
}
