#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

struct surface {
    uint8_t *pixels;
    size_t length;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
};

static const uint8_t font[128][5] = {
    [' ']={0,0,0,0,0}, ['-']={8,8,8,8,8}, [':']={0,20,0,20,0},
    ['0']={31,17,17,17,31}, ['1']={0,18,31,16,0},
    ['2']={29,21,21,21,23}, ['3']={17,21,21,21,31},
    ['4']={7,4,4,4,31}, ['5']={23,21,21,21,29},
    ['6']={31,21,21,21,29}, ['7']={1,1,25,5,3},
    ['8']={31,21,21,21,31}, ['9']={23,21,21,21,31},
    ['A']={30,5,5,5,30}, ['B']={31,21,21,21,10},
    ['C']={14,17,17,17,10}, ['D']={31,17,17,17,14},
    ['E']={31,21,21,21,17}, ['F']={31,5,5,5,1},
    ['G']={14,17,21,21,29}, ['H']={31,4,4,4,31},
    ['I']={17,17,31,17,17}, ['J']={8,16,16,16,15},
    ['K']={31,4,10,17,0}, ['L']={31,16,16,16,16},
    ['M']={31,2,4,2,31}, ['N']={31,2,4,8,31},
    ['O']={14,17,17,17,14}, ['P']={31,5,5,5,2},
    ['Q']={14,17,25,17,30}, ['R']={31,5,13,21,18},
    ['S']={18,21,21,21,9}, ['T']={1,1,31,1,1},
    ['U']={15,16,16,16,15}, ['V']={7,8,16,8,7},
    ['W']={31,8,4,8,31}, ['X']={17,10,4,10,17},
    ['Y']={1,2,28,2,1}, ['Z']={25,21,21,19,0}
};

static uint32_t channel(unsigned value, const struct fb_bitfield *field) {
    if (field->length == 0) return 0;
    unsigned max = (1u << field->length) - 1u;
    return ((value * max / 255u) << field->offset);
}

static uint32_t color(const struct surface *s, unsigned r, unsigned g, unsigned b) {
    return channel(r, &s->var.red) | channel(g, &s->var.green) |
           channel(b, &s->var.blue);
}

static void pixel(struct surface *s, int x, int y, uint32_t value) {
    if (x < 0 || y < 0 || x >= (int)s->var.xres || y >= (int)s->var.yres) return;
    size_t offset = (size_t)(y + (int)s->var.yoffset) * s->fix.line_length +
                    (size_t)(x + (int)s->var.xoffset) * (s->var.bits_per_pixel / 8u);
    if (offset + 4u > s->length) return;
    if (s->var.bits_per_pixel == 32) *(uint32_t *)(s->pixels + offset) = value;
    else if (s->var.bits_per_pixel == 16) *(uint16_t *)(s->pixels + offset) = (uint16_t)value;
}

static void rect(struct surface *s, int x, int y, int w, int h, int radius, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            int dx = px < radius ? radius - px : (px >= w - radius ? px - (w - radius - 1) : 0);
            int dy = py < radius ? radius - py : (py >= h - radius ? py - (h - radius - 1) : 0);
            if (dx * dx + dy * dy <= radius * radius) pixel(s, x + px, y + py, c);
        }
    }
}

static void circle(struct surface *s, int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) pixel(s, cx + x, cy + y, c);
}

static void text(struct surface *s, int x, int y, const char *value, int scale, uint32_t c) {
    for (; *value; ++value) {
        unsigned char ch = (unsigned char)*value;
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if ((font[ch][col] >> row) & 1u)
                    rect(s, x + col * scale, y + row * scale, scale, scale, 0, c);
        x += 6 * scale;
    }
}

