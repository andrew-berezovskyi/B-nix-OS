#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_FILES 10
#define FS_DISK_START_LBA 100 // Почнемо записувати файли з 100-го сектора, щоб не зачепити ядро

typedef struct {
    char name[32];
    char content[256];
    uint32_t size;
    bool is_used;
} vfs_file_t;

extern vfs_file_t virtual_fs[MAX_FILES];

void init_vfs(void);
void vfs_save_to_disk(void); // Записати масив на диск
void vfs_load_from_disk(void); // Прочитати масив з диска

#endif