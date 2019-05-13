#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/mmap.h"
#include <stdint.h>
#include <stdbool.h>
#include <hash.h>

enum page_type { NORMAL_PAGE, SEGMENT_PAGE, MMAP_PAGE };

// Supplemental page table entry
struct spt_entry 
{
	void *upage;
	struct ft_entry *fte;
	struct hash elem_hash;
	struct lock evict_lock;

	/* Eviction */
	// bool is_evictable = false;
	// struct lock evict_lock;
	enum page_type type;

	/* SEGMENT_PAGE pages */
	struct program_segment *ps; /* program segment the page belongs to */
	
	/* MMAP_PAGE pages */
	struct file_mapping *fmap;
	off_t offset; /* file offset */

	/* NORMAL_PAGE page */
	int slot_idx; /* slot index */
};

void page_init (void);
void load_page (spt_entry *spte);
struct spt_entry *allocate_page (void *addr, bool writable);

#endif /* vm/page.h */
