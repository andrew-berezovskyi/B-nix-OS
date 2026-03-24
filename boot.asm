section .multiboot
align 4
    dd 0x1BADB002            ; Magic
    dd 0x00000007            ; Flags: ALIGN(1) | MEMINFO(2) | GRAPHICS(4) = 7
    dd -(0x1BADB002 + 0x00000007) ; Checksum

    ; VESA/VBE fields
    dd 0, 0, 0, 0, 0
    dd 0                     ; Linear mode
    dd 1280                  ; Width
    dd 720                   ; Height
    dd 32                    ; 32 bits per pixel

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top

    ; 1. РЯТУЄМО ВАЖЛИВІ ДАНІ ОДРАЗУ!
    ; Закидаємо адресу multiboot (ebx) та магічне число (eax) на стек,
    ; де їх і чекає наша функція kernel_main.
    push ebx
    push eax

    ; 2. Вмикаємо FPU (тепер регістр eax можна безпечно використовувати)
    mov eax, cr0
    and eax, 0xFFFFFFFB      ; Скидаємо біт EM
    or eax, 0x00000002       ; Встановлюємо біт MP
    mov cr0, eax
    fninit                   ; Ініціалізуємо стан FPU

    ; 3. Запускаємо ядро (аргументи вже лежать на стеку)
    call kernel_main
.hang:
    hlt
    jmp .hang
    

global gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

global idt_flush
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

extern keyboard_handler_main
global keyboard_handler
keyboard_handler:
    pushad
    cld
    call keyboard_handler_main
    popad
    iretd

extern timer_handler_main
global timer_handler
timer_handler:
    pushad
    cld
    call timer_handler_main
    popad
    iretd

extern mouse_handler_main
global mouse_handler
mouse_handler:
    pushad
    cld
    call mouse_handler_main
    popad
    iretd

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: