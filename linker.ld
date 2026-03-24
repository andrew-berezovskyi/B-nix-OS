ENTRY(_start)

SECTIONS {
    /* Починаємо завантаження ядра з 1 Мегабайта */
    . = 1M;

    /* Секція коду */
    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)
        *(.text)
    }

    /* Секція даних (тільки для читання) */
    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    /* Секція ініціалізованих даних */
    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    /* Секція неініціалізованих даних та стеку */
    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }
}