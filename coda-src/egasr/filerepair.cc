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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/egasr/Attic/filerepair.cc,v 4.5 1998/10/07 20:29:43 rvb Exp $";
#endif /*_BLURB_*/




/* filerepair: Takes 2 filenames as input - an inconsistent file and a
   filename representing the new contents of the file.

   Replaces the inconsistent file with the latter file.  */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <venusioctl.h>
#include <vcrcommon.h>
#include <inconsist.h>

extern int path(char *, char *, char *);
extern int wildmat(char *text, char *pattern);

#ifdef __cplusplus
}
#endif __cplusplus

#define       ISDIR(vnode) ((vnode) & 1)  /* directory vnodesare odd */

int IsObjInc(char *name, ViceFid *fid) 
{
    int rc;
    char symval[MAXPATHLEN];
    struct stat statbuf;

    rc = stat(name, &statbuf);
    if ((rc == 0) || (errno != ENOENT)) return(0);
    
    /* is it a sym link */
    symval[0] = 0;
    rc = readlink(name, symval, MAXPATHLEN);
    if (rc < 0) return(0);
    
    /* it's a sym link, alright  */
    if (symval[0] == '@') {
	    sscanf(symval, "@%x.%x.%x",
		   &fid->Volume, &fid->Vnode, &fid->Unique);
	    return(1);
    } else 
	    return(0);
}

/* Returns 0 and fills outfid and outvv with fid and version vector
   for specified Coda path.  If version vector is not accessible,
   the StoreId fields of outvv are set to -1.
   Garbage may be copied into outvv for non-replicated files
   
   Returns -1 after printing error msg on failures. */

int getfid(char *path, ViceFid *outfid /* OUT */,
	   ViceVersionVector *outvv /* OUT */)

{
    int rc, saveerrno;
    struct ViceIoctl vi;
    char junk[2048];

    vi.in = 0;
    vi.in_size = 0;
    vi.out = junk;
    vi.out_size = (short) sizeof(junk);
    bzero(junk, (int) sizeof(junk));

    rc = pioctl(path, VIOC_GETFID, &vi, 0);
    saveerrno = errno;

    /* Easy: no conflicts */
    if (!rc)
    	{
	bcopy((const void *)junk, (void *)outfid, (int) sizeof(ViceFid));
	bcopy((const void *)junk+sizeof(ViceFid), (void *)outvv, (int)sizeof(ViceVersionVector));
	return(0);
	}

    /* if there are conflicts then can't use this object for the
       repair anyway.  A begin repair should have been done by this
       point. */
    return(-1);
}


void main(int argc, char **argv) 
{
	if (argc != 3) {
		fprintf(stderr, 
			"Usage: %s <inc-file-name> <merged-file-name>\n", 
			argv[0]);
		exit(-1);
	}
    

	/*  make sure repair file exists  */
	struct stat statbuf;
	int rc = stat(argv[2], &statbuf);
	if (rc != 0) {
		fprintf(stderr, "Couldn't find %s(errno = %d)\n", 
			argv[2], errno);
		exit(-1);
	}
	if (!(statbuf.st_mode & S_IFREG)) {
		fprintf(stderr, "File %s cannot be used for repair\n", argv[2]);
		exit(-1);
    }
	
	ViceFid fixfid;
	vv_t fixvv;
	char fixpath[MAXPATHLEN];
	if (!getfid(argv[2], &fixfid, &fixvv))
		sprintf(fixpath, "@%x.%x.%x", fixfid.Volume, fixfid.Vnode, fixfid.Unique);
	else 
		strcpy(fixpath, argv[2]);
	
	// do the repair 
	struct ViceIoctl vioc;
	char space[2048];
	vioc.in_size = (short)(1+strlen(fixpath));
	vioc.in = fixpath;
	vioc.out_size = (short)sizeof(space);
	vioc.out = space;
	bzero(space, sizeof(space));
	rc = pioctl(argv[1], VIOC_REPAIR, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) {
		fprintf(stderr, "Error %d for repair\n", errno);
		exit(-1);
	}
	
	if (stat(argv[1], &statbuf)) 
		exit(-1);
	exit(0);
}



