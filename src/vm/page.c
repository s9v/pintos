#include "threads/malloc.h"
#include <stddef.h>
#include "vm/page.h"

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
 * Make new supplementary page table entry for addr 
 */
struct spt_entry *
allocate_page (void *addr)
{
  struct thread *t = thread_current ();
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));

  if (spte == NULL)
    return NULL;

  spte->upage = addr;

  spte->access_time = (uint64_t)0;
  spte->dirty = false;
  spte->accessed = false;

  hash_insert (&t->sp_table, spte->elem_hash);

  return spte;
}
