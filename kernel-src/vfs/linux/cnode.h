/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 * Modifications for Linux port by Peter Braam
 */

/* 
 * HISTORY
 * cnode.h,v
 * Revision 1.2  1996/01/02 16:57:26  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:53  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:23  bnoble
 * Branch for BSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:23  bnoble
 * Bump to major revision 3 to prepare for BSD port
 *
 * Revision 2.2  1994/12/06  13:39:18  dcs
 * Add a flag value to indicate a cnode was orphaned, e.g. the venus
 * that created it has exited. This will allow one to restart venus
 * even though some process may be cd'd into /coda.
 *
 * Revision 2.1  94/07/21  16:25:33  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 * 
 * Revision 1.2.7.1  94/06/16  11:26:02  raiff
 * Branch for release beta-16Jun1994_39118
 * 
 * Revision 1.2  92/10/27  17:58:41  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:53  mja
 * 	Picked up fixed #ifdef KERNEL. Also...
 * 
 * 	Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 	[91/02/09            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:27:24  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.4  90/05/31  17:02:16  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 * 
 */

/* edited for Linux */


#ifndef	_CNODE_H_
#define	_CNODE_H_

#include <sys/types.h>

#define CODA_CNODE_MAGIC        0x47114711

/* kernel definition of ViceFid */

/* defintion of cnode, which combines ViceFid with inode information */

struct cnode {
#ifdef KERNEL
        struct inode    *c_vnode;    /* coda_fs inode associated with cnode */
#else
        struct vnode    *c_vnode;    /* for use in Venus */
#endif
        u_short	         c_flags;     /* flags (see below) */
        ViceFid	         c_fid;	     /* file handle */
#ifdef	KERNEL
        int             c_magic;     /* to verify the data structure */
#ifdef	__linux__
        struct inode    *c_ovp;	     /* open vnode pointer */
#else
        struct vnode    *c_ovp;	     /* open vnode pointer */
#endif	/* __linux__ */
        struct inode    *c_psdev;    /*psdev associated with this filesystem*/
        u_short	        c_ocount;    /* count of openers */
        u_short         c_owrite;    /* count of open for write */
        struct coda_vattr c_vattr;     /* attributes */
        char            *c_symlink;  /* pointer to symbolic link */
        u_short         c_symlen;    /* length of symbolic link */
        struct cnode    *c_next;     /* next cnode in the cache */
#endif	KERNEL
        dev_t	        c_device;    /* associated vnode device */
        ino_t	        c_inode;     /* associated vnode inode "number?"?? */
        /*    LINKS;                     links if on BSD44 machine */ 
};

/* flags */
#define C_VATTR       0x1         /* Validity of vattr in the cnode */
#define C_SYMLINK     0x2         /* Validity of symlink pointer in the cnode */
#define VALID_VATTR(cp)          ((cp->c_flags) & C_VATTR)
#define VALID_SYMLINK(cp)        ((cp->c_flags) & C_SYMLINK)


#ifdef KERNEL
#define C_DYING	      0x4	  /* Set for outstanding cnodes from last venus (which died) */
#define IS_DYING(cp)		 ((cp->c_flags) & C_DYING)

#define CN_WANTED     0x8        /* Set if lock wanted */
#define CN_LOCKED     0x10       /* Set if lock held */
#define CN_UNMOUNTING 0X20       /* Set if unmounting */
#define IS_UNMOUNTING(cp)       ((cp)->c_flags & CN_UNMOUNTING)

#endif KERNEL

#endif	not _CNODE_H_

