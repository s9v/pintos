#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/mmap.h"
#include "vm/page.h"
#include <string.h>

void
mmap_init (struct thread *t) {
	list_init (&t->file_mappings);
	t->next_mapid = 0;
}

mapid_t
mmap_mmap (int fd, void *addr) {
	// Memory mapping fd's 0 and 1 is not allow
	if (fd == 0 || fd == 1)
		return -1;

	// ADDR must be page-aligned
	if (pg_ofs (addr) != 0)
		return -1;

	struct fd_elem *fde = thread_get_fde (fd);
	if (fde == NULL)
		return -1;

	void *file = file_reopen (fde->file);
	if (file == NULL)
		return -1;
	if (file_length (file) == 0) {
		file_close (file);
		return -1;
	}

	struct thread *t = thread_current ();
	struct file_mapping *fmap = (struct file_mapping *) malloc (sizeof (struct file_mapping));
	if (fmap == NULL)
		return -1;

	fmap->mapid = t->next_mapid++;
	fmap->file = file;
	fmap->upage = addr;
	list_push_back (&t->file_mappings, &fmap->elem);

	return fmap->mapid;
}

/*struct file_mapping *
get_file_mapping (mapid_t mapid) {
	struct thread *t = thread_current ();
	struct list_elem *e;
	for (e = list_begin (&t->file_mappings); e != list_end (&t->file_mappings);
		 e = list_next (e)) {
		struct file_mapping *fmap = list_entry (e, struct file_mapping, elem);

		if (fmap->mapid == mapid)
			return fmap;
	}

	return NULL;
}

void
mmap_munmap (mapid_t mapid) {
	struct file_mapping *fmap = get_file_mapping (mapid);

	if (fmap == NULL)
		return;

	uint *upage = fmap->upage;
	void *R = fmap->upage + file_length (fmap->file);
	for (; upage <= R; upage += PGSIZE) {
		struct spt_entry *spte = hash_get_spte (upage);
		free_page (spte);
	}

	list_remove (&fmap->elem);
	file_close (fmap->file);
	free (fmap);
}*/

void
write_to_file (struct file *file, off_t offset, uint8_t *frame) {
	off_t size = PGSIZE;
	off_t file_len = file_length (file);
	if (size > file_len - offset)
		size = file_len - offset;
	
	file_seek (file, offset);
	file_write (file, frame, size); // TODO check return value
}

void
read_from_file (struct file *file, off_t offset, uint8_t *frame) {
	off_t size = PGSIZE;
	off_t file_len = file_length (file);
	if (size > file_len - offset)
		size = file_len - offset;
	
	file_seek (file, offset);
	file_read (file, frame, size); // TODO check return value
	memset (frame + size, 0, PGSIZE - size);
}

struct spt_entry *
allocate_mmap_page (uint8_t *upage, struct file_mapping *fmap) {
  struct spt_entry *spte = allocate_page (upage, true);
  spte->type = MMAP_PAGE;
  spte->fmap = fmap;
  spte->offset = upage - fmap->upage;
  return spte;
}

void
load_mmap_page (struct spt_entry *spte) {
	ASSERT (spte->type == MMAP_PAGE);
	ASSERT (spte->fte != NULL);

	struct file *file = spte->fmap->file;
	off_t offset = spte->offset;
	void *frame = spte->fte->frame;
	read_from_file (file, offset, frame);
}

/*void
free_mmap_page (struct spt_entry *spte) {
	ASSERT (spte->type == MMAP_PAGE);

	if (spte->fte != NULL)
		evict_mmap_page (spte);
}*/

void
evict_mmap_page (struct spt_entry *spte) {
	ASSERT (spte->type == MMAP_PAGE);
	ASSERT (spte->fte != NULL);

	struct file *file = spte->fmap->file;
	off_t offset = spte->offset;
	void *frame = spte->fte->frame;
	write_to_file (file, offset, frame);
}
