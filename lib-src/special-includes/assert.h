/* Local copy to tide over problems caused by Dale Moore's release of
   new assert.h.  Get him to fix it! (Satya 12/26/92)
   
   Also, redefine assert() to invoke SIGTRAP rather than abort() or exit();
   allows zombie-ing by signal handlers.

   Redefine assert to dereference zero. (luqi 1/17/95)
*/

#ifndef _ASSERT_H_
#define _ASSERT_H_ 1

#include <stdio.h>

#define assert(ex) {\
    if (!(ex)) {\
	int *addr = 0;\
	fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
	fflush(stderr);\
	*addr = 1;\
    }\
}

#endif  _ASSERT_H_

