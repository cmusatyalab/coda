#ifndef _BLURB_
#define _BLURB_
/*
                    RVM - Recoverable Virtual Memory
                             Release 1.1
                      School of Computer Science
                      Carnegie Mellon University
                          Copyright 1987-92

Use of RVM outside Carnegie Mellon University requires a license.
*/

static char *rcsid = "$Header: rvm_machutil.c,v 1.2 94/03/15 09:49:45 raiff Exp $";
#endif _BLURB_



#include <rvm.h>
#include "rvm_segment.h"

#ifdef MACH
#include <mach.h>

rvm_return_t    
allocate_mach_vm(addr, length)
     char **addr;
     unsigned long length;
{
    kern_return_t ret;
    int anywhere = (*addr == 0);
	
    ret = vm_allocate(task_self(), addr, length, anywhere);
    if (ret == KERN_INVALID_ADDRESS) return RVM_ERANGE;
    if (ret == KERN_NO_SPACE) return RVM_ENO_MEMORY;
}

rvm_return_t    
deallocate_mach_vm(addr, length)
     char *addr;
     unsigned long length;
{
    kern_return_t ret;
    
    ret = vm_deallocate(task_self(), addr, length);
    if (ret == KERN_INVALID_ADDRESS) return RVM_ERANGE;
}
#endif MACH
