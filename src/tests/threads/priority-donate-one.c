/* The main thread acquires a lock.  Then it creates two
   higher-priority threads that block acquiring the lock, causing
   them to donate their priorities to the main thread.  When the
   main thread releases the lock, the other threads should
   acquire it in priority order.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire1_thread_func;
static thread_func acquire2_thread_func;

void
test_priority_donate_one (void) 
{
  // printf ("<1>\n");
  // printf("thread_get_priority (): %d\n",thread_get_priority ());
  struct lock lock;
  // printf("thread_get_priority (): %d\n",thread_get_priority ());

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);
  // printf("thread_get_priority (): %d\n",thread_get_priority ());

  /* Make sure our priority is the default. */
  // printf("thread_get_priority (): %d\n",thread_get_priority ());
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  // printf ("<2>\n");

  lock_init (&lock);
  // printf ("<3>\n");
  lock_acquire (&lock);
  // printf ("<4>\n");
  thread_create ("acquire1", PRI_DEFAULT + 1, acquire1_thread_func, &lock);
  // printf ("<5>\n");
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("acquire2", PRI_DEFAULT + 2, acquire2_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  lock_release (&lock);
  msg ("acquire2, acquire1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}

static void
acquire1_thread_func (void *lock_) 
{
  // printf ("acquire1 <1>\n");
  struct lock *lock = lock_;
  // printf ("acquire1 <2>\n");
  lock_acquire (lock);
  // printf ("acquire1 <3>\n");
  msg ("acquire1: got the lock");
  lock_release (lock);
  msg ("acquire1: done");
  // printf ("acquire1 <9>\n");
}

static void
acquire2_thread_func (void *lock_) 
{
  // printf ("<1> ACQUIRE2 thread_get_priority = %d\n", thread_get_priority ());
  struct lock *lock = lock_;

  lock_acquire (lock);
  // printf ("<2> ACQUIRE2 thread_get_priority = %d\n", thread_get_priority ());
  msg ("acquire2: got the lock");
  // printf ("<3> ACQUIRE2 thread_get_priority = %d\n", thread_get_priority ());
  lock_release (lock);
  // printf ("<4> ACQUIRE2 thread_get_priority = %d\n", thread_get_priority ());
  msg ("acquire2: done");
  // printf ("<5> ACQUIRE2 thread_get_priority = %d\n", thread_get_priority ());

}
