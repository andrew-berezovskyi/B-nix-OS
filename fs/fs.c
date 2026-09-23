#include "fs.h"
#include "ata.h"

// ============================================================================
// [MODULE 1] PHYSICAL LAYER (Дисковий рівень: Inodes, Bitmap, Blocks)
// ============================================================================
#define FS_MAGIC 0xBEEFCAFE
#define MAX_INODES 128
#define MAX_BLOCKS 1024
#define BLOCK_SIZE 512
#define FS_START_LBA 100

typedef struct {
    uint8_t type;       // 0=Вільний, 1=Файл, 2=Папка
    uint32_t size;      // Розмір у байтах
    uint32_t blocks[8]; // Сектори на диску
} inode_t;

typedef struct {
    char name[28];
    uint32_t inode;
} dir_entry_t;

static inode_t inode_table[MAX_INODES];
static uint8_t block_bitmap[MAX_BLOCKS / 8];

static bool copy_name(char* dest, const char* src) {
    int i = 0;
    if (!dest || !src || src[0] == '\0') return false;
    while (src[i] && i < 27) { dest[i] = src[i]; i++; }
    if (src[i] != '\0') return false;
    dest[i] = '\0';
    return true;
}
static int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
static void custom_memset(void* dest, int val, int len) {
    uint8_t* ptr = (uint8_t*)dest; while (len--) *ptr++ = val;
}

static bool sync_fs(void) {
    uint8_t buffer[BLOCK_SIZE];
    custom_memset(buffer, 0, BLOCK_SIZE);
    *((uint32_t*)buffer) = FS_MAGIC;
    if (ata_write_sector(FS_START_LBA, buffer) != 0) return false;

    uint8_t* inode_ptr = (uint8_t*)inode_table;
    for (int i = 0; i < 10; i++) {
        if (ata_write_sector(FS_START_LBA + 1 + i, inode_ptr + (i * BLOCK_SIZE)) != 0) return false;
    }

    custom_memset(buffer, 0, BLOCK_SIZE);
    for(int i = 0; i < (MAX_BLOCKS / 8); i++) buffer[i] = block_bitmap[i];
    if (ata_write_sector(FS_START_LBA + 11, buffer) != 0) return false;
    return true;
}

static int alloc_block(void) {
    for (int i = 1; i < MAX_BLOCKS; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8)); 
            return i;
        }
    }
    return -1; 
}

static int find_inode(const char* name) {
    if (!name || name[0] == '\0') return -1;
    inode_t* root = &inode_table[0];
    if (root->type != FS_TYPE_DIR) return -1;
    
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];
    for (int b = 0; b < 8; b++) {
        if (root->blocks[b] == 0) continue; 
        
        if (root->blocks[b] >= MAX_BLOCKS ||
            ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries) != 0) return -1;
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int i = 0; i < count; i++) {
            if (entries[i].inode > 0 && entries[i].inode < MAX_INODES &&
                custom_strcmp(entries[i].name, name) == 0) return (int)entries[i].inode;
        }
    }
    return -1;
}

static int create_fs_node(const char* name, uint8_t type) {
    if (!name || name[0] == '\0' || find_inode(name) != -1) return -1;
    int name_len = 0;
    while (name[name_len] && name_len < 28) name_len++;
    if (name_len == 0 || name_len >= 28) return -1; 
    
    int new_ino = -1;
    for (int i = 1; i < MAX_INODES; i++) {
        if (inode_table[i].type == FS_TYPE_FREE) { new_ino = i; break; }
    }
    if (new_ino == -1) return -1; 

    inode_table[new_ino].type = type;
    inode_table[new_ino].size = 0;
    for(int i=0; i<8; i++) inode_table[new_ino].blocks[i] = 0;

    inode_t* root = &inode_table[0];
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];
    
    for (int b = 0; b < 8; b++) {
        if (root->blocks[b] == 0) {
            int block = alloc_block();
            if (block <= 0) return -1;
            root->blocks[b] = (uint32_t)block;
            custom_memset(entries, 0, sizeof(entries));
        } else if (ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries) != 0) {
            return -1;
        }
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int i = 0; i < count; i++) {
            if (entries[i].inode == 0) { 
                entries[i].inode = new_ino;
                if (!copy_name(entries[i].name, name)) return -1;
                ata_write_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries);
                sync_fs();
                return new_ino;
            }
        }
    }
    return -1;
}

