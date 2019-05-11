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

unsigned hash_hash_frame (const struct hash_elem *e, void *aux UNUSED);
bool hash_less_frame (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

unsigned hash_hash_frame (const struct hash_elem *e, void *aux UNUSED) {
	struct frame_table_entry *fte = hash_entry (e, struct frame_table_entry, elem_hash);

	return hash_int((int)fte->frame);
}

bool hash_less_frame (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct frame_table_entry *fte1 = hash_entry (a, struct frame_table_entry, elem_hash);
	struct frame_table_entry *fte2 = hash_entry (b, struct frame_table_entry, elem_hash);
	return fte1->frame < fte2->frame;
}

// struct hash frame_table;
struct list frame_list;

/*
 * Initialize frame table
 */
void 
frame_init (void)
{
	// hash_init (&frame_table, hash_hash_frame, hash_less_frame, NULL);
	list_init (&frame_list);
}


/* 
 * Make a new frame table entry for addr.
 */
bool
allocate_frame (void *addr)
{
	struct thread *t = thread_current ();
	void* upage = pg_round_up (addr);
	void* kpage;
	struct sup_page_table_entry* spte;

	if ((kpage = palloc_get_page (PAL_USER | PAL_ZERO)) == NULL) {
		// TODO: evict a frame to disk, and reuse the frame
		return false;
	}

	bool writable = true; // TODO: maybe false?

	ASSERT (pagedir_get_page (t->pagedir, upage) == NULL);
	
	if (!pagedir_set_page (t->pagedir, upage, kpage, writable)) {
		palloc_free_page (kpage);
		return false;
	}

	if ((spte = allocate_page (addr)) == NULL) {
		pagedir_clear_page (t->pagedir, upage);
		palloc_free_page (kpage);
		return false;
	}

	struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));

	if (fte == NULL) {
		// TODO: not so important, but
		pagedir_clear_page (t->pagedir, upage);
		palloc_free_page (kpage);
		return false;
	}

	fte->frame = kpage; // TODO is this correct???????????
	fte->owner = t;
	fte->spte = spte;

	list_push_back (&frame_list, &fte->elem);

	return true;
}

