#include "vbe.h"
#include "font.h"
#include "kheap.h"
#include "rtc.h" 
#include <stdint.h>
#include <stddef.h>

extern uint8_t* main_font_data;

// ==============================================================================
// 1. ХИТРИЙ ТРЮК ДЛЯ ЯДРА (Блокує всі конфлікти зі <string.h>)
// ==============================================================================
#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define strlen __builtin_strlen

// ==============================================================================
// 2. МАТЕМАТИКА ДЛЯ ВЕКТОРНИХ ШРИФТІВ ТА КАРТИНОК
// ==============================================================================
double fabs(double x) { return x < 0 ? -x : x; }
double floor(double x) { return (double)((int)x); }
double ceil(double x) { 
    int i = (int)x;
    return (x > (double)i) ? (double)(i + 1) : (double)i;
}

double fmod(double x, double y) {
    return x - (int)(x / y) * y;
}

double cos(double x) {
    double x2 = x * x;
    return 1.0 - (x2 / 2.0) + (x2 * x2 / 24.0) - (x2 * x2 * x2 / 720.0);
}

double acos(double x) {
    return 1.57079 - x; 
}

double sqrt(double x) {
    if (x < 0) return 0;
    double z = 1.0;
    for (int i = 0; i < 10; i++) z -= (z*z - x) / (2*z);
    return z;
}

double pow(double x, double y) {
    double res = 1;
    for(int i = 0; i < (int)y; i++) res *= x;
    return res;
}

// ==============================================================================
// 3. НАЛАШТУВАННЯ БІБЛІОТЕКИ STB IMAGE (ДЛЯ PNG КАРТИНОК)
// ==============================================================================
void* stbi_realloc_sized(void* ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) { kfree(ptr); return NULL; }
    if (!ptr) return kmalloc(new_size);
    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
        kfree(ptr);
    }
    return new_ptr;
}

#define STBI_NO_STDLIB
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_HDR
#define STBI_ONLY_PNG

#define STBI_MALLOC(sz)    kmalloc(sz)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) stbi_realloc_sized(p,oldsz,newsz)
#define STBI_FREE(p)       kfree(p)
#define STBI_ASSERT(x)    
#define STBI_MEMCPY memcpy
#define STBI_MEMSET memset
#define STBI_ABS(x) ((x) < 0 ? -(x) : (x))

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

// ==============================================================================
// 4. НАЛАШТУВАННЯ БІБЛІОТЕКИ STB TRUETYPE (ДЛЯ ШРИФТІВ)
// ==============================================================================
#define STBTT_malloc(x,u)  ((void)(u), kmalloc(x))
#define STBTT_free(x,u)    ((void)(u), kfree(x))
#define STBTT_assert(x)    ((void)(0))

#define STBTT_memset memset
#define STBTT_memcpy memcpy
#define STBTT_strlen strlen
#define STBTT_fmod   fmod
#define STBTT_cos    cos
#define STBTT_acos   acos
#define STBTT_fabs   fabs
#define STBTT_floor  floor
#define STBTT_ceil   ceil
#define STBTT_sqrt   sqrt
#define STBTT_pow    pow

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_NO_STDIO
#define STBTT_NO_STDLIB
#include "stb_truetype.h"

// ==============================================================================
// 5. ГРАФІЧНИЙ ДРАЙВЕР ТА GUI ФУНКЦІЇ
// ==============================================================================
static vbe_info_t vbe;
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

void swap_buffers(void) {
    uint32_t* src = backbuffer;
    uint32_t* dst = (uint32_t*)vbe.framebuffer;
    uint32_t count = vbe.width * vbe.height;
    asm volatile ("cld; rep movsl" : "+S"(src), "+D"(dst), "+c"(count) :: "memory");
}

void clear_screen(uint32_t color) {
    uint32_t* dest = backbuffer;
    uint32_t count = vbe.width * vbe.height;
    asm volatile ("cld; rep stosl" : "+D"(dest), "+c"(count) : "a"(color) : "memory" );
}

