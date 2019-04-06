#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

struct lock fs_lock;

// #define FSLOCK_WRAP (FSCODE)    ( lock_acquire (&fs_lock); (FSCODE); lock_acquire (&fs_lock); )

typedef int pid_t;
void syscall_init (void);
void exit (int status);

#endif /* userprog/syscall.h */
