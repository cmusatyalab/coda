/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <unistd.h>
#include <stdlib.h>
#include <rvm.h>
#include "rvm_segment.h"

/* from rvm_private.h */
rvm_bool_t rvm_register_page(char *vmaddr, rvm_length_t length);
rvm_bool_t rvm_unregister_page(char *vmaddr, rvm_length_t length);

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


