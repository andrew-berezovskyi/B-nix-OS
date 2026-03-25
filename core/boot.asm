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

    push ebx
    push eax

    ; Вмикаємо FPU
    mov eax, cr0
    and eax, 0xFFFFFFFB
    or eax, 0x00000002
    mov cr0, eax
    fninit

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

extern mouse_handler_main
global mouse_handler
mouse_handler:
    pushad
    cld
    call mouse_handler_main
    popad
    iretd

; ==============================================================
; ОНОВЛЕНО: ОБРОБНИК ТАЙМЕРА (ПЛАНУВАЛЬНИК)
; ==============================================================
extern timer_handler_main
global timer_handler
timer_handler:
    pushad                  ; Зберігаємо регістри поточної задачі
    
    push esp                ; Передаємо вказівник на context_t (поточний ESP)
    call timer_handler_main ; Викликаємо планувальник
    mov esp, eax            ; МАГІЯ: Перемикаємо ESP на стек нової задачі!
    
    ; Тепер ми на стеку нової задачі, відновлюємо її регістри
    popad
    iretd

; ==============================================================
; СИСТЕМНІ ВИКЛИКИ (INT 0x80)
; ==============================================================
extern syscall_handler_main
global syscall_handler
syscall_handler:
    pushad
    push edx
    push ecx
    push ebx
    push eax
    call syscall_handler_main
    add esp, 16
    mov [esp + 28], eax 
    popad
    iretd

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: