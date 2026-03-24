#include <stdint.h>
#include <stdbool.h>
#include "io.h"
#include "vbe.h"
#include "mouse.h"

int mouse_x = 640;
int mouse_y = 360;
bool mouse_left_pressed = false;

uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

const uint8_t cursor_shape[16][12] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,2,2,2,2,2,0},
    {2,1,1,2,1,1,2,0,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0},
    {2,0,0,0,0,2,2,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

void draw_cursor(int x, int y) {
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 12; cx++) {
            if (cursor_shape[cy][cx] == 1) draw_pixel(x + cx, y + cy, 0x000000);
            if (cursor_shape[cy][cx] == 2) draw_pixel(x + cx, y + cy, 0xFFFFFF);
        }
    }
}

void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) while (timeout--) if ((inb(0x64) & 1) == 1) return;
    else while (timeout--) if ((inb(0x64) & 2) == 0) return;
}
void mouse_write(uint8_t a_write) {
    mouse_wait(1); outb(0x64, 0xD4); mouse_wait(1); outb(0x60, a_write);
}
uint8_t mouse_read() {
    mouse_wait(0); return inb(0x60);
}

void mouse_handler_main() {
    uint8_t status = inb(0x64);
    while (status & 0x01) {
        int8_t mouse_in = inb(0x60);
        if (status & 0x20) {
            switch (mouse_cycle) {
                case 0:
                    mouse_byte[0] = mouse_in;
                    if (!(mouse_in & 0x08)) break; 
                    mouse_left_pressed = (mouse_in & 0x01) != 0;
                    mouse_cycle++; break;
                case 1:
                    mouse_byte[1] = mouse_in;
                    mouse_cycle++; break;
                case 2:
                    mouse_byte[2] = mouse_in;
                    mouse_cycle = 0;

                    mouse_x += mouse_byte[1];
                    mouse_y -= mouse_byte[2];

                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_x > 1280 - 12) mouse_x = 1280 - 12;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_y > 720 - 16) mouse_y = 720 - 16;
                    break;
            }
        }
        status = inb(0x64);
    }
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void init_mouse() {
    uint8_t status;
    mouse_wait(1); outb(0x64, 0xA8); 
    mouse_wait(1); outb(0x64, 0x20); mouse_wait(0); status = (inb(0x60) | 2); 
    mouse_wait(1); outb(0x64, 0x60); mouse_wait(1); outb(0x60, status);
    mouse_write(0xF6); mouse_read(); mouse_write(0xF4); mouse_read();
}