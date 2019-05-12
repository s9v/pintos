#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <stdint.h>
#include <stdbool.h>
#include <hash.h>

// Supplemental page table entry
struct spt_entry 
{
	void *upage;
	struct hash elem_hash;

	bool is_mmapped;
	bool is_readonly;
	bool is_segment;

	uint64_t access_time;
	bool dirty;
	bool accessed;
};

void page_init (void);
struct sup_page_table_entry *allocate_page (void *addr);

#endif /* vm/page.h */
