/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


/*
 * coda_globals.h -- header file for the CODA file server.
 */

#ifndef _CODA_GLOBALS_H_
#define _CODA_GLOBALS_H_ 1

/*
 * System Constant(s)
 */

/* Maximum number of volumes in recoverable storage (in any partitions) */
#define MAXVOLS		1024	    /* make this a power of 2 */

/* size of large and small vnode free lists */
#define LARGEFREESIZE	MAXVOLS / 8
#define SMALLFREESIZE	MAXVOLS / 2

/* incremental growth of large and small vnode arrays */
#define LARGEGROWSIZE	128
#define SMALLGROWSIZE	256

/*
 * Recoverable Object Declarations
 */
typedef int bool_t;

struct camlib_recoverable_segment {
	/* flag to determine whether or not initialization is required */
	bool_t  already_initialized;

	/* Array of headers for all volumes on this server */
	struct VolHead VolumeList[MAXVOLS];

	/* Free list for VnodeDiskObject structures; prevents excessive */
	/* malloc/free calls */
	VnodeDiskObject    *SmallVnodeFreeList[SMALLFREESIZE];
	VnodeDiskObject    *LargeVnodeFreeList[LARGEFREESIZE];

	/* pointer to last index in free list containing available */
	/* vnodediskdata object */
	short    SmallVnodeIndex;
	short    LargeVnodeIndex;

	/* MaxVolId: Maximum volume id allocated on this server */
	VolumeId    MaxVolId;

	long    Reserved[MAXVOLS];
	int	camlibDummy;		
};

extern struct camlib_recoverable_segment *camlibRecoverableSegment;
#define SRV_RVM(name) \
    (((struct camlib_recoverable_segment *) (camlibRecoverableSegment))->name)

#endif /* _CODA_GLOBALS_H_ */