void draw_char(int x, int y, char c, uint32_t color, int scale) {
    if (c < 0 || c > 127) return; 
    const uint8_t* glyph = font8x16[(uint8_t)c]; 
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if ((glyph[cy] >> (7 - cx)) & 1) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) draw_pixel(x + (cx * scale) + sx, y + (cy * scale) + sy, color);
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

void draw_ttf_char_internal(stbtt_fontinfo* font, int x, int y, char c, float size, uint32_t color) {
    int width, height, xoff, yoff;
    float scale = stbtt_ScaleForPixelHeight(font, size);
    unsigned char* bitmap = stbtt_GetCodepointBitmap(font, 0, scale, c, &width, &height, &xoff, &yoff);

    if (bitmap) {
        uint8_t r_src = (color >> 16) & 0xFF;
        uint8_t g_src = (color >> 8) & 0xFF;
        uint8_t b_src = color & 0xFF;

        for (int cy = 0; cy < height; cy++) {
            for (int cx = 0; cx < width; cx++) {
                uint8_t alpha = bitmap[cy * width + cx];
                if (alpha > 0) {
                    int sx = x + cx + xoff;
                    int sy = y + cy + yoff;
                    uint32_t bg = get_pixel(sx, sy);
                    
                    uint8_t r = (r_src * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
                    uint8_t g = (g_src * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
                    uint8_t b = (b_src * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
                    
                    draw_pixel(sx, sy, (r << 16) | (g << 8) | b);
                }
            }
        }
        kfree(bitmap);
    }
}

void draw_ttf_string(int x, int y, uint8_t* font_buffer, const char* str, float size, uint32_t color) {
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_buffer, 0)) return;
    
    float scale = stbtt_ScaleForPixelHeight(&font, size);
    int current_x = x;

    for (int i = 0; str[i] != '\0'; i++) {
        draw_ttf_char_internal(&font, current_x, y, str[i], size, color);
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, str[i], &advance, &lsb);
        current_x += (int)(advance * scale);
    }
}

void draw_filled_circle(int x, int y, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) draw_pixel(x + dx, y + dy, color);
        }
    }
}

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    draw_filled_rect(x + r, y, w - 2*r, h, color);
    draw_filled_rect(x, y + r, w, h - 2*r, color);
    draw_filled_circle(x + r, y + r, r, color);
    draw_filled_circle(x + w - r, y + r, r, color);
    draw_filled_circle(x + r, y + h - r, r, color);
    draw_filled_circle(x + w - r, y + h - r, r, color);
}

// Старі функції залишені як було
void draw_background_gradient() {
    for (int y = 0; y < vbe.height; y++) {
        uint8_t b = (y * 50) / vbe.height; 
        uint32_t color = b; 
        for (int x = 0; x < vbe.width; x++) { draw_pixel(x, y, color); }
    }
}

void draw_b_nix_logo(int x, int y, int size) {
    draw_filled_circle(x, y, size, 0xFFFFFF);
    draw_filled_circle(x, y, size - 2, 0x111111);
    if (main_font_data) draw_ttf_string(x - 20, y - 30, main_font_data, "B", 60.0, 0xFFFFFF);
}

void draw_cat_logo(int x, int y, int size, uint32_t color) {
    for(int i=0; i<size/3; i++) draw_pixel(x - size/2 + i, y - size/2 - i, color);
    for(int i=0; i<size/3; i++) draw_pixel(x - size/6 + i, y - size/2 - (size/3 - i), color);
    for(int i=0; i<size/3; i++) draw_pixel(x + size/2 - i, y - size/2 - i, color);
    for(int i=0; i<size/3; i++) draw_pixel(x + size/6 - i, y - size/2 - (size/3 - i), color);
    draw_rect_outline(x - size/2, y - size/2, size, size, color);
    draw_filled_circle(x, y + size/4, size/6, color);
}

