#include "vbe.h"
#include "font.h"
#include "kheap.h"
#include "kstring.h"
#include "rtc.h" 
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef __bool_true_false_are_defined
#define bool _Bool
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif

extern uint8_t* main_font_data;

double fabs(double x) { return x < 0 ? -x : x; }
double floor(double x) { return (double)((int)x); }
double ceil(double x) { int i = (int)x; return (x > (double)i) ? (double)(i + 1) : (double)i; }
double fmod(double x, double y) { return x - (int)(x / y) * y; }
double cos(double x) { double x2 = x * x; return 1.0 - (x2 / 2.0) + (x2 * x2 / 24.0) - (x2 * x2 * x2 / 720.0); }
double acos(double x) { return 1.57079 - x; }
double sqrt(double x) { if (x < 0) return 0; double z = 1.0; for (int i = 0; i < 10; i++) z -= (z*z - x) / (2*z); return z; }
double pow(double x, double y) { double res = 1; for(int i = 0; i < (int)y; i++) res *= x; return res; }

void* stbi_realloc_sized(void* ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) { kfree(ptr); return NULL; }
    if (!ptr) return kmalloc(new_size);
    void* new_ptr = kmalloc(new_size);
    if (new_ptr) { memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size); kfree(ptr); }
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
// 🔥 COMPOSITOR RENDER TARGETS
// ==============================================================================
static vbe_info_t vbe;
static uint32_t backbuffer[1280 * 720];
static uint32_t frontbuffer[1280 * 720]; 

// Поточна ціль малювання (може бути екраном або буфером вікна)
static uint32_t* current_rt = NULL;
static uint32_t rt_w = 0;
static uint32_t rt_h = 0;

void init_graphics(uint32_t* fb_addr, uint32_t w, uint32_t h, uint32_t p, uint8_t b) {
    vbe.framebuffer = fb_addr; vbe.width = w; vbe.height = h; vbe.pitch = p; vbe.bpp = b;
    for(int i=0; i < 1280 * 720; i++) {
        backbuffer[i] = 0;
        frontbuffer[i] = 0;
    }
    current_rt = backbuffer;
    rt_w = w;
    rt_h = h;
}

void set_render_target(uint32_t* target, int w, int h) {
    current_rt = target; rt_w = w; rt_h = h;
}

void reset_render_target(void) {
    current_rt = backbuffer; rt_w = vbe.width; rt_h = vbe.height;
}

// НОВЕ: Швидке копіювання готового буфера вікна на екран
void draw_buffer_to_screen(uint32_t* buffer, int bw, int bh, int x, int y) {
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + bw > (int)vbe.width) ? (int)vbe.width : x + bw;
    int end_y = (y + bh > (int)vbe.height) ? (int)vbe.height : y + bh;
    
    int draw_w = end_x - start_x;
    if (draw_w <= 0 || start_y >= end_y) return;
    
    for (int cy = start_y; cy < end_y; cy++) {
        int src_y = cy - y;
        uint32_t* dst_row = &backbuffer[cy * vbe.width + start_x];
        uint32_t* src_row = &buffer[src_y * bw + (start_x - x)];
        uint32_t count = draw_w;
        asm volatile ("cld; rep movsl" : "+S"(src_row), "+D"(dst_row), "+c"(count) :: "memory");
    }
}

void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)rt_w || y < 0 || y >= (int)rt_h) return;
    current_rt[y * rt_w + x] = color; 
}

uint32_t get_pixel(int x, int y) {
    if (x < 0 || x >= (int)rt_w || y < 0 || y >= (int)rt_h) return 0;
    return current_rt[y * rt_w + x];
}

void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + w > (int)rt_w) ? (int)rt_w : x + w;
    int end_y = (y + h > (int)rt_h) ? (int)rt_h : y + h;
    
    int draw_w = end_x - start_x;
    if (draw_w <= 0 || start_y >= end_y) return;

    for (int cy = start_y; cy < end_y; cy++) {
        uint32_t* row = &current_rt[cy * rt_w + start_x];
        uint32_t count = draw_w;
        asm volatile ("cld; rep stosl" : "+D"(row), "+c"(count) : "a"(color) : "memory");
    }
}

