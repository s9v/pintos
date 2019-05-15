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
	// printf ("CLOCK_FIND size(frame_list) %d\n", list_size (&frame_list));

	while (true) {
		if (clock_elem == NULL || clock_elem == list_end (&frame_list)) // either cold start or wrapped around
			clock_elem = list_begin (&frame_list);

		struct ft_entry *fte = list_entry (clock_elem, struct ft_entry, elem);
		void *upage = fte->spte->upage;
		uint32_t *pd = fte->owner->pagedir;

		if (pagedir_is_accessed (pd, upage)) // has second chance
			pagedir_set_accessed (pd, upage, false); // use second chance
		else {			
			// Prevent access to frame by syscalls
			// struct ft_entry *fte = list_entry (clock_elem, struct ft_entry, elem);
			//~ if (lock_try_acquire (&fte->spte->evict_lock)) // TODO parallelism
				break; // found frame to evict
		}

		clock_elem = list_next (clock_elem);
	}

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

	/* Uninstall page */
	uint32_t *pd = fte->owner->pagedir;
	void *upage = fte->spte->upage;
	pagedir_clear_page (pd, upage);

	/* Evict by page type */
	if (fte->spte->type == NORMAL_PAGE) {
		evict_normal_page (fte->spte);
	}
	else if (fte->spte->type == MMAP_PAGE) {
		evict_mmap_page (fte->spte);
	}
	else if (fte->spte->type == SEGMENT_PAGE) {
		// Do nothing
		// evict_segment_page (fte->spte);
	}

	fte->spte->fte = NULL;
	fte->spte = NULL;
	fte->owner = NULL;
	lock_acquire (&frame_lock);
	list_remove (&fte->elem);
	lock_release (&frame_lock);
	ASSERT (fte);
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
		struct list_elem *clock_found_e = clock_find (); // NOTE: will acquire lock for spte
		fte = list_entry (clock_found_e, struct ft_entry, elem); // TODO tell about how i forgot to remove `struct ft_entry *`
		fte = evict_frame (fte);
		// printf("fte that is not NULL %p\n", fte);
		memset (fte->frame, 0, PGSIZE);
		// printf("fte that is not NULL not MEMSETTED %p\n", fte);
		//~ lock_release (&spte->evict_lock); // TODO parallelism
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

/*void
free_frame (struct ft_entry *fte) {
	palloc_free_page (fte->frame);
	//~ free (fte);
}*/
