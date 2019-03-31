/* Prints the command-line arguments.
   This program is used for all of the args-* tests.  Grading is
   done differently for each of the args-* tests based on the
   output. */

#include "tests/lib.h"

int
main (int argc, char *argv[]) 
{
  int i;

  test_name = "args";

  msg ("begin");
  msg ("argc = %d", argc);
  for (i = 0; i <= argc; i++) {
    // msg ("&&& argv[%d] = '%p'", i, argv+i);
    if (argv[i] != NULL) {
      msg ("argv[%d] = '%s'", i, argv[i]); // <----- one or both of these MSG lines cause pagefault
    }
    else {
      msg ("argv[%d] = null", i);
    }
  }
  msg ("end");

  return 0;
}
