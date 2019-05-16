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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <debug.h>

// struct hash frame_table;
struct list frame_list;
struct lock frame_lock;
struct list_elem *clock_elem; // iterator for FRAME_LIST

struct list_elem *clock_find (void);

/*
 * Initialize frame table
 */
void 
frame_init (void)
{
	clock_elem = NULL;
	// hash_init (&frame_table, hash_hash_frame, hash_less_frame, NULL);
	list_init (&frame_list);
	lock_init (&frame_lock);
}

/*
 * Find frame to evict from FRAME_LIST using Clock Algorithm
 */
struct list_elem *
clock_find (void) {
	// printf ("clock_find START\n");

	while (true) {
		if (clock_elem == NULL || clock_elem == list_end (&frame_list)) // either cold start or wrapped around
			clock_elem = list_begin (&frame_list);
		
		struct ft_entry *fte = list_entry (clock_elem, struct ft_entry, elem);
		
		if (fte->spte != NULL && lock_try_acquire (&fte->spte->evict_lock)/* && fte->owner->pagedir != NULL*/) {
			void *upage = fte->spte->upage;
			uint32_t *pd = fte->owner->pagedir;

			if (pagedir_is_accessed (pd, upage)) { // Has second chance
				pagedir_set_accessed (pd, upage, false);
				lock_release (&fte->spte->evict_lock);
			}
			else {
					break; // Found frame to evict!
			}
		}
		
		clock_elem = list_next (clock_elem);
	}

	// printf ("clock_find END\n");

	struct list_elem *old_clock_elem = clock_elem;
	clock_elem = list_next (clock_elem);

	return old_clock_elem;
}

/*
 * Evict a frame, do necessary cleanup and return the evicted frame.
 */
struct ft_entry *
evict_frame (struct ft_entry *fte) {
	ASSERT (fte != NULL);
	ASSERT (fte->owner != NULL);
	ASSERT (fte->spte != NULL);

	struct spt_entry *spte = fte->spte;

	/* Uninstall page */
	uint32_t *pd = fte->owner->pagedir;
	void *upage = spte->upage;
	pagedir_clear_page (pd, upage);

	/* Evict by page type */
	if (spte->type == NORMAL_PAGE) {
		evict_normal_page (spte);
	}
	else if (spte->type == MMAP_PAGE) {
		evict_mmap_page (spte);
	}
	else if (spte->type == SEGMENT_PAGE) {
		// Do nothing
		// evict_segment_page (fte->spte);
	}

	spte->fte = NULL;
	fte->spte = NULL;
	fte->owner = NULL;

	lock_release (&spte->evict_lock); // ======= allow loading of page

	return fte;
}

/* 
 * Create room (frame) for virtual page corresponding to SPTE
 */
struct ft_entry *
allocate_frame (void)
{
	void* kpage;

	/* Find free frame */
	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	struct ft_entry *fte;

	if (kpage != NULL) {
		fte = malloc (sizeof (struct ft_entry));
		if (fte == NULL)
			PANIC ("malloc (ft_entry) failed");
		fte->frame = kpage;
		ASSERT (fte != NULL);
	}
	else {
		/* Find a frame to evict */
		lock_acquire (&frame_lock);
		struct list_elem *clock_found_e = clock_find (); // NOTE: will acquire lock for spte
		fte = list_entry (clock_found_e, struct ft_entry, elem); // TODO tell about how i forgot to remove `struct ft_entry *`
		list_remove (&fte->elem);
		lock_release (&frame_lock);

		fte = evict_frame (fte);

		memset (fte->frame, 0, PGSIZE);
	}

	// printf("fte that is NULL %p\n", fte);
	ASSERT (fte != NULL);
	fte->owner = thread_current ();
	// printf("fte->owner %p\n", fte->owner);
	fte->spte = NULL;

	lock_acquire (&frame_lock);
	list_push_back (&frame_list, &fte->elem);
	lock_release (&frame_lock);

	return fte;
}

void
free_frame (struct ft_entry *fte) {
	ASSERT (fte != NULL);

	lock_acquire (&frame_lock);
	list_remove (&fte->elem);
	lock_release (&frame_lock);

	palloc_free_page (fte->frame);
	free (fte);
}