/* Local copy to tide over problems caused by Dale Moore's release of
   new CODA_ASSERT.h.  Get him to fix it! (Satya 12/26/92)
   
   Also, redefine CODA_ASSERT() to invoke SIGTRAP rather than abort() or exit();
   allows zombie-ing by signal handlers.

   Redefine CODA_ASSERT to dereference zero. (luqi 1/17/95)
*/

#ifndef _ASSERT_H_
#define _ASSERT_H_ 1

#include <stdio.h>
#include <unistd.h>

#define CODA_ASSERT(ex) do {\
    if (!(ex)) {\
	fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
	fflush(stderr);\
    while ( 1 ) { sleep(1); } ;\
    }\
} while (0)


#endif  _ASSERT_H_

