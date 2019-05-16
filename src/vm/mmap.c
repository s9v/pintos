#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/mmap.h"
#include "vm/page.h"
#include "userprog/syscall.h"
#include <string.h>

struct file_mapping *get_file_mapping (mapid_t mapid);

void
mmap_init (struct thread *t) {
	list_init (&t->file_mappings);
	t->next_mapid = 0;
}

mapid_t
mmap_mmap (int fd, void *addr) {
	if (fd == 0 || fd == 1) // Memory mapping fd's 0 and 1 is not allow
		return -1;

	if (pg_ofs (addr) != 0) // ADDR must be page-aligned
		return -1;

	struct fd_elem *fde;
	struct file *file;
	off_t file_len;
	struct file_mapping *fmap;
	struct thread *t = thread_current ();

	fde = thread_get_fde (fd);
	if (fde == NULL)
		return -1;

  lock_acquire (&fs_lock);
	file = file_reopen (fde->file);
	lock_release (&fs_lock);

	if (file == NULL)
		return -1;

  lock_acquire (&fs_lock);
  file_len = file_length (file);
	lock_release (&fs_lock);

	if (file_len == 0)
			goto fail;

	// ==== Check for overlap ====

  uint8_t *L = addr;
  uint8_t *R = L + file_len;
  R = pg_round_up(R);

  // Overlap with other file mappings
  struct list_elem *e;
  for (e = list_begin (&t->file_mappings); e != list_end (&t->file_mappings);
  			e = list_next (e)) {
  	struct file_mapping *_fmap = list_entry (e, struct file_mapping, elem);
  	
  	lock_acquire (&fs_lock);
  	off_t _file_len = file_length (_fmap->file);
		lock_release (&fs_lock);

  	uint8_t *LL = _fmap->upage;
  	uint8_t *RR = LL + _file_len;
	  RR = pg_round_up(RR);

	  if ((L <= LL && LL < R) || (L <= RR && RR < R))
	  	goto fail;
	}

	// Overlap with program segments
	for (e = list_begin (&t->segments); e != list_end (&t->segments);
  			e = list_next (e)) {
  	struct program_segment *_segment = list_entry (e, struct program_segment, elem);
  	off_t _segment_len = _segment->read_bytes + _segment->zero_bytes;
  	
  	uint8_t *LL = _segment->upage;
  	uint8_t *RR = LL + _segment_len;
	  RR = pg_round_up(RR);

	  if ((L <= LL && LL < R) || (L <= RR && RR < R))
	  	goto fail;
	}

	// Overlap with existing pages (including stack pages)
	// uint8_t *_upage;
	// for (_upage = L; _upage < R; _upage += PGSIZE) {
	// 	struct spt_entry *spte = hash_get_spte (_upage);
	// 	if (spte != NULL)
	// 		goto fail;
	// }

	if ((uint8_t *)STACK_LIMIT <= L || (uint8_t *)STACK_LIMIT < R)
		goto fail;

  // ==== end of Check for overlap ====

	fmap = (struct file_mapping *) malloc (sizeof (struct file_mapping));
	if (fmap == NULL)
		goto fail;

	fmap->mapid = t->next_mapid++;
	fmap->file = file;
	fmap->upage = addr;
	list_push_back (&t->file_mappings, &fmap->elem);

	return fmap->mapid;

fail:
  lock_acquire (&fs_lock);
	if (file != NULL)
		file_close (file);
  lock_release (&fs_lock);

  if (fmap != NULL)
  	free (fmap);

	return -1;
}

struct file_mapping *
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

  lock_acquire (&fs_lock);
	off_t file_len = file_length (fmap->file);
  lock_release (&fs_lock);

  uint8_t *L = fmap->upage;
  uint8_t *R = L + file_len;
  R = pg_round_up(R);
	
	uint8_t *_upage;
	for (_upage = L; _upage < R; _upage += PGSIZE) {
		struct spt_entry *spte = hash_get_spte (_upage);
		
		if (spte != NULL) {
			ASSERT (spte->type == MMAP_PAGE);
			
			list_remove (&spte->elem);
			free_page (spte);
		}
	}

	list_remove (&fmap->elem);
	file_close (fmap->file);
	free (fmap);
}

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

void
free_mmap_page (struct spt_entry *spte) {
	ASSERT (spte->type == MMAP_PAGE);

	if (spte->fte != NULL)
		evict_mmap_page (spte);
}

void
evict_mmap_page (struct spt_entry *spte) {
	ASSERT (spte->type == MMAP_PAGE);
	ASSERT (spte->fte != NULL);

	struct file *file = spte->fmap->file;
	off_t offset = spte->offset;
	void *frame = spte->fte->frame;
	write_to_file (file, offset, frame);
}
