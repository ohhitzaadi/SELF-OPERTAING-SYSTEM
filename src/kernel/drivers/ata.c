#include "ata.h"
#include "io.h"

extern void kprint(const char *str);

void ata_read_sector(uint32_t lba, uint8_t *buf) {
  // Select drive (LBA mode, primary master)
  outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
  // Sector count (1 sector)
  outb(0x1F2, 1);
  // LBA registers
  outb(0x1F3, (uint8_t)lba);
  outb(0x1F4, (uint8_t)(lba >> 8));
  outb(0x1F5, (uint8_t)(lba >> 16));
  // Command: READ SECTOR (0x20)
  outb(0x1F7, 0x20);

  // Wait for BSY to clear and DRQ to set
  while ((inb(0x1F7) & 0x88) != 0x08);

  // Read 256 words (512 bytes)
  uint16_t *wbuf = (uint16_t *)buf;
  for (int i = 0; i < 256; i++) {
    wbuf[i] = inw(0x1F0);
  }
}

void ata_write_sector(uint32_t lba, const uint8_t *buf) {
  // Select drive (LBA mode, primary master)
  outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
  // Sector count (1 sector)
  outb(0x1F2, 1);
  // LBA registers
  outb(0x1F3, (uint8_t)lba);
  outb(0x1F4, (uint8_t)(lba >> 8));
  outb(0x1F5, (uint8_t)(lba >> 16));
  // Command: WRITE SECTOR (0x30)
  outb(0x1F7, 0x30);

  // Wait for BSY to clear and DRQ to set
  while ((inb(0x1F7) & 0x88) != 0x08);

  // Write 256 words (512 bytes)
  const uint16_t *wbuf = (const uint16_t *)buf;
  for (int i = 0; i < 256; i++) {
    outw(0x1F0, wbuf[i]);
  }

  // Flush cache
  outb(0x1F7, 0xE7);
  while (inb(0x1F7) & 0x80);
}

void ata_init() {
  kprint("[ATA] Initializing IDE/ATA Primary Master controller...\n");
  // Basic check for drive presence
  outb(0x1F2, 0x55);
  uint8_t sc = inb(0x1F2);
  if (sc == 0x55) {
    kprint("[ATA] Primary Master Drive detected and responding.\n");
  } else {
    kprint("[ATA] Warning: Primary Master Drive not responding.\n");
  }
}
