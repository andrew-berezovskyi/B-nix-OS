#include "fs.h"
#include "ata.h"

// Наш масив файлів у пам'яті
vfs_file_t virtual_fs[MAX_FILES];

// Власна функція копіювання рядків (копіюємо, поки не знайдемо \0)
static void custom_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Функція записує весь наш масив файлів на жорсткий диск
void vfs_save_to_disk(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        // Кожен файл (структура vfs_file_t) записується в окремий сектор
        // Сектор 100 - файл 0, 101 - файл 1 і так далі.
        ata_write_sector(FS_DISK_START_LBA + i, (uint8_t*)&virtual_fs[i]);
    }
}

// Функція зчитує масив файлів із жорсткого диска в оперативну пам'ять
void vfs_load_from_disk(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        ata_read_sector(FS_DISK_START_LBA + i, (uint8_t*)&virtual_fs[i]);
    }
}

void init_vfs(void) {
    // 1. Намагаємося завантажити дані, які вже були на диску
    vfs_load_from_disk();

    // 2. Перевіряємо, чи диск не "сміття" або порожній.
    // Якщо у всіх файлів прапорець is_used == false, значить ми запустилися вперше.
    bool disk_is_empty = true;
    for (int i = 0; i < MAX_FILES; i++) {
        if (virtual_fs[i].is_used) {
            disk_is_empty = false;
            break;
        }
    }

    // 3. Якщо диск порожній (перший запуск), створюємо стандартні файли
    if (disk_is_empty) {
        // Очищаємо пам'ять про всяк випадок
        for (int i = 0; i < MAX_FILES; i++) {
            virtual_fs[i].is_used = false;
        }

        // Створюємо readme.txt
        virtual_fs[0].is_used = true;
        custom_strcpy(virtual_fs[0].name, "readme.txt");
        custom_strcpy(virtual_fs[0].content, "Welcome to B-nix OS! Your files are now persistent.");
        virtual_fs[0].size = 51;

        // Створюємо системний конфіг
        virtual_fs[1].is_used = true;
        custom_strcpy(virtual_fs[1].name, "config.sys");
        custom_strcpy(virtual_fs[1].content, "OS=B-NIX\nSTORAGE=ATA_DISK");
        virtual_fs[1].size = 25;

        // Одразу зберігаємо ці дефолтні файли на диск, щоб наступного разу вони завантажились
        vfs_save_to_disk();
    }
}