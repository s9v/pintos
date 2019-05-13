#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include <bitmap.h>
#include <stdbool.h>

#define SECTORS_PER_SLOT   (PGSIZE / DISK_SECTOR_SIZE)

/* The swap device */
static struct disk *swap_device;

/* Tracks in-use and free swap slots */
static struct lock swap_lock; // Protects swap_bitmap
struct disk *swap_disk;
struct bitmap *swap_bitmap;
int swap_nslots;
/* 
 * Initialize swap_device, swap_table, and swap_lock.
 */
void 
swap_init (void)
{
	swap_disk = disk_get (1, 1);
	disk_sector_t nsectors = disk_size (swap_disk);
	swap_nslots = nsectors * DISK_SECTOR_SIZE / PGSIZE;
	swap_bitmap = bitmap_create (swap_nslots);
}

/*
 * Reclaim a frame from swap device.
 * 1. Check that the page has been already evicted. 
 * 2. You will want to evict an already existing frame
 * to make space to read from the disk to cache. 
 * 3. Re-link the new frame with the corresponding supplementary
 * page table entry. 
 * 4. Do NOT create a new supplementray page table entry. Use the 
 * already existing one. 
 * 5. Use helper function read_from_disk in order to read the contents
 * of the disk into the frame. 
 */ 
bool 
swap_in (void *frame, int slot_idx)
{
	read_from_disk (frame, slot_idx);
	bitmap_reset (swap_bitmap, slot_idx);
	
	return true;
}

/* 
 * Evict a frame to swap device. 
 * 1. Choose the frame you want to evict. 
 * (Ex. Least Recently Used policy -> Compare the timestamps when each 
 * frame is last accessed)
 * 2. Evict the frame. Unlink the frame from the supplementray page table entry
 * Remove the frame from the frame table after freeing the frame with
 * pagedir_clear_page. 
 * 3. Do NOT delete the supplementary page table entry. The process
 * should have the illusion that they still have the page allocated to
 * them. 
 * 4. Find a free block to write you data. Use swap table to get track
 * of in-use and free swap slots.
 */
int
swap_out (void *frame)
{
	int slot_idx = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
	
	if (slot_idx == BITMAP_ERROR)
		return BITMAP_ERROR;

	write_to_disk (frame, slot_idx);
	return slot_idx;
}

/* 
 * Read data from swap device to frame. 
 * Look at device/disk.c
 */
void read_from_disk (uint8_t *frame, int slot_idx)
{
	disk_sector_t sec_no = slot_idx * SECTORS_PER_SLOT;
	int sec_offset;
	for (sec_offset = 0; sec_offset < SECTORS_PER_SLOT; sec_offset++) {
		disk_read (swap_disk, sec_no + sec_offset, frame);
		frame += DISK_SECTOR_SIZE;
	}
}

/* Write data to swap device from frame */
void write_to_disk (uint8_t *frame, int slot_idx)
{
	disk_sector_t sec_no = slot_idx * SECTORS_PER_SLOT;
	int sec_offset;
	for (sec_offset = 0; sec_offset < SECTORS_PER_SLOT; sec_offset++) {
		disk_write (swap_disk, sec_no + sec_offset, frame);
		frame += DISK_SECTOR_SIZE;
	}
}

