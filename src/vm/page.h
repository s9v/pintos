#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/mmap.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <hash.h>

enum page_type { NORMAL_PAGE, SEGMENT_PAGE, MMAP_PAGE };

// Supplemental page table entry
struct spt_entry 
{
	void *upage;
	bool writable;
	struct ft_entry *fte;
	struct hash_elem elem_hash;
	struct list_elem elem;
	struct lock evict_lock;

	/* Type specific attributes */
	enum page_type type;
	// type = SEGMENT_PAGE
	struct program_segment *ps;
	// type = MMAP_PAGE
	struct file_mapping *fmap;
	off_t offset;
	// type = NORMAL_PAGE
	int slot_idx;
};

/* Helpers */
struct spt_entry *hash_get_spte (void *upage);
/* end of Helpers */

void page_init (struct thread *t);
void load_page (struct spt_entry *spte);
void free_page (struct spt_entry *spte);
struct spt_entry *allocate_page (void *addr, bool writable);
struct spt_entry *allocate_normal_page (void *upage);
void load_normal_page (struct spt_entry *spte);
void evict_normal_page (struct spt_entry *spte);
void free_normal_page (struct spt_entry *spte);

#endif /* vm/page.h */
