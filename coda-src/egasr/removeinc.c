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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/egasr/removeinc.cc,v 4.9 1998/11/25 19:23:27 braam Exp $";
#endif /*_BLURB_*/



/*
 * removeinc - removes an inconsistent file (after repairing with an empty file) 
 */ 
 
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include "coda_assert.h"
#include <errno.h>
#include <venusioctl.h>
#include <vcrcommon.h>
#include <coda_assert.h>
#include <codadir.h>

extern int path(char *, char *, char *);
extern int wildmat(char *text, char *pattern);

#ifdef __cplusplus
}
#endif __cplusplus


int IsObjInc(char *name, ViceFid *fid) 
{
    int rc;
    char symval[MAXPATHLEN];
    struct stat statbuf;

    fid->Vnode = 0; fid->Unique = 0; fid->Volume = 0;

    // what if the begin repair has been done already 
    rc = stat(name, &statbuf);
    if (rc == 0) {
	struct ViceIoctl vioc;
	char space[2048];
        ViceVersionVector vv;
	vioc.in_size = (short) (1+strlen(name));
	vioc.in = name;
	vioc.out_size = (short) sizeof(space);
	vioc.out = space;
	bzero(space, (int) sizeof(space));
	rc = pioctl(name, VIOC_GETFID, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) {
	    /* fprintf(stderr, "Error %d for Getfid\n", errno); */
	    return(0);
	}
	bcopy((const void *)space, (void *)fid, (int) sizeof(ViceFid));
	bcopy((const void *)space+sizeof(ViceFid), (void *)&vv, (int) sizeof(ViceVersionVector));
	if (!ISDIR(*fid) && (statbuf.st_mode & S_IFDIR))
	    return(1);
	else if (vv.StoreId.Host == -1) 
	    return(1);
	else return(0);
    }
    
    // is it a sym link
    symval[0] = 0;
    rc = readlink(name, symval, MAXPATHLEN);
    if (rc < 0) return(0);
    
    // it's a sym link, alright 
    if (symval[0] == '@') 
	sscanf(symval, "@%x.%x.%x",
	       &fid->Volume, &fid->Vnode, &fid->Unique);
    return(1);
}

void main(int argc, char **argv) 
{
	ViceFid fid;
	char tmpfname[80];
	int fd;
	struct ViceIoctl vioc;
	char space[2048];
	int rc;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <inc-file-name>\n", argv[0]);
		exit(-1);
	}
    
	//make sure object is inconsistent
	if (!IsObjInc(argv[1], &fid)) {
		/* fprintf(stderr, "%s isn't inconsistent\n", argv[1]); */
	}
    
	// get fid and make sure it is a file
	if (ISDIR(fid) && !FID_IsLocalDir(&fid)) {
		fprintf(stderr, 
			"%s is a directory - must be removed manually\n", 
			argv[1]);
		exit(-1);
	}
	
	// create an empty file /tmp/REPAIR.XXXXXX
	strcpy(tmpfname, "/tmp/RMINC.XXXXXX");
	if ((fd = mkstemp(tmpfname)) < 0) {
		fprintf(stderr, "Couldn't create /tmp file\n");
		exit(-1);
	}
	close(fd);

	// dorepair on the fid with an empty file
	vioc.in_size = (short)(1+strlen(tmpfname)); 
	vioc.in = tmpfname;
	vioc.out_size = (short) sizeof(space);
	vioc.out = space;
	bzero(space, (int) sizeof(space));
	rc = pioctl(argv[1], VIOC_REPAIR, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) {
		fprintf(stderr, "Error %d for repair\n", errno);
		unlink(tmpfname);
		exit(-1);
	}
	unlink(tmpfname);
	
	// remove the repaired file 
	if (unlink(argv[1])) {
		fprintf(stderr, "Couldn't remove %s\n", argv[1]);
		exit(-1);
	}
	exit(0);
}



