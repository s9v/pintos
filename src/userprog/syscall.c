#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "devices/input.h"
#include "string.h"

// Global variables
struct lock fd_lock;
struct lock fs_lock;

static void syscall_handler (struct intr_frame *);

void syscall_handler_arg0 (int, struct intr_frame *);
void syscall_handler_arg1 (int, struct intr_frame *);
void syscall_handler_arg2 (int, struct intr_frame *);
void syscall_handler_arg3 (int, struct intr_frame *);

int write (int fd, const void *buffer, unsigned length);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void check_user_ptr (int *ptr, int offset);
void check_valid_ptr (const void *ptr);

void check_user_ptr (int *ptr, int offset) {
  if (ptr <= (int *)0x08048000 || (int *)PHYS_BASE <= ptr + offset) {
    exit (-1);
  }
}

void check_valid_ptr (const void *ptr) {
  struct thread *cur = thread_current ();

  if (ptr == NULL || !is_user_vaddr (ptr) || pagedir_get_page (cur->pagedir, ptr) == NULL)
    exit (-1);
}

void
syscall_init (void) 
{
  lock_init (&fd_lock);
  lock_init (&fs_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  check_user_ptr (f->esp, 0);

  int syscall_no = *(int *) f->esp;

  switch (syscall_no) {
    case SYS_HALT:
      syscall_handler_arg0(syscall_no, f);
    break;

    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
      syscall_handler_arg1(syscall_no, f);
    break;

    case SYS_CREATE:
    case SYS_SEEK:
      syscall_handler_arg2(syscall_no, f);
    break;

    case SYS_READ:
    case SYS_WRITE:
      syscall_handler_arg3(syscall_no, f);
    break;


    default:
      PANIC ("syscall not implemented: [%d]", syscall_no);
    break;
  }
  
}

void syscall_handler_arg0 (int syscall_no, struct intr_frame *f UNUSED) {
  if (syscall_no == SYS_HALT) {
    power_off();
  }
}

void syscall_handler_arg1 (int syscall_no, struct intr_frame *f) {
  int *ptr = f->esp;
  check_user_ptr (ptr, 1);

  ptr++;
  int arg1 = *ptr;

  if (syscall_no == SYS_EXIT) {
    int status = arg1;
    exit(status);
  } else if (syscall_no == SYS_EXEC) {
    char *filename = (char *) arg1;

    // TODO move file-exist check inside process_execute()
    // open file
    lock_acquire (&fs_lock);
    struct file *file = filesys_open (filename);
    lock_release (&fs_lock);

    if (file == NULL) {
      f->eax = -1;
      return;
    }

    // close file
    lock_acquire (&fs_lock);
    file_close (file);
    lock_release (&fs_lock);

    f->eax = process_execute (filename);
  } else if (syscall_no == SYS_WAIT) {
    pid_t pid = (pid_t) arg1;
    f->eax = process_wait (pid);
  } else if (syscall_no == SYS_REMOVE) {
    char *file = (char *) arg1;
    f->eax = remove (file);
  } else if (syscall_no == SYS_OPEN) {
    char *file = (char *) arg1;
    f->eax = open (file);
  } else if (syscall_no == SYS_FILESIZE) {
    int fd = arg1;
    f->eax = filesize (fd);
  } else if (syscall_no == SYS_TELL) {
    int fd = arg1;
    f->eax = tell (fd);
  } else if (syscall_no == SYS_CLOSE) {
    int fd = arg1;
    close (fd);
  }
}

void syscall_handler_arg2 (int syscall_no, struct intr_frame *f) {
  int *ptr = f->esp;
  check_user_ptr (ptr, 2);

  ptr++;
  int arg1 = *ptr;
  ptr++;
  int arg2 = *ptr;

  if (syscall_no == SYS_CREATE) {
    char *file = (char *) arg1;
    unsigned initial_size = (unsigned) arg2;
    f->eax = create (file, initial_size);
  } else if (syscall_no == SYS_SEEK) {
    int fd = arg1;
    unsigned position = (unsigned) arg2;

    seek (fd, position);
  }
}

void syscall_handler_arg3 (int syscall_no, struct intr_frame *f) {
  int *ptr = f->esp;
  check_user_ptr (ptr, 3);

  ptr++;
  int arg1 = *ptr;
  ptr++;
  int arg2 = *ptr;
  ptr++;
  int arg3 = *ptr;

  if (syscall_no == SYS_READ) {
    int fd = arg1;
    void *buffer = (void *) arg2;
    unsigned length = (unsigned) arg3;

    f->eax = read (fd, buffer, length);
  } else if (syscall_no == SYS_WRITE) {
    int fd = arg1;
    void *buffer = (void *) arg2;
    unsigned length = (unsigned) arg3;
    
    f->eax = write(fd, buffer, length);
  }
}

/* ==== ACTUAL SYSCALL FUNCTIONS ==== */

void exit (int status) {
  // copy
  struct thread *cur = thread_current ();
  char *file_name = cur->name;
  int file_name_len = strlen(file_name);
  char *file_name2 = malloc (file_name_len);
  strlcpy (file_name2, file_name, file_name_len + 1);

  // open real file_name
  char *save_ptr, *file_name_real;
  file_name_real = strtok_r (file_name2, " ", &save_ptr);

  printf ("%s: exit(%d)\n", file_name_real, status); // KEEP THIS !!!!

  cur->exit_status = status;
  thread_exit ();
}

int write (int fd, const void *buffer, unsigned length) {
  check_valid_ptr (buffer);

	if (fd == STDOUT_FILENO) {
		putbuf (buffer, length);
		return length;
	}
	else {
    struct fd_elem *fde = thread_get_fde (fd);

    if (fde == NULL) {
      return -1;
    }

    lock_acquire (&fs_lock);
    int bytes_written = file_write (fde->file, buffer, length);
    lock_release (&fs_lock);
		return bytes_written;
	}
}

bool create (const char *file, unsigned initial_size) {
  check_valid_ptr (file);

  lock_acquire (&fs_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&fs_lock);

  return success;
}

bool remove (const char *file) {
  check_valid_ptr (file);

  lock_acquire (&fs_lock);
  bool success = filesys_remove (file);
  lock_release (&fs_lock);

  return success;
}

int open (const char *filename) {
  check_valid_ptr (filename);

  lock_acquire (&fs_lock);
  struct file *file = filesys_open (filename);
  lock_release (&fs_lock);

  if (file == NULL)
    return -1;

  struct fd_elem *fde = thread_new_fd (file);
  return fde->fd;
}

int filesize (int fd) {
  struct fd_elem *fde = thread_get_fde (fd);
  lock_acquire (&fs_lock);
  struct inode *inode = file_get_inode(fde->file);
  int length = inode_length (inode);
  lock_release (&fs_lock);

  return length;
}

int read (int fd, void *buffer, unsigned length) {
  check_valid_ptr (buffer);

  if (fd == STDIN_FILENO) {
    // TODO Zaaaaaaaaaaaaaa
    // input_getc()
    return -1;
  }
  else {
    struct fd_elem *fde = thread_get_fde (fd);

    if (fde == NULL) {
      return -1;
    }

    lock_acquire (&fs_lock);
    int bytes_read = file_read (fde->file, buffer, length);
    lock_release (&fs_lock);
    return bytes_read;
  }
}

void seek (int fd, unsigned position) {
  struct fd_elem *fde = thread_get_fde (fd);
  lock_acquire (&fs_lock);
  file_seek (fde->file, position);
  lock_release (&fs_lock);
}

unsigned tell (int fd) {
  struct fd_elem *fde = thread_get_fde (fd);
  lock_acquire (&fs_lock);
  unsigned position = file_tell (fde->file);
  lock_release (&fs_lock);

  return position;
}

void close (int fd) {
  struct fd_elem *fde = thread_get_fde (fd);

  if (fde == NULL)
    return;

  lock_acquire (&fs_lock);
  file_close (fde->file);
  lock_release (&fs_lock);
  thread_del_fde (fde);
}