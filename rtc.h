#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// Функція записує поточні значення в змінні, які ми їй передамо
void read_rtc(uint8_t* h, uint8_t* m, uint8_t* s, uint8_t* d, uint8_t* mo, uint32_t* y);

#endif