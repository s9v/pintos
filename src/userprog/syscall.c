#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
int write (int fd, const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  int syscall_no = *(int *) f->esp;
  if(syscall_no == SYS_WRITE) {
  	int fd = *(int *) (f->esp + 4);
  	void *buffer = (void *) *(int *) (f->esp + 8);
  	unsigned size = *(int *) (f->esp + 12);
  	printf("fd: %d\n",fd);
  	printf("size: %d\n",size);
  	printf("buff: %s\n",(char *)buffer);
  	write(fd, buffer, size);
  }
}

// void exit (int status) {
//   printf("%s: exit(%d)\n",thread_current()->name, status);
// }

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