static void render(struct surface *s) {
    int w = (int)s->var.xres, h = (int)s->var.yres;
    uint32_t navy = color(s, 7, 15, 34);
    uint32_t blue = color(s, 24, 74, 146);
    uint32_t panel = color(s, 20, 31, 54);
    uint32_t panel2 = color(s, 31, 45, 72);
    uint32_t white = color(s, 239, 246, 255);
    uint32_t muted = color(s, 139, 160, 190);
    uint32_t accent = color(s, 86, 156, 255);
    uint32_t mint = color(s, 91, 218, 183);
    uint32_t coral = color(s, 255, 115, 125);
    uint32_t gold = color(s, 255, 195, 92);

    for (int y = 0; y < h; ++y) {
        unsigned mix = (unsigned)(y * 100 / (h ? h : 1));
        uint32_t bg = color(s, 7 + mix / 12, 15 + mix / 9, 34 + mix / 5);
        rect(s, 0, y, w, 1, 0, bg);
    }
    circle(s, w * 4 / 5, h / 3, h / 3, blue);
    circle(s, w / 8, h * 4 / 5, h / 4, navy);

    int bar_h = h < 600 ? 42 : 50;
    rect(s, 0, 0, w, bar_h, 0, panel);
    circle(s, 27, bar_h / 2, 11, accent);
    text(s, 48, bar_h / 2 - 7, "B-NIX", 2, white);
    text(s, w / 2 - 42, bar_h / 2 - 7, "AURORA", 2, muted);

    time_t now = time(NULL);
    struct tm tm_now;
    char clock_text[16] = "00:00";
    if (localtime_r(&now, &tm_now) != NULL)
        (void)snprintf(clock_text, sizeof clock_text, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    text(s, w - 100, bar_h / 2 - 7, clock_text, 2, white);

    int card_w = w < 900 ? w * 4 / 5 : 660;
    int card_h = h < 600 ? 180 : 220;
    int card_x = (w - card_w) / 2;
    int card_y = (h - card_h) / 2 - 20;
    rect(s, card_x + 8, card_y + 10, card_w, card_h, 26, navy);
    rect(s, card_x, card_y, card_w, card_h, 26, panel2);
    text(s, card_x + 42, card_y + 38, "WELCOME TO B-NIX", w < 800 ? 2 : 3, white);
    text(s, card_x + 44, card_y + 88, "LINUX FOUNDATION READY", 2, mint);
    text(s, card_x + 44, card_y + 122, "AURORA DESKTOP PREVIEW", 2, muted);
    rect(s, card_x + 42, card_y + card_h - 52, 180, 32, 12, accent);
    text(s, card_x + 68, card_y + card_h - 43, "OPEN SESSION", 2, white);

    int dock_w = w < 700 ? w - 40 : 430;
    int dock_h = h < 500 ? 60 : 76;
    int dock_x = (w - dock_w) / 2;
    int dock_y = h - dock_h - 22;
    rect(s, dock_x + 5, dock_y + 7, dock_w, dock_h, 24, navy);
    rect(s, dock_x, dock_y, dock_w, dock_h, 24, panel);
    uint32_t icons[5] = {accent, mint, gold, coral, color(s, 167, 139, 250)};
    int gap = dock_w / 6;
    int ir = dock_h / 3;
    for (int i = 0; i < 5; ++i) circle(s, dock_x + gap * (i + 1), dock_y + dock_h / 2, ir, icons[i]);
}

int main(void) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "bnix-shell: open /dev/fb0: %s\n", strerror(errno));
        return 1;
    }
    struct surface s;
    memset(&s, 0, sizeof s);
    if (ioctl(fd, FBIOGET_FSCREENINFO, &s.fix) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &s.var) < 0) {
        fprintf(stderr, "bnix-shell: framebuffer query failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    if (s.var.bits_per_pixel != 32 && s.var.bits_per_pixel != 16) {
        fprintf(stderr, "bnix-shell: unsupported depth %u\n", s.var.bits_per_pixel);
        close(fd);
        return 1;
    }
    s.length = s.fix.smem_len ? s.fix.smem_len : (size_t)s.fix.line_length * s.var.yres;
    s.pixels = mmap(NULL, s.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s.pixels == MAP_FAILED) {
        fprintf(stderr, "bnix-shell: mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    render(&s);
    int console = open("/dev/console", O_WRONLY);
    if (console >= 0) {
        dprintf(console, "[BNIX-GUI] shell ready %ux%u@%u\n",
                s.var.xres, s.var.yres, s.var.bits_per_pixel);
        close(console);
    }
    for (;;) {
        sleep(1);
        render(&s);
    }
}
