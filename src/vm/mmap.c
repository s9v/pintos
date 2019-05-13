#include "threads/vaddr.h"
#include "vm/mmap.h"

void mmap_init (void) {
	struct thread *t = thread_current ();
	list_init (&t->file_mappings);
	t->next_mapid = 0;
}

madip_t mmap (int fd, void *addr) {
	// Memory mapping fd's 0 and 1 is not allow
	if (fd == 0 || fd == 1)
		return -1;

	// ADDR must be page-aligned
	if (pg_ofs (addr) != 0)
		return -1;

	void *file = file_reopen (fde->file);
	if (file == NULL)
		return -1;
	if (file_length (file) == 0) {
		close (file)
		return -1;
	}

	struct thread *t = thread_current ();
	struct fd_elem *fde = thread_get_fde (fd);
	struct file_mapping *fmap = (struct file_mapping *) malloc (sizeof (struct file_mapping));
	if (fmap == NULL)
		return -1;

	fmap->mapid = t->next_mapid++;
	fmap->file = file;
	fmap->upage = addr;
	list_push_back (&t->file_mappings, fmap->elem);
}

struct file_mapping *get_file_mapping (mapid_t mapid) {
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

void munmap (mapid_t mapid) {
	struct file_mapping *fmap = get_file_mapping (mapid);

	if (fmap == NULL)
		return;

	file_close (fmap->file);
	list_remove (&fmap->elem);
	free_page (fmap->upage);
	free (fmap);
}

void write_to_file (struct file *file, off_t offset, void *frame) {
	off_t size = PGSIZE;
	off_t file_len = file_length (file);
	if (size > file_len - offset)
		size = file_len - offset;
	
	file_seek (file, offset);
	file_write (file, frame, size);
}

void read_from_file (struct file *file, off_t offset, void *frame) {
	off_t size = PGSIZE;
	off_t file_len = file_length (file);
	if (size > file_len - offset)
		size = file_len - offset;
	
	file_seek (file, offset);
	file_read (file, frame, size);
	memset (frame + size, 0, PGSIZE - size);
}
