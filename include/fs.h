#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>

// Прапорці для відкриття файлів
#define O_RDONLY 1
#define O_WRONLY 2
#define O_CREAT  4

// Типи Inode
#define FS_TYPE_FREE 0
#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

// Структура для читання папок (Directory Entry)
typedef struct {
    char name[28];
    uint32_t inode;
    uint8_t type;
} fs_dirent_t;

// Ініціалізація Файлової Системи
void init_fs(void); 

// ==========================================
// CORE API (Робота з файлами через Descriptors)
// ==========================================
int fs_open(const char* path, int flags);
int fs_read(int fd, void* buf, int size);
int fs_write(int fd, const void* buf, int size);
void fs_close(int fd);

// ==========================================
// DIRECTORY API (Навігація по папках)
// ==========================================
int fs_mkdir(const char* path);
int fs_opendir(const char* path);
bool fs_readdir(int dir_fd, fs_dirent_t* out_ent);
void fs_closedir(int dir_fd);

int fs_unlink(const char* path);

#endif