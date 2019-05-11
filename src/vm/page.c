#include "threads/malloc.h"
#include <stddef.h>
#include "vm/page.h"

/*
 * Initialize supplementary page table
 */
void 
page_init (void)
{

}

/*
 * Make new supplementary page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (void *addr)
{
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));

  if (spte == NULL)
  	return NULL;

  spte->user_vaddr = addr;

  spte->access_time = (uint64_t)0;
  spte->dirty = false;
  spte->accessed = false;

  return spte;
}
