#include "threads/malloc.h"
#include "vm/page.h"
#include <stddef.h>
#include <debug.h>

unsigned hash_hash_page (const struct hash_elem *e, void *aux UNUSED);
bool hash_less_page (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

unsigned hash_hash_spte (const struct hash_elem *e, void *aux UNUSED) {
    struct spt_entry *spte = hash_entry (e, struct spt_entry, elem_hash);
    return hash_int ((int)spte->upage);
}

bool hash_less_spte (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct spt_entry *spte1 = hash_entry (a, struct spt_entry, elem_hash);
    struct spt_entry *spte2 = hash_entry (b, struct spt_entry, elem_hash);
    return spte1->upage < spte2->upage;
}

/*
 * Initialize supplementary page table
 */
void 
page_init (void)
{
  struct thread *t = thread_current ();
  hash_init (&t->sp_table, hash_hash_spte, hash_less_spte, NULL);
}

/*
 * Load existing page from swap disk, mapped file, or program file.
 */
void load_page (spt_entry *spte)
{
  void *frame = allocate_frame (spte);

  if (frame == NULL)
    PANIC ('allocate_frame failed');

  /* Swap the frame out */
  if (spte->type == NORMAL_PAGE) {
    swap_in (frame, spte->slot_idx);
  }
  else if (spte->type == MMAP_PAGE) {
    struct file_mapping *fmap = spte->fmap;
    read_from_file (fmap, spte->offset, frame);
  }
  else if (spte->type == SEGMENT_PAGE) {
    spte->upage
    load_segment_page ();
  }
}

/*
 * Free page, its corresponding frame and maybe the slot in swap disk.
 */
void free_page (void *upage) {

}

/* === Helper functions === */

/*
 * Get spt_entry by 
 */
struct spt_entry *hash_get_spte (void *upage) {
  struct thread *t = thread_current ();

  struct spt_entry dummy_spte;
  dummy_spte.upage = upage;
  struct hash_elem *he = hash_find (&t->sp_table, &dummy_spte.elem_hash);

  if (he == NULL)
    return NULL;

  struct spt_entry *spte = hash_entry (he, struct spt_entry, elem_hash);
  return spte;
}

/*
 * Make new spt_entry for ADDR.
 */
struct spt_entry *
allocate_page (void *addr, bool writable)
{
  struct thread *t = thread_current ();
  struct spt_entry *spte = malloc (sizeof (struct sup_page_table_entry));
  if (spte == NULL)
    return NULL;

  spte->upage = pg_round_down (addr);
  spte->writable = writable;

  hash_insert (&t->sp_table, spte->elem_hash);

  return spte;
}
