#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_FILES 10

// Структура нашого віртуального файлу
typedef struct {
    char name[32];
    char content[256];
    uint32_t size;
    bool is_used;
} vfs_file_t;

// Масив усіх файлів у системі (наш "жорсткий диск")
extern vfs_file_t virtual_fs[MAX_FILES];

void init_vfs(void);

#endif