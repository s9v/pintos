#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/file.h"
#include "vm/page.h"
#include <list.h>
#include <stdint.h>
#include <stdbool.h>

struct program_segment {
	struct file *file;
	off_t ofs;
	uint8_t *upage;
	int read_bytes;
	int zero_bytes;
	bool writable;
	struct list_elem elem;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool load_segment_page (struct spt_entry *spte);
struct spt_entry *allocate_segment_page (void *upage, struct program_segment *ps);

#endif /* userprog/process.h */
