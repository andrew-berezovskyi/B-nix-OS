#ifndef SYS_API_H
#define SYS_API_H

#include <stdint.h>

// Ці функції програми викликатимуть замість fs_open, fs_read тощо
int sys_open(const char* path, int flags);
int sys_read(int fd, void* buf, int size);
int sys_write(int fd, const void* buf, int size);
void sys_close(int fd);

int sys_unlink(const char* path);

#endif  