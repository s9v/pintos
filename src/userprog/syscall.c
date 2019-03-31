#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "string.h"

static void syscall_handler (struct intr_frame *);
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
  printf ("system call! [%d]\n", syscall_no);
  if (syscall_no == SYS_WRITE) {
  	int *ptr = f->esp;

  	ptr++;
  	int fd = *ptr;

  	ptr++;
  	void *buffer = *(void **)ptr;

  	ptr++;
  	unsigned size = *(unsigned *)ptr;
  	
  	// printf("fd: %d\n",fd);
  	// printf("size: %d\n",size);
  	// printf("buff: %s\n",(char *)buffer);
  	write(fd, buffer, size);
  }
  else if (syscall_no == SYS_EXIT) {
  	int *ptr = f->esp;

  	ptr++;
  	int status = *ptr;

  	exit(status);
  }
  else {
  	PANIC ("yaaani: [%d]", syscall_no);
  }
}

void exit (int status) {
  // copy
  char *file_name = thread_current()->name;
  int file_name_len = strlen(file_name);
  char *file_name2 = malloc (file_name_len);
  strlcpy (file_name2, file_name, file_name_len + 1);

  // open real file_name
  char *save_ptr, *file_name_real;
  file_name_real = strtok_r (file_name2, " ", &save_ptr);

  printf ("%s: exit(%d)\n", file_name_real, status); // KEEP THIS !!!!

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