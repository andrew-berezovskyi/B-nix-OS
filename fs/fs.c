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

static void custom_strcpy(char* dest, const char* src) { while (*src) *dest++ = *src++; *dest = '\0'; }
static int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
static void custom_memset(void* dest, int val, int len) {
    uint8_t* ptr = (uint8_t*)dest; while (len--) *ptr++ = val;
}

static void sync_fs(void) {
    uint8_t buffer[BLOCK_SIZE];
    custom_memset(buffer, 0, BLOCK_SIZE);
    *((uint32_t*)buffer) = FS_MAGIC;
    ata_write_sector(FS_START_LBA, buffer);

    uint8_t* inode_ptr = (uint8_t*)inode_table;
    for(int i = 0; i < 10; i++) ata_write_sector(FS_START_LBA + 1 + i, inode_ptr + (i * BLOCK_SIZE));

    custom_memset(buffer, 0, BLOCK_SIZE);
    for(int i = 0; i < (MAX_BLOCKS / 8); i++) buffer[i] = block_bitmap[i];
    ata_write_sector(FS_START_LBA + 11, buffer);
}

static int alloc_block(void) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8)); 
            return i;
        }
    }
    return -1; 
}

static int find_inode(const char* name) {
    inode_t* root = &inode_table[0];
    if (root->type != FS_TYPE_DIR) return -1;
    
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];
    for (int b = 0; b < 8; b++) {
        if (root->blocks[b] == 0) continue; 
        
        ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries);
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int i = 0; i < count; i++) {
            if (entries[i].inode != 0 && custom_strcmp(entries[i].name, name) == 0) return entries[i].inode;
        }
    }
    return -1;
}