// НОВЕ: Функція для видалення файлу та звільнення місця на диску
int fs_unlink(const char* path) {
    if (!path) return -1;
    while (*path == '/') path++;
    if (*path == '\0') return -1;
    inode_t* root = &inode_table[0];
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];

    for (int b = 0; b < 8; b++) {
        if (root->blocks[b] == 0) continue;
        if (root->blocks[b] >= MAX_BLOCKS ||
            ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries) != 0) return -1;
        int count = BLOCK_SIZE / sizeof(dir_entry_t);

        for (int i = 0; i < count; i++) {
            if (entries[i].inode != 0 && custom_strcmp(entries[i].name, path) == 0) {
                int target_ino = (int)entries[i].inode;
                if (target_ino <= 0 || target_ino >= MAX_INODES ||
                    inode_table[target_ino].type == FS_TYPE_DIR) return -1; // Папки поки не видаляємо цим методом
                
                // 1. Звільняємо блоки файлу
                for (int j = 0; j < 8; j++) {
                    int blk = inode_table[target_ino].blocks[j];
                    if (blk > 0 && blk < MAX_BLOCKS) block_bitmap[blk / 8] &= ~(1 << (blk % 8));
                }
                
                // 2. Звільняємо Inode
                inode_table[target_ino].type = FS_TYPE_FREE;
                
                // 3. Видаляємо запис із директорії
                entries[i].inode = 0;
                custom_memset(entries[i].name, 0, 28);
                
                ata_write_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries);
                sync_fs();
                return 0; // Успіх
            }
        }
    }
    return -1; // Файл не знайдено
}

void init_fs(void) {
    uint8_t buffer[BLOCK_SIZE];
    int superblock_ok = ata_read_sector(FS_START_LBA, buffer);

    if (superblock_ok == 0 && *((uint32_t*)buffer) == FS_MAGIC) {
        uint8_t* inode_ptr = (uint8_t*)inode_table;
        for(int i = 0; i < 10; i++) ata_read_sector(FS_START_LBA + 1 + i, inode_ptr + (i * BLOCK_SIZE));
        ata_read_sector(FS_START_LBA + 11, buffer);
        for(int i = 0; i < (MAX_BLOCKS / 8); i++) block_bitmap[i] = buffer[i];
    } else {
        custom_memset(inode_table, 0, sizeof(inode_table));
        custom_memset(block_bitmap, 0, sizeof(block_bitmap));
        
        /* Block zero is reserved because zero means "no block" on disk. */
        block_bitmap[0] |= 1U;
        inode_table[0].type = FS_TYPE_DIR;
        inode_table[0].size = 0;
        int root_block = alloc_block();
        if (root_block <= 0) return;
        inode_table[0].blocks[0] = (uint32_t)root_block;
        sync_fs();
        
        int fd = fs_open("readme.txt", 4 | 2); // O_CREAT | O_WRONLY
        if (fd != -1) {
            const char* msg = "Welcome to B-nix OS! Your new FS Layer is active.\n";
            int len = 0; while(msg[len]) len++;
            fs_write(fd, msg, len);
            fs_close(fd);
        }
        fs_mkdir("System_Logs");
    }
}

// ============================================================================
// [MODULE 2] LOGICAL LAYER (VFS, File Descriptors, Syscall Backend)
// ============================================================================
#define MAX_FDS 32
typedef struct {
    bool used;
    int inode;
    uint32_t offset;
    int flags;
} file_desc_t;

static file_desc_t fd_table[MAX_FDS];

int fs_open(const char* path, int flags) {
    if (!path) return -1;
    while (*path == '/') path++;
    if (*path == '\0') return -1; 
    int ino = find_inode(path);
    if (ino == -1) {
        if (!(flags & O_CREAT)) return -1;
        ino = create_fs_node(path, FS_TYPE_FILE);
        if (ino == -1) return -1;
    }
    if (inode_table[ino].type != FS_TYPE_FILE) return -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true; fd_table[i].inode = ino; fd_table[i].offset = 0; fd_table[i].flags = flags;
            return i;
        }
    }
    return -1;
}

