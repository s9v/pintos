#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#define CACHE_ITEMS 64

void cache_init (void); // TODO: call it from filesys_init()!
void cache_read (struct disk *disk, disk_sector_t sector, void *buffer);
void cache_write (struct disk *disk, disk_sector_t sector, const void *buffer);
void cache_flush (void);

/* later */
// void cache_wb (void); // write-back

#endif /* filesys/cache.h */
