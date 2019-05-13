#include "vm/frame.h"
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <hash.h>
#include <list.h>
#include <stddef.h>
#include <stdbool.h>
#include <debug.h>

// struct hash frame_table;
struct list frame_list;
struct lock frame_lock;
struct list_elem *clock_elem; // iterator for FRAME_LIST

/*
 * Initialize frame table
 */
void 
frame_init (void)
{
	// hash_init (&frame_table, hash_hash_frame, hash_less_frame, NULL);
	list_init (&frame_list);
	lock_init (&frame_lock);
}

/*
 * Find frame to evict using Clock Algorithm
 */
struct list_elem *clock_evict (void) {
	while (true) {
		if (clock_elem == NULL) // either cold start or wrapped around
			clock_elem = list_begin (&frame_list);

		struct frame_table_entry *fte = list_entry (clock_elem, struct frame_table_entry, elem);
		void *upage = fte->spte->upage;
		uint32_t *pd = fte->owner->pagedir;

		if (pagedir_is_accessed (pd, upage)) // has second chance
			pagedir_set_accessed (pd, upage, false); // use second chance
		else
			break; // found frame to evict

		clock_elem = list_next (clock_elem);
	}

	struct list_elem *old_clock_elem = clock_elem;
	clock_elem = list_next (clock_elem);

	return old_clock_elem;
}

/*
 * Evict a frame
 */
void *evict_frame (void) {
	struct list_elem *old_clock_elem = clock_evict ();
	
	/* Remove page from pagedir */
	struct frame_table_entry *fte = list_entry (old_clock_elem, struct frame_table_entry, elem);
	void *upage = fte->spte->upage;
	uint32_t *pd = fte->owner->pagedir;
	pagedir_clear_page (pd, upage);

	/* Remove frame table entry from list */
	list_remove (old_clock_elem);

	/* Swap the frame out */
	void *frame = fte->frame;

	if (fte->spte->type == NORMAL_PAGE) {
		int slot_idx = swap_out (frame);
		if (slot_idx == BITMAP_ERROR)
			return NULL;

		fte->spte->slot_idx = slot_idx;
	}
	else if (fte->spte->type == MMAP_PAGE) {
		mapid_t mapid = fte->spte->mapid;
		write_to_file (mapid, offset, frame);
	}

	free (fte);
	return frame;
}

/* 
 * Make a new frame table entry for SPTE.
 */
void *
allocate_frame (struct spt_entry *spte)
{
	ASSERT (spte != NULL);

	struct thread *t = thread_current ();
	void* upage = spte->upage;
	void* kpage;

	/* Find free frame */
	if ((kpage = palloc_get_page (PAL_USER | PAL_ZERO)) == NULL) {
		// TODO: evict a frame to disk, and reuse the frame
		kpage = evict_frame ();

		if (kpage == NULL)
			return NULL;
	}

	/* Map upage to kpage in pagedir */
	ASSERT (pagedir_get_page (t->pagedir, upage) == NULL);

	if (!pagedir_set_page (t->pagedir, upage, kpage, spte->writable)) {
		palloc_free_page (kpage);
		return NULL;
	}

	/* Add new frame to frame_list */
	struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
	if (fte == NULL) {
		pagedir_clear_page (t->pagedir, upage);
		palloc_free_page (kpage);
		return NULL;
	}

	fte->frame = kpage;
	fte->owner = t;
	fte->spte = spte;

	lock_acquire (&frame_lock);
	list_push_back (&frame_list, &fte->elem);
	lock_release (&frame_lock);

	return kpage;
}

