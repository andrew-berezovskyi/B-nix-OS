#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// Магічні числа, які кажуть, що це дійсно ELF-файл (0x7F, 'E', 'L', 'F')
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// Головний заголовок ELF-файлу (завжди на самому початку файлу)
typedef struct {
    uint8_t  e_ident[16]; // Магічні числа та інфо про розрядність
    uint16_t e_type;      // Тип (1 - Relocatable, 2 - Executable)
    uint16_t e_machine;   // Архітектура (3 - x86)
    uint32_t e_version;   // Версія
    uint32_t e_entry;     // 🔥 ТОЧКА ВХОДУ! (Звідси процесор почне виконувати код)
    uint32_t e_phoff;     // Зміщення до таблиці заголовків програм
    uint32_t e_shoff;     // Зміщення до таблиці секцій
    uint32_t e_flags;
    uint16_t e_ehsize;    // Розмір цього заголовка
    uint16_t e_phentsize; // Розмір одного запису в таблиці програм
    uint16_t e_phnum;     // Кількість записів у таблиці програм
    uint16_t e_shentsize; 
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf32_ehdr_t;

// Типи сегментів
#define PT_NULL    0
#define PT_LOAD    1 // Цей сегмент треба завантажити в пам'ять!
#define PT_DYNAMIC 2

// Заголовок програми (Program Header) - описує шматок файлу, який треба завантажити
typedef struct {
    uint32_t p_type;   // Тип сегмента (нас цікавить PT_LOAD)
    uint32_t p_offset; // Де цей шматок лежить у файлі
    uint32_t p_vaddr;  // Куди його треба покласти у віртуальній пам'яті
    uint32_t p_paddr;  // Фізична адреса (ігноруємо)
    uint32_t p_filesz; // Скільки байт він займає у файлі
    uint32_t p_memsz;  // Скільки байт він має займати в пам'яті (може бути більшим за filesz)
    uint32_t p_flags;  // Права доступу (Read, Write, Execute)
    uint32_t p_align;  // Вирівнювання
} elf32_phdr_t;

#endif