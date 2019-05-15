#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/mmap.h"
#include "userprog/pagedir.h"
#include <stddef.h>
#include <hash.h>
#include <bitmap.h>
#include <stdio.h>
#include <debug.h>
#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"

/* === Helper functions === */

/*
 * Get spt_entry by 
 */
struct spt_entry *
hash_get_spte (void *upage) {
  struct thread *t = thread_current ();

  struct spt_entry dummy_spte;
  dummy_spte.upage = upage;
  struct hash_elem *he = hash_find (&t->sp_table, &dummy_spte.elem_hash);

  if (he == NULL)
    return NULL;

  struct spt_entry *spte = hash_entry (he, struct spt_entry, elem_hash);
  return spte;
}

unsigned hash_hash_spte (const struct hash_elem *e, void *aux UNUSED);
bool hash_less_spte (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

unsigned hash_hash_spte (const struct hash_elem *e, void *aux UNUSED) {
    struct spt_entry *spte = hash_entry (e, struct spt_entry, elem_hash);
    return hash_int ((int)spte->upage);
}

bool hash_less_spte (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct spt_entry *spte1 = hash_entry (a, struct spt_entry, elem_hash);
    struct spt_entry *spte2 = hash_entry (b, struct spt_entry, elem_hash);
    return spte1->upage < spte2->upage;
}

/* === end of Helper functions === */

/*
 * Initialize supplementary page table
 */
void 
page_init (struct thread *t)
{
  hash_init (&t->sp_table, hash_hash_spte, hash_less_spte, NULL);
}

/*
 * Load existing page from swap disk, mapped file, or program file.
 */
void
load_page (struct spt_entry *spte)
{
  struct thread *t = thread_current ();
  struct ft_entry *fte = allocate_frame ();
  fte->spte = spte;
  fte->owner = t;
  spte->fte = fte;
  ASSERT (pagedir_get_page (t->pagedir, spte->upage) == NULL);
  pagedir_set_page (t->pagedir, spte->upage, fte->frame, spte->writable);

  /* Swap the frame out */
  if (spte->type == NORMAL_PAGE) {
    // printf ("hell normal\n");
    load_normal_page (spte);
  }
  else if (spte->type == MMAP_PAGE) {
    load_mmap_page (spte);
  }
  else if (spte->type == SEGMENT_PAGE) {
    load_segment_page (spte);
  }


}

/*
 * Free page, its corresponding frame and maybe the slot in swap disk.
 */
/*void
free_page (struct spt_entry *spte) {
  struct thread *t = thread_current ();
  
  if (spte->type == MMAP_PAGE) {
    free_mmap_page (spte);
  }
  else if (spte->type == NORMAL_PAGE) {
    free_normal_page (spte);
  }
  else if (spte->type == SEGMENT_PAGE) {
    // Do nothing
    // free_segment_page (spte);
  }

  if (spte->fte != NULL)
    free_frame (spte->fte);

  hash_delete (&t->sp_table, &spte->elem_hash);
  free (spte);
}*/

/*
 * Make new spt_entry for ADDR.
 */
struct spt_entry *
allocate_page (void *addr, bool writable)
{
  struct thread *t = thread_current ();
  
  struct spt_entry *spte = malloc (sizeof (struct spt_entry));
  if (spte == NULL)
    PANIC ("malloc(spt_entry) failed");

  spte->upage = pg_round_down (addr);
  spte->writable = writable;
  lock_init (&spte->evict_lock);
  hash_insert (&t->sp_table, &spte->elem_hash);

  /* Install page */
  struct ft_entry *fte = allocate_frame ();
  fte->spte = spte;
  fte->owner = t;
  spte->fte = fte;
  ASSERT (pagedir_get_page (t->pagedir, spte->upage) == NULL);
  pagedir_set_page (t->pagedir, spte->upage, fte->frame, writable);

  return spte;
}

struct spt_entry *
allocate_normal_page (void *upage) {
  struct spt_entry *spte = allocate_page (upage, true);
  spte->type = NORMAL_PAGE;
  spte->slot_idx = -1;
  return spte;
}

void
load_normal_page (struct spt_entry *spte) {
  ASSERT (spte->type == NORMAL_PAGE);
  ASSERT (spte->fte != NULL);

  void *frame = spte->fte->frame;
  if (spte->slot_idx == -1)
    memset (frame, 0, PGSIZE);
  else
    swap_in (frame, spte->slot_idx);
}

void
evict_normal_page (struct spt_entry *spte) {
  ASSERT (spte->type == NORMAL_PAGE);
  ASSERT (spte->fte != NULL);

  void *frame = spte->fte->frame;
  int slot_idx = swap_out (frame);
  if (slot_idx == (int)BITMAP_ERROR)
    PANIC ("swap_out() returned BITMAP_ERROR");

  spte->slot_idx = slot_idx;
}

/*void
free_normal_page (struct spt_entry *spte) {
  ASSERT (spte->type == NORMAL_PAGE);
  
  if (spte->fte == NULL) {
    int slot_idx = spte->slot_idx;
    free_slot (slot_idx);
  }
}*/
