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
 * Make a new frame table entry for addr.
 */
void *
allocate_frame (void *addr, bool writable)
{
	struct thread *t = thread_current ();
	void* upage = pg_round_down (addr);
	void* kpage;
	struct sup_page_table_entry* spte;

	if ((kpage = palloc_get_page (PAL_USER | PAL_ZERO)) == NULL) {
		// TODO: evict a frame to disk, and reuse the frame
		return NULL;
	}

	ASSERT (pagedir_get_page (t->pagedir, upage) == NULL);

	if (!pagedir_set_page (t->pagedir, upage, kpage, writable)) {
		palloc_free_page (kpage);
		return NULL;
	}

	void *kpage_ = pagedir_get_page (t->pagedir, upage);

	if ((spte = allocate_page (addr)) == NULL) {
		pagedir_clear_page (t->pagedir, upage);
		palloc_free_page (kpage);
		return NULL;
	}

	struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));

	if (fte == NULL) {
		// TODO: not so important, but
		pagedir_clear_page (t->pagedir, upage);
		palloc_free_page (kpage);
		return NULL;
	}

	fte->frame = kpage; // TODO is this correct???????????
	fte->owner = t;
	fte->spte = spte;

	lock_acquire (&frame_lock);
	list_push_back (&frame_list, &fte->elem);
	lock_release (&frame_lock);

	return kpage;
}

