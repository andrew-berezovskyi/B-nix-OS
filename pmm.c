#include "pmm.h"

// Щоб підтримувати до 4 ГБ пам'яті, нам потрібно 1 мільйон блоків.
// 1 мільйон бітів / 32 біти (розмір uint32_t) = 32768 елементів масиву.
// Цей масив займе всього 128 КБ в нашому ядрі, але дозволить керувати всіма 4 ГБ!
static uint32_t memory_bitmap[32768]; 

static uint32_t max_blocks = 0;
static uint32_t used_blocks = 0;

// Встановити біт в 1 (зайнято)
static inline void bitmap_set(uint32_t bit) {
    memory_bitmap[bit / 32] |= (1 << (bit % 32));
}

// Встановити біт в 0 (вільно)
static inline void bitmap_unset(uint32_t bit) {
    memory_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// Перевірити стан біта
static inline int bitmap_test(uint32_t bit) {
    return memory_bitmap[bit / 32] & (1 << (bit % 32));
}

// Знайти перший вільний блок (перший нульовий біт)
static int bitmap_find_first_free() {
    for (uint32_t i = 0; i < max_blocks / 32; i++) {
        if (memory_bitmap[i] != 0xFFFFFFFF) { // Якщо хоч один біт в цьому uint32_t дорівнює 0
            for (int j = 0; j < 32; j++) {
                int bit = i * 32 + j;
                if (!bitmap_test(bit)) {
                    return bit;
                }
            }
        }
    }
    return -1; // Немає вільної пам'яті!
}

void init_pmm(uint32_t mem_size_kb) {
    max_blocks = (mem_size_kb * 1024) / PMM_BLOCK_SIZE;
    used_blocks = max_blocks; // Спочатку робимо всю пам'ять "зайнятою"

    // Заповнюємо весь масив одиницями (всі блоки зайняті)
    for (uint32_t i = 0; i < 32768; i++) {
        memory_bitmap[i] = 0xFFFFFFFF;
    }

    // Тепер звільняємо тільки ту пам'ять, яка реально існує і доступна нам.
    // ВАЖЛИВО: Ми починаємо звільняти пам'ять з 2-го Мегабайта (блок 512 і далі),
    // тому що в перших 2 МБ лежить наше ядро, VGA-буфер (0xB8000) та BIOS.
    for (uint32_t i = 512; i < max_blocks; i++) {
        bitmap_unset(i);
        used_blocks--;
    }
}

uint32_t pmm_alloc_block(void) {
    if (pmm_get_free_block_count() == 0) {
        return 0; // Пам'ять закінчилась (Out of Memory)
    }

    int frame = bitmap_find_first_free();
    if (frame == -1) return 0;

    bitmap_set(frame);
    used_blocks++;

    // Повертаємо фізичну адресу в пам'яті: номер блоку * 4096
    return frame * PMM_BLOCK_SIZE; 
}

void pmm_free_block(uint32_t address) {
    uint32_t frame = address / PMM_BLOCK_SIZE;
    if (bitmap_test(frame)) {
        bitmap_unset(frame);
        used_blocks--;
    }
}

uint32_t pmm_get_free_block_count(void) {
    return max_blocks - used_blocks;
}

// ДОДАНО: Функція для отримання загальної кількості блоків
uint32_t pmm_get_max_blocks(void) {
    return max_blocks;
}