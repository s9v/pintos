#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <hash.h>
#include <list.h>

struct ft_entry
{
	void* frame; // kpage
	struct thread* owner;
	struct spt_entry* spte;
	// struct hash_elem elem_hash;
	struct list_elem elem;
};

void frame_init (void);
void *allocate_frame (void *addr, bool writable);


#endif /* vm/frame.h */
