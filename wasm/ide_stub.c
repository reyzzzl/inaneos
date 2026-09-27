#include "ide.h"

extern int disk_read(unsigned long lba, unsigned char *dst);   // 512B, 0 ok
extern int disk_write(unsigned long lba, const unsigned char *src); // 512B, 0 ok
extern unsigned long disk_sectors(void); // no disk

int ide_init(void) { return 0; }

int ide_read(uint64_t lba, void *buf) {
  if (lba >= disk_sectors())
    return -1;
  return disk_read((unsigned long)lba, (unsigned char *)buf);
}

int ide_write(uint64_t lba, const void *buf) {
  if (lba >= disk_sectors())
    return -1;
  return disk_write((unsigned long)lba, (const unsigned char *)buf);
}

uint64_t ide_sectors(void) { return disk_sectors(); }
