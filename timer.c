#include "timer.h"
#include "io.h"

// Лічильник "тіків" таймера (volatile каже компілятору не оптимізувати цю змінну, бо вона змінюється апаратно)
volatile uint32_t timer_ticks = 0;
// Зберігаємо частоту, щоб знати, скільки тіків в 1 секунді
uint32_t current_frequency = 0;

// Обробник переривання (викликатиметься з Асемблера)
void timer_handler_main(void) {
    timer_ticks++;

    // Обов'язково кажемо Master PIC, що ми обробили переривання
    outb(0x20, 0x20);
}

// Ініціалізація мікросхеми PIT
void init_timer(uint32_t frequency) {
    current_frequency = frequency;

    // Базова апаратна частота генератора на материнській платі завжди 1.193182 МГц
    uint32_t divisor = 1193180 / frequency;

    // Відправляємо команду (0x36) в регістр управління PIT (порт 0x43)
    // Це означає: Канал 0, режим "Square Wave Generator"
    outb(0x43, 0x36);

    // Відправляємо дільник (по байтах) у канал 0 (порт 0x40)
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    
    outb(0x40, low);
    outb(0x40, high);
}

// Функція для отримання часу роботи в секундах
uint32_t get_uptime_seconds(void) {
    if (current_frequency == 0) return 0;
    return timer_ticks / current_frequency;
}


// ДОДАНО: Функція сну
void sleep(uint32_t seconds) {
    uint32_t start_ticks = timer_ticks;
    uint32_t wait_ticks = seconds * current_frequency;

    asm volatile("sti"); // Дозволяємо переривання, щоб таймер міг оновлювати timer_ticks
    
    // Поки не пройде потрібна кількість тіків
    while ((timer_ticks - start_ticks) < wait_ticks) {
        // Кажемо процесору заснути до наступного тіку (економимо 100% CPU)
        asm volatile("hlt");
    }
}