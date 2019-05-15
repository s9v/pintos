#ifndef VM_MMAP_H
#define VM_MMAP_H

typedef int mapid_t;

#include "threads/thread.h"
#include "filesys/file.h"
#include <list.h>


/* Memory mapping for a file */
struct file_mapping {
	mapid_t mapid;
	struct list_elem elem;
  struct file *file;
  uint8_t *upage;        // start at this user address
};

void mmap_init (struct thread *t);
mapid_t mmap_mmap (int fd, void *addr);
// void mmap_munmap (mapid_t mapid);
void write_to_file (struct file *file, off_t offset, uint8_t *frame);
void read_from_file (struct file *file, off_t offset, uint8_t *frame);
struct spt_entry *allocate_mmap_page (uint8_t *upage, struct file_mapping *fmap);
void load_mmap_page (struct spt_entry *spte);
// void free_mmap_page (struct spt_entry *spte);
void evict_mmap_page (struct spt_entry *spte);

#endif