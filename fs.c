#include "fs.h"

vfs_file_t virtual_fs[MAX_FILES];

// Власна функція копіювання рядків
static void custom_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void init_vfs(void) {
    // Очищаємо всі комірки
    for (int i = 0; i < MAX_FILES; i++) {
        virtual_fs[i].is_used = false;
    }

    // Створюємо Файл 1
    virtual_fs[0].is_used = true;
    custom_strcpy(virtual_fs[0].name, "readme.txt");
    custom_strcpy(virtual_fs[0].content, "Welcome to B-nix OS! This is your first file.");
    virtual_fs[0].size = 45;

    // Створюємо Файл 2
    virtual_fs[1].is_used = true;
    custom_strcpy(virtual_fs[1].name, "config.sys");
    custom_strcpy(virtual_fs[1].content, "OS=B-NIX\nGUI=ENABLED\nVERSION=0.1");
    virtual_fs[1].size = 32;

    // Створюємо Файл 3
    virtual_fs[2].is_used = true;
    custom_strcpy(virtual_fs[2].name, "secret.log");
    custom_strcpy(virtual_fs[2].content, "You found the secret log...");
    virtual_fs[2].size = 27;
}