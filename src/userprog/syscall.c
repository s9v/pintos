#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/init.h"
#include "string.h"

static void syscall_handler (struct intr_frame *);

void syscall_handler_arg0 (int, struct intr_frame *);
void syscall_handler_arg1 (int, struct intr_frame *);
void syscall_handler_arg2 (int, struct intr_frame *);

void exit (int status);
int write (int fd, const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_no = *(int *) f->esp;

  if (syscall_no == SYS_WRITE) {
    int *ptr = f->esp;

    ptr++;
    int fd = *ptr;

    ptr++;
    void *buffer = *(void **)ptr;

    ptr++;
    unsigned size = *(unsigned *)ptr;
    
    f->eax = write(fd, buffer, size);
  }
  else
  switch (syscall_no) {
    case SYS_HALT:
      syscall_handler_arg0(syscall_no, f);
    break;

    case SYS_EXIT: 
    case SYS_EXEC: 
    case SYS_WAIT: 
    // case SYS_REMOVE: 
    // case SYS_OPEN: 
    // case SYS_FILESIZE: 
    // case SYS_TELL: 
    // case SYS_CLOSE: 
      syscall_handler_arg1(syscall_no, f);
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
  ptr++;
  int arg1 = *ptr;

  if (syscall_no == SYS_EXIT) {
    int status = arg1;
    exit(status);
  } else if (syscall_no == SYS_EXEC) {
    char *file = (char *) arg1;
    f->eax = process_execute (file);
  } else if (syscall_no == SYS_WAIT) {
    pid_t pid = (pid_t) arg1;
    f->eax = process_wait (pid);
  } else if (syscall_no == SYS_REMOVE) {

  } else if (syscall_no == SYS_OPEN) {

  } else if (syscall_no == SYS_FILESIZE) {

  } else if (syscall_no == SYS_TELL) {

  } else if (syscall_no == SYS_CLOSE) {

  }
}

void syscall_handler_arg2 (int syscall_no, struct intr_frame *f) {
  int *ptr = f->esp;
  ptr++;
  int arg1 = *ptr;
  ptr++;
  int arg2 = *ptr;

  // if statements ?????
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

int write (int fd, const void *buffer, unsigned size) {
	if (fd == 1) {
		putbuf (buffer, size);
		return 0;
	}
	else {
		printf ("unknown fd: %d\n", fd);
		return -1;
	}
}