int fs_read(int fd, void* buf, int size) {
    if (!buf || size < 0 || fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) return -1;
    if (size == 0) return 0;
    inode_t* file = &inode_table[fd_table[fd].inode];
    if (file->type != FS_TYPE_FILE) return -1;
    
    uint32_t bytes_read = 0; uint8_t* out_buf = (uint8_t*)buf; uint8_t sec_buf[BLOCK_SIZE];
    while (bytes_read < (uint32_t)size && fd_table[fd].offset < file->size) {
        int block_idx = fd_table[fd].offset / BLOCK_SIZE;
        int block_off = fd_table[fd].offset % BLOCK_SIZE;
        if (block_idx >= 8 || file->blocks[block_idx] == 0) break;
        
        if (file->blocks[block_idx] >= MAX_BLOCKS ||
            ata_read_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf) != 0) {
            return bytes_read == 0 ? -1 : (int)bytes_read;
        }
        int to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;
        if (to_copy > file->size - fd_table[fd].offset) to_copy = file->size - fd_table[fd].offset;
        
        for(int i=0; i<to_copy; i++) out_buf[bytes_read++] = sec_buf[block_off + i];
        fd_table[fd].offset += to_copy;
    }
    return bytes_read;
}

int fs_write(int fd, const void* buf, int size) {
    if (!buf || size < 0 || fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) return -1;
    if (size == 0) return 0;
    if (!(fd_table[fd].flags & 2)) return -1; // 2 = O_WRONLY
    
    inode_t* file = &inode_table[fd_table[fd].inode];
    uint32_t bytes_written = 0; const uint8_t* in_buf = (const uint8_t*)buf; uint8_t sec_buf[BLOCK_SIZE];
    
    while (bytes_written < (uint32_t)size) {
        int block_idx = fd_table[fd].offset / BLOCK_SIZE;
        int block_off = fd_table[fd].offset % BLOCK_SIZE;
        if (block_idx >= 8) break; 
        
        if (file->blocks[block_idx] == 0) {
            int block = alloc_block();
            if (block <= 0) break;
            file->blocks[block_idx] = (uint32_t)block;
            custom_memset(sec_buf, 0, BLOCK_SIZE);
        } else if (file->blocks[block_idx] >= MAX_BLOCKS ||
                   ata_read_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf) != 0) {
            break;
        }
        int to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_written) to_copy = size - bytes_written;
        
        for(int i=0; i<to_copy; i++) sec_buf[block_off + i] = in_buf[bytes_written++];
        if (ata_write_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf) != 0) break;
        
        fd_table[fd].offset += to_copy;
        if (fd_table[fd].offset > file->size) file->size = fd_table[fd].offset;
    }
    sync_fs();
    return bytes_written;
}

void fs_close(int fd) { if (fd >= 0 && fd < MAX_FDS) fd_table[fd].used = false; }
int fs_mkdir(const char* path) {
    if (!path) return -1;
    while (*path == '/') path++;
    return *path ? create_fs_node(path, FS_TYPE_DIR) : -1;
}

int fs_opendir(const char* path) {
    if (!path) return -1;
    while (*path == '/') path++;
    int ino = (path[0] == '\0') ? 0 : find_inode(path);
    if (ino == -1 || inode_table[ino].type != FS_TYPE_DIR) return -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) { fd_table[i].used = true; fd_table[i].inode = ino; fd_table[i].offset = 0; return i; }
    }
    return -1;
}

bool fs_readdir(int dir_fd, fs_dirent_t* out_ent) {
    if (!out_ent || dir_fd < 0 || dir_fd >= MAX_FDS || !fd_table[dir_fd].used) return false;
    inode_t* dir = &inode_table[fd_table[dir_fd].inode];
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];
    int current_logical_index = 0;
    
    for (int b = 0; b < 8; b++) {
        if (dir->blocks[b] == 0) continue;
        if (dir->blocks[b] >= MAX_BLOCKS ||
            ata_read_sector(FS_START_LBA + 12 + dir->blocks[b], (uint8_t*)entries) != 0) return false;
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        for (int i = 0; i < count; i++) {
            if (entries[i].inode > 0 && entries[i].inode < MAX_INODES) {
                if (current_logical_index == fd_table[dir_fd].offset) {
                    if (!copy_name(out_ent->name, entries[i].name)) return false;
                    out_ent->inode = entries[i].inode;
                    out_ent->type = inode_table[entries[i].inode].type;
                    fd_table[dir_fd].offset++; 
                    return true;
                }
                current_logical_index++;
            }
        }
    }
    return false; 
}
void fs_closedir(int dir_fd) { fs_close(dir_fd); }