#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/vol/RCS/coda_globals.h,v 4.1 1997/01/08 21:52:08 rvb Exp $";
#endif /*_BLURB_*/








/*
 * coda_globals.h -- Camelot header file for the CODA file server.
 */

#ifndef _CODA_GLOBALS_H_
#define _CODA_GLOBALS_H_ 1

/*
 * System Constant(s)
 */

/* Maximum number of volumes in recoverable storage (in any partitions) */
#define MAXVOLS		1024	    /* make this a power of 2 */
#define CLS  CAMLIB_LOCK_SPACE_PRIMARY	    /* lock name space for coda filesystem */
				  /* LOCK_SPACE_PRIMARY defined in camelot library */

/* size of large and small vnode free lists */
#define LARGEFREESIZE	MAXVOLS / 8
#define SMALLFREESIZE	MAXVOLS / 2

/* incremental growth of large and small vnode arrays */
#define LARGEGROWSIZE	128
#define SMALLGROWSIZE	256

/*
 * Recoverable Object Declarations
 */
typedef int boolean_t; /* defined in /usr/include/mach/machine/boolean.h on Mach machines */

CAMLIB_BEGIN_RECOVERABLE_DECLARATIONS

    /* flag to determine whether or not initialization is required */
    boolean_t	    already_initialized;

    /* Array of headers for all volumes on this server */
    struct VolHead VolumeList[MAXVOLS];

    /* Free list for VnodeDiskObject structures; prevents excessive */
    /* malloc/free calls */
    struct VnodeDiskObject    *SmallVnodeFreeList[SMALLFREESIZE];
    struct VnodeDiskObject    *LargeVnodeFreeList[LARGEFREESIZE];

    /* pointer to last index in free list containing available */
    /* vnodediskdata object */
    short    SmallVnodeIndex;
    short    LargeVnodeIndex;

    /*
      BEGIN_HTML
      <pre>
      <a name="MaxVolId"><strong>Maximum volume id allocated on this server</strong></a></pre>
      END_HTML
    */ 
    VolumeId    MaxVolId;

    long    Reserved[MAXVOLS];

CAMLIB_END_RECOVERABLE_DECLARATIONS


#endif _CODA_GLOBALS_H_
