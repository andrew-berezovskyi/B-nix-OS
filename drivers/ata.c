#include "ata.h"
#include "io.h"

// Стандартні порти первинного (Primary) ATA-контролера
#define ATA_PORT_DATA       0x1F0
#define ATA_PORT_ERROR      0x1F1
#define ATA_PORT_SECT_COUNT 0x1F2
#define ATA_PORT_LBA_LOW    0x1F3
#define ATA_PORT_LBA_MID    0x1F4
#define ATA_PORT_LBA_HIGH   0x1F5
#define ATA_PORT_DRV_HEAD   0x1F6
#define ATA_PORT_STATUS     0x1F7
#define ATA_PORT_COMMAND    0x1F7

// Статусні біти диска
#define ATA_SR_BSY     0x80    // Busy (диск зайнятий)
#define ATA_SR_DRQ     0x08    // Data request (диск готовий віддати/прийняти дані)

// Чекаємо, поки диск перестане бути "зайнятим"
static void ata_wait_bsy(void) {
    while (inb(ATA_PORT_STATUS) & ATA_SR_BSY);
}

// Чекаємо, поки диск скаже "давай дані"
static void ata_wait_drq(void) {
    while (!(inb(ATA_PORT_STATUS) & ATA_SR_DRQ));
}

// Читання сектора з диска
void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    
    // Вибір диска (Master) та налаштування режиму LBA (біти 24-27)
    outb(ATA_PORT_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_PORT_SECT_COUNT, 1);                         // Читаємо рівно 1 сектор
    outb(ATA_PORT_LBA_LOW, (uint8_t) lba);                // Молодші 8 біт адреси
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));          // Середні 8 біт
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));        // Старші 8 біт
    outb(ATA_PORT_COMMAND, 0x20);                         // 0x20 = Команда READ SECTORS

    ata_wait_bsy();
    ata_wait_drq();

    // Читаємо 256 слів (тобто 512 байт, бо 1 слово = 2 байти)
    uint16_t* ptr = (uint16_t*) buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_PORT_DATA);
    }
}

// Запис сектора на диск
void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    
    outb(ATA_PORT_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_PORT_SECT_COUNT, 1);                         
    outb(ATA_PORT_LBA_LOW, (uint8_t) lba);                
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));          
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));        
    outb(ATA_PORT_COMMAND, 0x30);                         // 0x30 = Команда WRITE SECTORS

    ata_wait_bsy();
    ata_wait_drq();

    // Пишемо 256 слів (512 байт)
    uint16_t* ptr = (uint16_t*) buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_PORT_DATA, ptr[i]);
    }
    
    // Скидаємо кеш диска (Cache Flush), щоб дані точно записалися на бліни диска
    outb(ATA_PORT_COMMAND, 0xE7);
    ata_wait_bsy();
}