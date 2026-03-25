#include "gui.h"
#include "vbe.h"
#include "rtc.h"

#define DESKTOP_TOP_BAR_H   32
#define DESKTOP_DOCK_W      60
#define DESKTOP_TITLE_H     32

void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) { draw_pixel(x + i, y, color); draw_pixel(x + i, y + h - 1, color); }
    for (int i = 0; i < h; i++) { draw_pixel(x, y + i, color); draw_pixel(x + w - 1, y + i, color); }
}

void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h; cy++) for (int cx = x; cx < x + w; cx++) draw_pixel(cx, cy, color);
}

static int rtc_weekday_sun0(uint32_t year, uint8_t month, uint8_t day) {
    const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (int)((year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7);
}

static void format_rtc_datetime_kali(char* out, size_t cap) {
    uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y);
    if (cap < 28) { out[0] = '\0'; return; }
    static const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mn[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int wdi = rtc_weekday_sun0(y, mo, d); if (wdi < 0) wdi = 0; if (wdi > 6) wdi = 6;
    const char* wds = wd[wdi]; const char* ms = mn[(mo >= 1 && mo <= 12) ? (mo - 1) : 0];
    size_t p = 0; while (wds[0] && p + 1 < cap) out[p++] = *wds++;
    if (p + 1 < cap) out[p++] = ' '; while (ms[0] && p + 1 < cap) out[p++] = *ms++;
    if (p + 1 < cap) out[p++] = ' ';
    if (d >= 10) { if (p + 1 < cap) out[p++] = (char)('0' + (d / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (d % 10)); } else { if (p + 1 < cap) out[p++] = (char)('0' + d); }
    if (p + 1 < cap) out[p++] = ','; if (p + 1 < cap) out[p++] = ' ';
    if (p + 1 < cap) out[p++] = (char)('0' + (hour / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (hour % 10));
    if (p + 1 < cap) out[p++] = ':';
    if (p + 1 < cap) out[p++] = (char)('0' + (min / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (min % 10));
    out[p] = '\0';
}

static void draw_desktop_dock_blend(uint32_t w, uint32_t h) {
    const uint32_t dock_c = 0x111111; const uint8_t alpha = 200;
    for (int y = (int)DESKTOP_TOP_BAR_H; y < (int)h; y++) {
        for (int x = 0; x < DESKTOP_DOCK_W; x++) {
            uint32_t bg = get_pixel(x, y);
            uint8_t br = (uint8_t)(bg >> 16), bg_g = (uint8_t)(bg >> 8), bb = (uint8_t)bg;
            uint8_t fr = (uint8_t)(dock_c >> 16), fg = (uint8_t)(dock_c >> 8), fb = (uint8_t)dock_c;
            uint8_t r = (uint8_t)((fr * alpha + br * (255 - alpha)) >> 8);
            uint8_t g = (uint8_t)((fg * alpha + bg_g * (255 - alpha)) >> 8);
            uint8_t b = (uint8_t)((fb * alpha + bb * (255 - alpha)) >> 8);
            draw_pixel(x, y, (r << 16) | (g << 8) | b);
        }
    }
    draw_filled_rect(DESKTOP_DOCK_W - 2, DESKTOP_TOP_BAR_H + 4, 2, (int)h - DESKTOP_TOP_BAR_H - 4, 0x2A4A66);
}

static void draw_dock_icon_placeholder(int x, int y, uint32_t accent) {
    draw_rounded_rect(x, y, 40, 40, 8, 0x2A2F38); draw_rect_outline(x, y, 40, 40, accent);
}

static void draw_status_tray_icons(int right_x, int y0) {
    int x = right_x;
    draw_filled_rect(x, y0, 18, 10, 0x3A3A3A); draw_filled_rect(x + 18, y0 + 2, 3, 6, 0x3A3A3A); draw_filled_rect(x + 3, y0 + 2, 10, 6, 0x55AA66); x += 28;
    draw_pixel(x + 8, y0 + 8, 0xCCCCCC); draw_rect_outline(x + 4, y0 + 4, 8, 6, 0xAAAAAA); draw_rect_outline(x + 2, y0 + 2, 12, 10, 0x888888); x += 28;
    draw_filled_rect(x, y0 + 4, 4, 8, 0xCCCCCC); draw_filled_rect(x + 4, y0 + 2, 10, 12, 0x666666);
}

static void draw_top_bar_kali(uint32_t w) {
    draw_filled_rect(0, 0, w, DESKTOP_TOP_BAR_H, 0x111111);
    const int bl = 22;
    if (main_font_data) {
        draw_ttf_string(14, bl, main_font_data, "Activities", 15.0f, 0xE8E8E8);
        int ix = 110; draw_filled_rect(ix, 8, 18, 16, 0x4A90D9); draw_filled_rect(ix + 24, 8, 18, 16, 0x2D2D2D); draw_filled_rect(ix + 52, 8, 18, 16, 0x3D3D3D);
        char dt[32]; format_rtc_datetime_kali(dt, sizeof(dt));
        int tw = measure_ttf_text_width(main_font_data, dt, 15.0f);
        draw_ttf_string((int)((w - (uint32_t)tw) / 2), bl, main_font_data, dt, 15.0f, 0xE8E8E8);
        draw_status_tray_icons((int)w - 118, 11);
    }
}

void draw_desktop_chrome(uint32_t w, uint32_t h) {
    draw_top_bar_kali(w); draw_desktop_dock_blend(w, h);
    for (int i = 0; i < 3; i++) {
        uint32_t ac = (i == 0) ? 0x4A90D9 : ((i == 1) ? 0x00A0C8 : 0x666666);
        draw_dock_icon_placeholder(10, DESKTOP_TOP_BAR_H + 14 + i * 58, ac);
    }
}

void draw_kali_window_frame(int x, int y, int ww, int wh, bool active, const char* title) {
    uint32_t border = active ? 0x00A0C8 : 0x3A3A3A;
    draw_filled_rect(x, y, ww, wh, 0x1A1A1A); draw_rect_outline(x, y, ww, wh, border);
    uint32_t title_bg = active ? 0x252A30 : 0x1E1E1E;
    draw_filled_rect(x + 1, y + 1, ww - 2, DESKTOP_TITLE_H - 1, title_bg);
    if (active) draw_filled_rect(x + 1, y + 1, ww - 2, 3, 0x00B4D8);
    
    // ВИПРАВЛЕНО ТУТ: Було 22, стало (y + 22)
    if (main_font_data) draw_ttf_string(x + 10, y + 22, main_font_data, title, 14.0f, 0xE0E0E0);
    
    int cl_x = x + ww - 34; 
    draw_filled_rect(cl_x, y + 4, 28, 24, 0xC03030);
    
    // ВИПРАВЛЕНО ТУТ: Було 20, стало (y + 20)
    if (main_font_data) draw_ttf_string(cl_x + 8, y + 20, main_font_data, "X", 13.0f, 0xFFFFFF);
}