void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    draw_filled_rect(x, y, w, 1, color);
    draw_filled_rect(x, y + h - 1, w, 1, color);
    for (int i = 1; i < h - 1; i++) {
        draw_pixel(x, y + i, color);
        draw_pixel(x + w - 1, y + i, color);
    }
}

void swap_buffers(void) {
    uint32_t* src = backbuffer;
    uint32_t* dst = (uint32_t*)vbe.framebuffer;
    uint32_t* shadow = frontbuffer;
    uint32_t width = vbe.width;
    uint32_t height = vbe.height;

    for (uint32_t y = 0; y < height; y++) {
        uint32_t offset = y * width;
        uint32_t min_x = width;
        uint32_t max_x = 0;
        for (uint32_t x = 0; x < width; x++) {
            if (src[offset + x] != shadow[offset + x]) {
                if (min_x == width) min_x = x; max_x = x;                    
            }
        }
        if (min_x < width) {
            uint32_t count = max_x - min_x + 1;
            uint32_t start = offset + min_x;
            uint32_t* dst_ptr = dst + start;
            uint32_t* src_ptr = src + start;
            uint32_t* shd_ptr = shadow + start;
            for(uint32_t i = 0; i < count; i++) {
                uint32_t pixel = src_ptr[i];
                dst_ptr[i] = pixel; shd_ptr[i] = pixel;    
            }
        }
    }
}

void clear_screen(uint32_t color) {
    uint32_t* dest = current_rt; uint32_t count = rt_w * rt_h;
    asm volatile ("cld; rep stosl" : "+D"(dest), "+c"(count) : "a"(color) : "memory" );
}

typedef struct {
    uint8_t* bitmap;
    int width, height, xoff, yoff, advance;
} cached_glyph_t;

static cached_glyph_t glyph_cache[4][128]; 
static bool cache_initialized[4] = {false, false, false, false};

static int get_size_index(float size) {
    if (size == 11.0f) return 0;
    if (size == 12.0f) return 1;
    if (size == 13.0f) return 2;
    if (size == 14.0f) return 3;
    return -1;
}

static void init_glyph_cache(uint8_t* font_buffer, float size, int size_idx) {
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_buffer, 0)) return;
    float scale = stbtt_ScaleForPixelHeight(&font, size);
    
    for (int i = 32; i < 127; i++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, i, &advance, &lsb);
        glyph_cache[size_idx][i].advance = (int)(advance * scale);
        glyph_cache[size_idx][i].bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, i, 
            &glyph_cache[size_idx][i].width, &glyph_cache[size_idx][i].height, 
            &glyph_cache[size_idx][i].xoff, &glyph_cache[size_idx][i].yoff);
    }
    cache_initialized[size_idx] = true;
}

