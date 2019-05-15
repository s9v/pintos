#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <hash.h>
#include <list.h>

struct ft_entry
{
	void *frame;
	struct thread *owner;
	struct spt_entry *spte;
	
  struct list_elem elem;
};

void frame_init (void);
struct ft_entry *allocate_frame (void);
struct ft_entry *evict_frame (struct ft_entry *fte);
// void free_frame (struct ft_entry *fte);

#endif /* vm/frame.h */
