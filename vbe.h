#ifndef VBE_H
#define VBE_H

#include <stdint.h>

typedef struct {
    uint32_t* framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} vbe_info_t;

void init_graphics(uint32_t* fb_addr, uint32_t w, uint32_t h, uint32_t p, uint8_t b);
void draw_pixel(int x, int y, uint32_t color);

// НОВА ФУНКЦІЯ: Читання кольору пікселя з екрана
uint32_t get_pixel(int x, int y);

void clear_screen(uint32_t color);
void draw_char(int x, int y, char c, uint32_t color, int scale);
void draw_string(int x, int y, const char* str, uint32_t color, int scale);

#endif