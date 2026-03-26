OUTPUT_FORMAT("binary")
SECTIONS
{
    /* Точка завантаження програми (32 Мегабайти) */
    . = 0x2000000;

    .text : { 
        *(.entry)    /* 🔥 МАГІЯ: Цей код гарантовано буде найпершим! */
        *(.text) 
        *(.rodata) 
        *(.rodata.*) 
    }
    
    .data : { 
        *(.data) 
    }
    
    .bss : { 
        *(.bss) 
    }
}