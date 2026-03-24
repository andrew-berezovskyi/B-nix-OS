#include "vbe.h"
#include "font.h"

static vbe_info_t vbe;

// НАШ НЕВИДИМИЙ ЕКРАН
static uint32_t backbuffer[1280 * 720];

void init_graphics(uint32_t* fb_addr, uint32_t w, uint32_t h, uint32_t p, uint8_t b) {
    vbe.framebuffer = fb_addr;
    vbe.width = w;
    vbe.height = h;
    vbe.pitch = p;
    vbe.bpp = b;
}

void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= vbe.width || y < 0 || y >= vbe.height) return;
    backbuffer[y * vbe.width + x] = color; 
}

uint32_t get_pixel(int x, int y) {
    if (x < 0 || x >= vbe.width || y < 0 || y >= vbe.height) return 0;
    return backbuffer[y * vbe.width + x];
}

// --- СУПЕРШВИДКЕ КОПІЮВАННЯ НА ВІДЕОКАРТУ ---
void swap_buffers(void) {
    uint32_t* src = backbuffer;
    uint32_t* dst = (uint32_t*)vbe.framebuffer;
    uint32_t count = vbe.width * vbe.height;

    // rep movsl копіює дані блоками по 4 байти на швидкості шини процесора
    asm volatile (
        "cld\n\t"
        "rep movsl"
        : "+S" (src), "+D" (dst), "+c" (count)
        :
        : "memory"
    );
}

// --- СУПЕРШВИДКЕ ОЧИЩЕННЯ ЕКРАНА ---
void clear_screen(uint32_t color) {
    uint32_t* dest = backbuffer;
    uint32_t count = vbe.width * vbe.height;

    // rep stosl миттєво заливає пам'ять одним кольором
    asm volatile (
        "cld\n\t"
        "rep stosl"
        : "+D" (dest), "+c" (count)
        : "a" (color)
        : "memory"
    );
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