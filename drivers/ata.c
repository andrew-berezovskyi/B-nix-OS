#include "ata.h"
#include "io.h"

#define ATA_DATA       0x1F0
#define ATA_SECT_COUNT 0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF  0x20
#define ATA_SR_BSY 0x80
#define ATA_TIMEOUT 1000000U

static int ata_wait(int require_drq) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; ++i) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0x00 || status == 0xFF) continue;
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
        if (!(status & ATA_SR_BSY) && (!require_drq || (status & ATA_SR_DRQ))) return 0;
    }
    return -1;
}

static void ata_select(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0U | ((lba >> 24) & 0x0FU)));
    outb(ATA_SECT_COUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!buffer || ata_wait(0) != 0) return -1;
    ata_select(lba);
    outb(ATA_COMMAND, 0x20);
    if (ata_wait(1) != 0) return -1;
    uint16_t* words = (uint16_t*)buffer;
    for (uint32_t i = 0; i < 256; ++i) words[i] = inw(ATA_DATA);
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!buffer || ata_wait(0) != 0) return -1;
    ata_select(lba);
    outb(ATA_COMMAND, 0x30);
    if (ata_wait(1) != 0) return -1;
    const uint16_t* words = (const uint16_t*)buffer;
    for (uint32_t i = 0; i < 256; ++i) outw(ATA_DATA, words[i]);
    outb(ATA_COMMAND, 0xE7);
    return ata_wait(0);
}
