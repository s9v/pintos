#ifndef VM_MMAP_H
#define VM_MMAP_H
#include "filesys/file.h"

typedef int mapid_t;

/* Memory mapping for a file */
struct file_mapping {
	mapid_t mapid;
	struct file *file;
	void *upage;
	struct list_elem elem;
};

void mmap_init (void);
madip_t mmap (int fd, void *addr);
void munmap (mapid_t mapid);
void write_to_file (struct file *file, off_t offset, void *frame);
void read_from_file (struct file_mapping *fmap, off_t offset, void *frame);

#endif