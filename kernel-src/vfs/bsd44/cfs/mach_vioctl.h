/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	mach_vioctl.h,v $
 * Revision 1.2.34.1  97/11/12  12:39:54  rvb
 * First cut at prototype
 * 
 * Revision 1.2  96/01/02  16:57:27  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:54  bnoble
 * Added CFS-specific files
 *
 * Revision 2.4  90/08/30  11:51:12  bohman
 * 	Ioctl changes for STDC.
 * 	[90/08/28            bohman]
 * 
 * Revision 2.3  89/03/09  22:10:26  rpd
 * 	More cleanup.
 * 
 * Revision 2.2  89/02/25  17:58:32  gm0w
 * 	Changes for cleanup.
 * 
 *  7-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	No need for VICE conditional.
 *
 * 22-Oct-86  Jay Kistler (jjk) at Carnegie-Mellon University
 *	Created from Andrew's vice.h and viceioctl.h.
 *
 */
/*
 * ITC Remote file system - vice ioctl interface module
 */

/*
 *  TODO:  Find /usr/local/include/viceioctl.h.
 */

#ifndef	_SYS_VICEIOCTL_H_
#define _SYS_VICEIOCTL_H_

#include <sys/types.h>
#include <sys/ioctl.h>

struct ViceIoctl {
	caddr_t in, out;	/* Data to be transferred in, or out */
	short in_size;		/* Size of input buffer <= 2K */
	short out_size;		/* Maximum size of output buffer, <= 2K */
};

/* The 2K limits above are a consequence of the size of the kernel buffer
   used to buffer requests from the user to venus--2*MAXPATHLEN.
   The buffer pointers may be null, or the counts may be 0 if there
   are no input or output parameters
 */

#ifdef	__STDC__
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#else
#define _VICEIOCTL(id)  ((unsigned int ) _IOW(V, id, struct ViceIoctl))
#endif
/* Use this macro to define up to 256 vice ioctl's.  These ioctl's
   all potentially have in/out parameters--this depends upon the
   values in the ViceIoctl structure.  This structure is itself passed
   into the kernel by the normal ioctl parameter passing mechanism.
 */

#define _VALIDVICEIOCTL(com) (com >= _VICEIOCTL(0) && com <= _VICEIOCTL(255))

#endif	_SYS_VICEIOCTL_H_
