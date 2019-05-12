#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <hash.h>
#include <list.h>

struct frame_table_entry
{
	void* frame; // kpage
	struct thread* owner;
	struct sup_page_table_entry* spte;
	struct hash_elem elem_hash;
	struct list_elem elem;
};

void frame_init (void);
void *allocate_frame (void *addr, bool writable);


#endif /* vm/frame.h */
