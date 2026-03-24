#include "vbe.h"
#include "font.h"

static vbe_info_t vbe;

void init_graphics(uint32_t* fb_addr, uint32_t w, uint32_t h, uint32_t p, uint8_t b) {
    vbe.framebuffer = fb_addr;
    vbe.width = w;
    vbe.height = h;
    vbe.pitch = p;
    vbe.bpp = b;
}

void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= vbe.width || y < 0 || y >= vbe.height) return;
    uint32_t* pixel_addr = (uint32_t*)((uint8_t*)vbe.framebuffer + y * vbe.pitch + x * (vbe.bpp / 8));
    *pixel_addr = color;
}

// НОВА ФУНКЦІЯ: Повертає колір пікселя
uint32_t get_pixel(int x, int y) {
    if (x < 0 || x >= vbe.width || y < 0 || y >= vbe.height) return 0;
    uint32_t* pixel_addr = (uint32_t*)((uint8_t*)vbe.framebuffer + y * vbe.pitch + x * (vbe.bpp / 8));
    return *pixel_addr;
}

void clear_screen(uint32_t color) {
    for (uint32_t y = 0; y < vbe.height; y++) {
        for (uint32_t x = 0; x < vbe.width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

void draw_char(int x, int y, char c, uint32_t color, int scale) {
    if (c < 0 || c > 127) return; 
    const uint8_t* glyph = font8x16[(uint8_t)c]; 
    
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if ((glyph[cy] >> (7 - cx)) & 1) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        draw_pixel(x + (cx * scale) + sx, y + (cy * scale) + sy, color);
                    }
                }
            }
        }
    }
}

void draw_string(int x, int y, const char* str, uint32_t color, int scale) {
    int current_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(current_x, y, str[i], color, scale);
        current_x += 8 * scale;
    }
}