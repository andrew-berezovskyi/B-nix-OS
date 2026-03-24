#ifndef VBE_H
#define VBE_H

#include <stdint.h>
#include <stddef.h>

// Робимо змінну шрифту доступною всюди
extern uint8_t* main_font_data;

typedef struct {
    uint32_t* framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} vbe_info_t;

// Базова графіка
void init_graphics(uint32_t* fb_addr, uint32_t w, uint32_t h, uint32_t p, uint8_t b);
void draw_pixel(int x, int y, uint32_t color);
uint32_t get_pixel(int x, int y);
void swap_buffers(void);
void clear_screen(uint32_t color);

// Примітиви
void draw_filled_rect(int x, int y, int w, int h, uint32_t color);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void draw_filled_circle(int x, int y, int r, uint32_t color);
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);

// Шрифти
void draw_ttf_string(int x, int y, uint8_t* font_buffer, const char* str, float size, uint32_t color);
void draw_ttf_char(int x, int y, uint8_t* font_buffer, char c, float size, uint32_t color);

// Ефекти Kali
void draw_background_gradient();
void draw_b_nix_logo(int x, int y, int size);

// (Додай до існуючих прототипів)
void draw_filled_circle(int x, int y, int r, uint32_t color);
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void draw_ttf_string(int x, int y, uint8_t* font_buffer, const char* str, float size, uint32_t color);

// Нові функції в стилі Kali
void draw_cat_logo(int x, int y, int size, uint32_t color);
void draw_kali_style_background();
void draw_top_panel(uint32_t w, uint32_t h);

// (У vbe.h додаємо)
void draw_background_image(uint8_t* img_data, uint32_t img_size);
void draw_image_with_alpha(uint8_t* img_data, uint32_t img_size, int x, int y);

void draw_char(int x, int y, char c, uint32_t color, int scale);
void draw_string(int x, int y, const char* str, uint32_t color, int scale);

#endif