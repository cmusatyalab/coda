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

static char *rcsid = "$Header: rvm_segutil.c,v 1.5 96/11/19 14:29:07 tilt Exp $";
#endif _BLURB_

#include <unistd.h>
#include <rvm.h>
#include "rvm_segment.h"

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
/* Mach memory allocation functions */
#ifdef MACH
#include <mach.h>
#define ALLOCATE_VM_DEFINED

rvm_return_t    
allocate_vm(addr, length)
     char **addr;
     unsigned long length;
{
    kern_return_t ret;
    int anywhere = (*addr == 0);
	
    ret = vm_allocate(task_self(), addr, length, anywhere);
    if (ret == KERN_INVALID_ADDRESS) return RVM_ERANGE;
    if (ret == KERN_NO_SPACE) return RVM_ENO_MEMORY;

    return RVM_SUCCESS;
}

rvm_return_t    
deallocate_vm(addr, length)
     char *addr;
     unsigned long length;
{
    kern_return_t ret;
    
    ret = vm_deallocate(task_self(), addr, length);
    if (ret == KERN_INVALID_ADDRESS) return RVM_ERANGE;

    return RVM_SUCCESS;
}
#endif

/* NetBSD memory allocation; uses mmap as an allocator.  Any mmap-aware
   system should be able to use this code */

#if __NetBSD__ || LINUX
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

    *addr = mmap(*addr, length, (PROT_READ | PROT_WRITE), 
		 (MAP_PRIVATE | MAP_ANON), -1, 0);

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

    if (munmap(addr, length)) {
	ret = RVM_EINTERNAL;
    }    

    if (rvm_unregister_page(addr, length) == rvm_false) {
	ret = RVM_EINTERNAL;
    }

    return ret;
}


#endif /* __NetBSD__ */

/* Generic Unix memory allocation functions */
#ifndef ALLOCATE_VM_DEFINED
rvm_return_t
allocate_vm(addr, length)
     char **addr;
     unsigned long length;
    {
    rvm_length_t    base,oldbase;       /* base of memory allocated */
    rvm_length_t    pad,limit;          /* padding for page alignment */

    /* see if can allocate anywhere */
    if (*addr == 0)
        {
        /* round up present break pt to page size & allocate */
        oldbase = (rvm_length_t)sbrk(0);
        pad = RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(oldbase) - oldbase;
        base = (rvm_length_t)sbrk((int)(length+pad));
        if (base == -1) return RVM_ENO_MEMORY; /* out of space */
        if (base == oldbase)            /* got what we expected? */
            {
            *addr = (char *)(base+pad); /* yes, return 1st page addr */
            return RVM_SUCCESS;
            }
    
        /* test if got enough anyway by extracting 1st page addr
           and testing if in allocated region */
        oldbase = base;
        limit = base+pad+length;
        base = RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE(limit);
        if ((base-length) >= oldbase)
            {
            *addr = (char *)base;       /* yes, return 1st page addr */
            return RVM_SUCCESS;
            }

        /* try to get remainder of last page */
        pad = RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(limit) - limit;
        base = (rvm_length_t)sbrk((int)(pad));
        if (base == -1) return RVM_ENO_MEMORY; /* out of space */
        if (base == limit)              /* got remainder? */
            {
            *addr = (char *)((base+pad)-length); /* yes, return 1st page addr */
            return RVM_SUCCESS;
            }
    
        /* reallocate with certianty */
        base = (rvm_length_t)sbrk((int)(length+RVM_PAGE_SIZE-1));
        if (base == -1) return RVM_ENO_MEMORY; /* out of space */
        *addr = RVM_ROUND_ADDR_UP_TO_PAGE_SIZE(base);
        }
    else                              /* move break pt to specified addr */
        {
        /* test if break pt already beyond request */
        oldbase = (rvm_length_t)sbrk(0);
        if (oldbase > (rvm_length_t)*addr)
            return RVM_ERANGE;

        /* no, set new break pt for request */
        oldbase=(rvm_length_t)
            sbrk((int)RVM_ADD_LENGTH_TO_ADDR(*addr,length)-oldbase);
        if (oldbase == -1)
            return RVM_ENO_MEMORY;
        }

    return RVM_SUCCESS;
    }

rvm_return_t    
deallocate_vm(addr, length)
     char *addr;
     unsigned long length;
    {
    rvm_length_t    oldbase;            /* base of memory allocated */

    return RVM_SUCCESS;                 /* deallocation below may not work */

    /* reset break pt if deallocated space is at end of
       Unix data segment */
/*
    oldbase = (rvm_length_t)sbrk(0);
    if ((char *)oldbase == RVM_ADD_LENGTH_TO_ADDR(addr,length))
        {
        oldbase = (rvm_length_t)brk(addr);
        if (oldbase == -1)
            return RVM_ERANGE;
        }

    return RVM_SUCCESS;
*/
    }
#endif MACH