void draw_kali_style_background() {
    clear_screen(0x1A1A1A); 
    draw_cat_logo(vbe.width / 2, vbe.height / 2, vbe.height / 2, 0x222222); 
}

void draw_top_panel(uint32_t w, uint32_t h) {
    draw_filled_rect(0, 0, w, 28, 0x111111);
    draw_rect_outline(0, 0, w, 28, 0x222222);
    if (main_font_data) draw_ttf_string(10, 6, main_font_data, "B-nix OS 2026", 16.0, 0xCCCCCC);
    uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y);
    char ts[6] = "00:00"; ts[0] = (hour/10)+'0'; ts[1] = (hour%10)+'0'; ts[3] = (min/10)+'0'; ts[4] = (min%10)+'0';
    if (main_font_data) draw_ttf_string(w - 70, 6, main_font_data, ts, 16.0, 0xCCCCCC);
}

// ==============================================================================
// 6. ФУНКЦІЇ МАЛЮВАННЯ КАРТИНОК З ПАМ'ЯТІ (PNG) (З ПЕРЕВОРОТОМ)
// ==============================================================================
void draw_background_image(uint8_t* img_data, uint32_t img_size) {
    int width, height, channels;
    unsigned char *pixels = stbi_load_from_memory(img_data, img_size, &width, &height, &channels, 3);

    if (!pixels) return; 

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if(y >= vbe.height || x >= vbe.width) continue; 
            
            // ПЕРЕВЕРТАЄМО КАРТИНКУ ПО ВЕРТИКАЛІ
            int src_y = height - 1 - y;
            int pixel_index = (src_y * width + x) * 3;
            
            uint8_t r = pixels[pixel_index];
            uint8_t g = pixels[pixel_index + 1];
            uint8_t b = pixels[pixel_index + 2];
            backbuffer[y * vbe.width + x] = (r << 16) | (g << 8) | b;
        }
    }
    stbi_image_free(pixels);
}

void draw_image_with_alpha(uint8_t* img_data, uint32_t img_size, int x, int y) {
    int width, height, channels;
    unsigned char *pixels = stbi_load_from_memory(img_data, img_size, &width, &height, &channels, 4);

    if (!pixels) return;

    for (int cy = 0; cy < height; cy++) {
        for (int cx = 0; cx < width; cx++) {
            int screen_x = x + cx;
            int screen_y = y + cy;

            if (screen_x < 0 || screen_x >= vbe.width || screen_y < 0 || screen_y >= vbe.height) continue;

            // ПЕРЕВЕРТАЄМО КАРТИНКУ ПО ВЕРТИКАЛІ
            int src_y = height - 1 - cy;
            int pixel_index = (src_y * width + cx) * 4;
            
            uint8_t r = pixels[pixel_index];
            uint8_t g = pixels[pixel_index + 1];
            uint8_t b = pixels[pixel_index + 2];
            uint8_t alpha = pixels[pixel_index + 3];

            if (alpha == 0) continue; 

            if (alpha == 255) {
                draw_pixel(screen_x, screen_y, (r << 16) | (g << 8) | b);
            } else {
                uint32_t bg_color = get_pixel(screen_x, screen_y);
                uint8_t r_bg = (bg_color >> 16) & 0xFF;
                uint8_t g_bg = (bg_color >> 8) & 0xFF;
                uint8_t b_bg = bg_color & 0xFF;

                uint8_t r_out = (r * alpha + r_bg * (255 - alpha)) >> 8;
                uint8_t g_out = (g * alpha + g_bg * (255 - alpha)) >> 8;
                uint8_t b_out = (b * alpha + b_bg * (255 - alpha)) >> 8;

                draw_pixel(screen_x, screen_y, (r_out << 16) | (g_out << 8) | b_out);
            }
        }
    }
    stbi_image_free(pixels);
}