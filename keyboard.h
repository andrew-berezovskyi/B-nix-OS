#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Ці змінні зберігатимуть останню натиснуту клавішу
extern volatile char last_key_pressed;
extern volatile bool key_ready;

void init_keyboard(void);

#endif