static int create_fs_node(const char* name, uint8_t type) {
    if (find_inode(name) != -1) return -1; 
    
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
        if (root->blocks[b] == 0) root->blocks[b] = alloc_block(); 
        
        ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries);
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int i = 0; i < count; i++) {
            if (entries[i].inode == 0) { 
                entries[i].inode = new_ino;
                custom_strcpy(entries[i].name, name);
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
    while(*path == '/') path++;
    inode_t* root = &inode_table[0];
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];

    for (int b = 0; b < 8; b++) {
        if (root->blocks[b] == 0) continue;
        ata_read_sector(FS_START_LBA + 12 + root->blocks[b], (uint8_t*)entries);
        int count = BLOCK_SIZE / sizeof(dir_entry_t);

        for (int i = 0; i < count; i++) {
            if (entries[i].inode != 0 && custom_strcmp(entries[i].name, path) == 0) {
                int target_ino = entries[i].inode;
                if (inode_table[target_ino].type == FS_TYPE_DIR) return -1; // Папки поки не видаляємо цим методом
                
                // 1. Звільняємо блоки файлу
                for (int j = 0; j < 8; j++) {
                    int blk = inode_table[target_ino].blocks[j];
                    if (blk != 0) block_bitmap[blk / 8] &= ~(1 << (blk % 8)); // Забираємо 1 з Bitmap
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
    ata_read_sector(FS_START_LBA, buffer);
    
    if (*((uint32_t*)buffer) == FS_MAGIC) {
        uint8_t* inode_ptr = (uint8_t*)inode_table;
        for(int i = 0; i < 10; i++) ata_read_sector(FS_START_LBA + 1 + i, inode_ptr + (i * BLOCK_SIZE));
        ata_read_sector(FS_START_LBA + 11, buffer);
        for(int i = 0; i < (MAX_BLOCKS / 8); i++) block_bitmap[i] = buffer[i];
    } else {
        custom_memset(inode_table, 0, sizeof(inode_table));
        custom_memset(block_bitmap, 0, sizeof(block_bitmap));
        
        inode_table[0].type = FS_TYPE_DIR;
        inode_table[0].size = 0;
        inode_table[0].blocks[0] = alloc_block();
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
    while(*path == '/') path++; 
    int ino = find_inode(path);
    if (ino == -1) {
        if (!(flags & 4)) return -1; // 4 = O_CREAT
        ino = create_fs_node(path, FS_TYPE_FILE);
        if (ino == -1) return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true; fd_table[i].inode = ino; fd_table[i].offset = 0; fd_table[i].flags = flags;
            return i;
        }
    }
    return -1;
}

int fs_read(int fd, void* buf, int size) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) return -1;
    inode_t* file = &inode_table[fd_table[fd].inode];
    if (file->type != FS_TYPE_FILE) return -1;
    
    uint32_t bytes_read = 0; uint8_t* out_buf = (uint8_t*)buf; uint8_t sec_buf[BLOCK_SIZE];
    while (bytes_read < (uint32_t)size && fd_table[fd].offset < file->size) {
        int block_idx = fd_table[fd].offset / BLOCK_SIZE;
        int block_off = fd_table[fd].offset % BLOCK_SIZE;
        if (block_idx >= 8 || file->blocks[block_idx] == 0) break;
        
        ata_read_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf);
        int to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;
        if (to_copy > file->size - fd_table[fd].offset) to_copy = file->size - fd_table[fd].offset;
        
        for(int i=0; i<to_copy; i++) out_buf[bytes_read++] = sec_buf[block_off + i];
        fd_table[fd].offset += to_copy;
    }
    return bytes_read;
}

int fs_write(int fd, const void* buf, int size) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].used) return -1;
    if (!(fd_table[fd].flags & 2)) return -1; // 2 = O_WRONLY
    
    inode_t* file = &inode_table[fd_table[fd].inode];
    uint32_t bytes_written = 0; const uint8_t* in_buf = (const uint8_t*)buf; uint8_t sec_buf[BLOCK_SIZE];
    
    while (bytes_written < (uint32_t)size) {
        int block_idx = fd_table[fd].offset / BLOCK_SIZE;
        int block_off = fd_table[fd].offset % BLOCK_SIZE;
        if (block_idx >= 8) break; 
        
        if (file->blocks[block_idx] == 0) {
            file->blocks[block_idx] = alloc_block();
            if (file->blocks[block_idx] == 0) break; 
        }
        
        ata_read_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf);
        int to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_written) to_copy = size - bytes_written;
        
        for(int i=0; i<to_copy; i++) sec_buf[block_off + i] = in_buf[bytes_written++];
        ata_write_sector(FS_START_LBA + 12 + file->blocks[block_idx], sec_buf);
        
        fd_table[fd].offset += to_copy;
        if (fd_table[fd].offset > file->size) file->size = fd_table[fd].offset;
    }
    sync_fs();
    return bytes_written;
}

void fs_close(int fd) { if (fd >= 0 && fd < MAX_FDS) fd_table[fd].used = false; }
int fs_mkdir(const char* path) { while(*path == '/') path++; return create_fs_node(path, FS_TYPE_DIR); }

int fs_opendir(const char* path) {
    while(*path == '/') path++;
    int ino = (path[0] == '\0') ? 0 : find_inode(path);
    if (ino == -1 || inode_table[ino].type != FS_TYPE_DIR) return -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) { fd_table[i].used = true; fd_table[i].inode = ino; fd_table[i].offset = 0; return i; }
    }
    return -1;
}

bool fs_readdir(int dir_fd, fs_dirent_t* out_ent) {
    if (dir_fd < 0 || dir_fd >= MAX_FDS || !fd_table[dir_fd].used) return false;
    inode_t* dir = &inode_table[fd_table[dir_fd].inode];
    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)];
    int current_logical_index = 0;
    
    for (int b = 0; b < 8; b++) {
        if (dir->blocks[b] == 0) continue;
        ata_read_sector(FS_START_LBA + 12 + dir->blocks[b], (uint8_t*)entries);
        int count = BLOCK_SIZE / sizeof(dir_entry_t);
        for (int i = 0; i < count; i++) {
            if (entries[i].inode != 0) {
                if (current_logical_index == fd_table[dir_fd].offset) {
                    custom_strcpy(out_ent->name, entries[i].name);
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