#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/rvm-src/seg/rvm_segutil.c,v 4.4 1998/03/06 20:21:54 braam Exp $";
#endif _BLURB_

#include <unistd.h>
#include <stdlib.h>
#include <rvm.h>
#include "rvm_segment.h"

#ifdef __CYGWIN32__
#include <windows.h>
#endif

/* Routine to check if regions will overlap in memory. */

int overlap(nregions, regionDefs)
     unsigned long      nregions; 
     rvm_region_def_t   regionDefs[];
{
    int i,j;
    rvm_region_def_t temp;

    /* sort array */
    for (i = 0; i < (nregions - 1); i++) {
	for (j = i + 1; j < nregions; j++) {
	    if (regionDefs[j].vmaddr < regionDefs[i].vmaddr) {
		temp.vmaddr = regionDefs[i].vmaddr;
		temp.length = regionDefs[i].length;
		temp.offset = regionDefs[i].offset;

		regionDefs[i].vmaddr = regionDefs[j].vmaddr;
		regionDefs[i].length = regionDefs[j].length;
		regionDefs[i].offset = regionDefs[j].offset;

		regionDefs[j].vmaddr = temp.vmaddr;
		regionDefs[j].length = temp.length;
		regionDefs[j].offset = temp.offset;
	    }
	}
    }

    for (i = 0; i < (nregions - 1); i++) {
	if (regionDefs[i].vmaddr + regionDefs[i].length > regionDefs[i+1].vmaddr)
	    return(TRUE);
    }

    return FALSE;
}     


/* BSD44 memory allocation; uses mmap as an allocator.  Any mmap-aware
   system should be able to use this code */

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#define ALLOCATE_VM_DEFINED


rvm_return_t    
allocate_vm(addr, length)
     char **addr;
     unsigned long length;
{
    rvm_return_t  ret = RVM_SUCCESS;
    char *requested_addr = *addr; /* save this so that we can check it
				     against the address location
				     returned by mmap. this is
				     important because if it isn't 0,
				     it's a location that we HAVE to
				     be able to map to. */

#ifdef __CYGWIN32__
    {
      HANDLE hMap = CreateFileMapping((HANDLE)0xFFFFFFFF, NULL,
                                      PAGE_READWRITE, 0, length, NULL);
      if (hMap == NULL)
          return(RVM_EINTERNAL);
      *addr = MapViewOfFileEx(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0, *addr);
      if (*addr == NULL)
          *addr = (char *)-1;
      CloseHandle(hMap);
    }
#else
    *addr = mmap(*addr, length, (PROT_READ | PROT_WRITE), 
		 (MAP_PRIVATE | MAP_ANON), -1, 0);
#endif

    if (*addr == (char*)-1) {
	if (errno == ENOMEM) {
	    ret = RVM_ENO_MEMORY;
	} else {
	    ret = RVM_EINTERNAL;
	}
    }

    if (requested_addr != 0 && *addr != requested_addr) {
	ret = RVM_EINTERNAL;	/* couldn't allocated requested memory. */
    }

    /* modified by tilt, Nov 19 1996.
       When we allocate a page (or range of pages) we register
       it in an internal table we're keeping around to keep
       track of pages. (The previous solution was to try to
       re-allocate the page, and see if it fails, which is
       not only wrong [since we don't if it's allocated, or
       actually allocated in the RVM heap!!], but doesn't
       work with mmap()). */
    if (rvm_register_page(*addr, length) == rvm_false) {
	ret = RVM_EINTERNAL;
    }
    
    return ret;
}

rvm_return_t    
deallocate_vm(addr, length)
     char *addr;
     unsigned long length;
{
    rvm_return_t   ret = RVM_SUCCESS;

#ifdef __CYGWIN32__
    UnmapViewOfFile(addr);
#else
    if (munmap(addr, length)) {
	ret = RVM_EINTERNAL;
    }
#endif

    if (rvm_unregister_page(addr, length) == rvm_false) {
	ret = RVM_EINTERNAL;
    }

    return ret;
}