void draw_ttf_char_internal(stbtt_fontinfo* font, int x, int y, char c, float size, uint32_t color) {
    int width, height, xoff, yoff;
    float scale = stbtt_ScaleForPixelHeight(font, size);
    unsigned char* bitmap = stbtt_GetCodepointBitmap(font, 0, scale, c, &width, &height, &xoff, &yoff);
    if (bitmap) {
        uint8_t r_src = (color >> 16) & 0xFF, g_src = (color >> 8) & 0xFF, b_src = color & 0xFF;
        for (int cy = 0; cy < height; cy++) {
            for (int cx = 0; cx < width; cx++) {
                uint8_t alpha = bitmap[cy * width + cx];
                if (alpha > 0) {
                    int sx = x + cx + xoff, sy = y + cy + yoff;
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
    if (!font_buffer) return;
    int size_idx = get_size_index(size);
    
    if (size_idx != -1) {
        if (!cache_initialized[size_idx]) { init_glyph_cache(font_buffer, size, size_idx); }
        int current_x = x;
        uint8_t r_src = (color >> 16) & 0xFF, g_src = (color >> 8) & 0xFF, b_src = color & 0xFF;
        for (int i = 0; str[i] != '\0'; i++) {
            unsigned char c = str[i];
            if (c < 32 || c > 126) c = '?'; 
            cached_glyph_t* g = &glyph_cache[size_idx][c];
            if (g->bitmap) {
                for (int cy = 0; cy < g->height; cy++) {
                    int sy = y + cy + g->yoff;
                    if (sy < 0 || sy >= (int)rt_h) continue;
                    for (int cx = 0; cx < g->width; cx++) {
                        int sx = current_x + cx + g->xoff;
                        if (sx < 0 || sx >= (int)rt_w) continue;
                        uint8_t alpha = g->bitmap[cy * g->width + cx];
                        if (alpha > 0) {
                            if (alpha == 255) { current_rt[sy * rt_w + sx] = color; } 
                            else {
                                uint32_t bg = current_rt[sy * rt_w + sx];
                                uint8_t r = (r_src * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
                                uint8_t g = (g_src * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
                                uint8_t b = (b_src * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
                                current_rt[sy * rt_w + sx] = (r << 16) | (g << 8) | b;
                            }
                        }
                    }
                }
            }
            current_x += g->advance;
        }
    } else {
        stbtt_fontinfo font; if (!stbtt_InitFont(&font, font_buffer, 0)) return;
        float scale = stbtt_ScaleForPixelHeight(&font, size);
        int current_x = x;
        for (int i = 0; str[i] != '\0'; i++) {
            draw_ttf_char_internal(&font, current_x, y, str[i], size, color);
            int advance, lsb; stbtt_GetCodepointHMetrics(&font, str[i], &advance, &lsb);
            current_x += (int)(advance * scale);
        }
    }
}

int measure_ttf_text_width(uint8_t* font_buffer, const char* str, float size) {
    if (!font_buffer || !str) return 0;
    int size_idx = get_size_index(size);
    if (size_idx != -1) {
        if (!cache_initialized[size_idx]) init_glyph_cache(font_buffer, size, size_idx);
        int total = 0;
        for (int i = 0; str[i] != '\0'; i++) {
            unsigned char c = str[i];
            if (c >= 32 && c <= 126) total += glyph_cache[size_idx][c].advance;
        }
        return total;
    } else {
        stbtt_fontinfo font; if (!stbtt_InitFont(&font, font_buffer, 0)) return 0;
        float scale = stbtt_ScaleForPixelHeight(&font, size);
        int total = 0;
        for (int i = 0; str[i] != '\0'; i++) {
            int advance, lsb; stbtt_GetCodepointHMetrics(&font, str[i], &advance, &lsb);
            total += (int)(advance * scale);
        }
        return total;
    }
}

static uint32_t* background_cache = NULL;
static uint32_t background_cache_w = 0, background_cache_h = 0;
static uint32_t* icon_cache = NULL; int icon_cache_w = 0, icon_cache_h = 0;
static uint32_t* folder_cache = NULL; int folder_cache_w = 0, folder_cache_h = 0;
static uint32_t* file_cache = NULL; int file_cache_w = 0, file_cache_h = 0;

static uint32_t* decode_icon(uint8_t* img_data, uint32_t img_size, int* w_out, int* h_out) {
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(img_data, img_size, &width, &height, &channels, 4);
    if (!pixels) return NULL;
    uint32_t* rgba = (uint32_t*)kmalloc((size_t)width * (size_t)height * sizeof(uint32_t));
    if (!rgba) { stbi_image_free(pixels); return NULL; }
    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y; 
        for (int x = 0; x < width; x++) {
            int i = (src_y * width + x) * 4;
            rgba[y * width + x] = (pixels[i+3] << 24) | (pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2];
        }
    }
    stbi_image_free(pixels);
    *w_out = width; *h_out = height;
    return rgba;
}

void draw_cached_icon_at(uint32_t* cache, int cache_w, int cache_h, int x, int y) {
    if (!cache) return;
    for (int cy = 0; cy < cache_h; cy++) {
        int dy = y + cy;
        if (dy < 0 || dy >= (int)rt_h) continue;
        uint32_t* row = &current_rt[dy * rt_w];
        for (int cx = 0; cx < cache_w; cx++) {
            int dx = x + cx;
            if (dx < 0 || dx >= (int)rt_w) continue;
            uint32_t px = cache[cy * cache_w + cx];
            uint8_t a = (uint8_t)(px >> 24);
            if (a == 0) continue; 
            uint8_t r = (uint8_t)(px >> 16), g = (uint8_t)(px >> 8), b = (uint8_t)px;
            if (a == 255) { row[dx] = (r << 16) | (g << 8) | b; } 
            else {
                uint32_t bg = row[dx];
                uint8_t rb = (uint8_t)(bg >> 16), gb = (uint8_t)(bg >> 8), bb = (uint8_t)bg;
                uint8_t ro = (r * a + rb * (255 - a)) >> 8;
                uint8_t go = (g * a + gb * (255 - a)) >> 8;
                uint8_t bo = (b * a + bb * (255 - a)) >> 8;
                row[dx] = (ro << 16) | (go << 8) | bo;
            }
        }
    }
}

bool cache_icon_image(uint8_t* img_data, uint32_t img_size) {
    if (icon_cache) kfree(icon_cache);
    icon_cache = decode_icon(img_data, img_size, &icon_cache_w, &icon_cache_h);
    return icon_cache != NULL;
}
bool cache_folder_image(uint8_t* img_data, uint32_t img_size) {
    if (folder_cache) kfree(folder_cache);
    folder_cache = decode_icon(img_data, img_size, &folder_cache_w, &folder_cache_h);
    return folder_cache != NULL;
}
bool cache_file_image(uint8_t* img_data, uint32_t img_size) {
    if (file_cache) kfree(file_cache);
    file_cache = decode_icon(img_data, img_size, &file_cache_w, &file_cache_h);
    return file_cache != NULL;
}

void draw_icon_folder(int x, int y) { if (folder_cache) draw_cached_icon_at(folder_cache, folder_cache_w, folder_cache_h, x, y); }
void draw_icon_file(int x, int y) { if (file_cache) draw_cached_icon_at(file_cache, file_cache_w, file_cache_h, x, y); }
void draw_cached_icon_centered(int center_x, int center_y) {
    if (!icon_cache) return;
    draw_cached_icon_at(icon_cache, icon_cache_w, icon_cache_h, center_x - (icon_cache_w / 2), center_y - (icon_cache_h / 2));
}

bool cache_background_image(uint8_t* img_data, uint32_t img_size) {
    int src_w, src_h, channels;
    unsigned char* pixels = stbi_load_from_memory(img_data, img_size, &src_w, &src_h, &channels, 3);
    if (!pixels) return false;
    uint32_t target_w = vbe.width, target_h = vbe.height;
    uint32_t* new_cache = (uint32_t*)kmalloc(target_w * target_h * sizeof(uint32_t));
    if (!new_cache) { stbi_image_free(pixels); return false; }
    int dst_x0 = ((int)target_w - src_w) / 2;
    int dst_y0 = ((int)target_h - src_h) / 2;
    for (int y = 0; y < src_h; y++) {
        int dst_y = dst_y0 + y;
        if (dst_y < 0 || dst_y >= (int)target_h) continue;
        int src_y = src_h - 1 - y;
        uint32_t* row = &new_cache[dst_y * target_w];
        for (int x = 0; x < src_w; x++) {
            int dst_x = dst_x0 + x;
            if (dst_x < 0 || dst_x >= (int)target_w) continue;
            int i = (src_y * src_w + x) * 3;
            row[dst_x] = (pixels[i] << 16) | (pixels[i+1] << 8) | pixels[i+2];
        }
    }
    if (background_cache) kfree(background_cache);
    background_cache = new_cache; background_cache_w = target_w; background_cache_h = target_h;
    stbi_image_free(pixels);
    return true;
}

void draw_cached_background(void) {
    if (!background_cache || background_cache_w != vbe.width || background_cache_h != vbe.height) { clear_screen(0x000000); return; }
    // Фон завжди малюється на backbuffer екрану
    memcpy(backbuffer, background_cache, vbe.width * vbe.height * sizeof(uint32_t));
}