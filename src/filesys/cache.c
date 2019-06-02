#include "devices/disk.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cache.h"

struct cache_item {
  struct disk *disk;
  disk_sector_t sector;
  bool dirty;
  int64_t access_time;
  uint8_t data[DISK_SECTOR_SIZE];
};

struct lock cache_lock;

// struct cache_item cache[CACHE_ITEMS]; // TODO: malloc instead, if binary size is too big
struct cache_item *cache; // TODO: malloc instead, if binary size is too big


int cache_lookup (struct disk *disk, disk_sector_t sector);
int cache_evict (void);
int cache_load (struct disk *disk, disk_sector_t sector);
void cache_discard (int idx);


void cache_init (void) {
  cache = (struct cache_item *) calloc (CACHE_ITEMS, sizeof (struct cache_item));
  lock_init (&cache_lock);

  int i;
  for (i = 0; i < CACHE_ITEMS; i++)
    cache[i].sector = (disk_sector_t) -1;
}

void cache_read (struct disk *disk, disk_sector_t sector, void *buffer) {
  lock_acquire (&cache_lock);
  // printf ("CACHE READ  <<<===   disk: %p   sector: %d\n", disk, sector);
  int idx = cache_lookup (disk, sector);

  if (idx == -1)
    idx = cache_load (disk, sector);

  memcpy (buffer, cache[idx].data, DISK_SECTOR_SIZE);
  cache[idx].access_time = timer_ticks ();
  lock_release (&cache_lock);
}

void cache_write (struct disk *disk, disk_sector_t sector, const void *buffer) {
  lock_acquire (&cache_lock);
  // printf ("CACHE WRITE ===>>>   disk: %p   sector: %d\n", disk, sector);
  int idx = cache_lookup (disk, sector);

  if (idx == -1)
    idx = cache_load (disk, sector);

  memcpy (cache[idx].data, buffer, DISK_SECTOR_SIZE);
  cache[idx].dirty = true;
  cache[idx].access_time = timer_ticks ();
  lock_release (&cache_lock);
}

void cache_flush (void) {
  int i;
  for (i = 0; i < CACHE_ITEMS; i++)
    cache_discard (i);

  free (cache);
}

int cache_lookup (struct disk *disk, disk_sector_t sector) {
  int i;
  for (i = 0; i < CACHE_ITEMS; i++)
    if (cache[i].disk == disk && cache[i].sector == sector)
      return i;
  return -1;
}

int cache_evict (void) {
  int i;
  for (i = 0; i < CACHE_ITEMS; i++)
    if (cache[i].sector == (disk_sector_t) -1)
      return i;

  int idx = -1;
  for (i = 0; i < CACHE_ITEMS; i++)
    if (idx == -1 || cache[i].access_time < cache[idx].access_time)
      idx = i;

  cache_discard (idx);
  return idx;
}

int cache_load (struct disk *disk, disk_sector_t sector) {
  int idx = cache_evict ();
  struct cache_item *item = &cache[idx];

  item->dirty = false;
  item->access_time = timer_ticks ();
  item->disk = disk;
  item->sector = sector;
  disk_read (disk, sector, item->data);

  return idx;
}

void cache_discard (int idx) {
  struct cache_item *item = &cache[idx];

  if (item->dirty)
    disk_write (item->disk, item->sector, item->data);

  item->sector = (disk_sector_t) -1;
}