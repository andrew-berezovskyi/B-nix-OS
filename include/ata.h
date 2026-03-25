#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Один сектор на жорсткому диску завжди дорівнює 512 байт
#define ATA_SECTOR_SIZE 512

// Прочитати 1 сектор (512 байт) за адресою lba у buffer
void ata_read_sector(uint32_t lba, uint8_t* buffer);

// Записати 1 сектор (512 байт) із buffer за адресою lba
void ata_write_sector(uint32_t lba, uint8_t* buffer);

#endif