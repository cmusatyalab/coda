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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/egasr/removeinc.cc,v 4.3 1998/01/10 18:37:05 braam Exp $";
#endif /*_BLURB_*/



/*
 * removeinc - removes an inconsistent file (after repairing with an empty file) 
 */ 
 
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#if !defined(__GLIBC__)
#include <libc.h>
#endif
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __BSD44_
#include <sys/dir.h>
#endif
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <venusioctl.h>
#include <vcrcommon.h>

extern int path(char *, char *, char *);
extern int wildmat(char *text, char *pattern);

#ifdef __cplusplus
}
#endif __cplusplus

#define       ISDIR(vnode) ((vnode) & 1)  /* directory vnodesare odd */

int IsObjInc(char *name, ViceFid *fid) {
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
	int rc = pioctl(name, VIOC_GETFID, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) {
	    /* fprintf(stderr, "Error %d for Getfid\n", errno); */
	    return(0);
	}
	bcopy((const void *)space, (void *)fid, (int) sizeof(ViceFid));
	bcopy((const void *)space+sizeof(ViceFid), (void *)&vv, (int) sizeof(ViceVersionVector));
	if (!ISDIR(fid->Vnode) && (statbuf.st_mode & S_IFDIR))
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
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <inc-file-name>\n", argv[0]);
		exit(-1);
	}
    
	//make sure object is inconsistent
	ViceFid fid;
	if (!IsObjInc(argv[1], &fid)) {
		/* fprintf(stderr, "%s isn't inconsistent\n", argv[1]); */
	}
    
	// get fid and make sure it is a file
	if (ISDIR(fid.Vnode)) {
		fprintf(stderr, "%s is a directory - must be removed manually\n");
		exit(-1);
	}
	
	// create an empty file /tmp/REPAIR.XXXXXX
	char tmpfname[80];
	int fd;
	strcpy(tmpfname, "/tmp/RMINC.XXXXXX");
	if ((fd = mkstemp(tmpfname)) < 0) {
		fprintf(stderr, "Couldn't create /tmp file\n");
		exit(-1);
	}
	close(fd);

	// dorepair on the fid with an empty file
	struct ViceIoctl vioc;
	char space[2048];
	vioc.in_size = (short)(1+strlen(tmpfname)); 
	vioc.in = tmpfname;
	vioc.out_size = (short) sizeof(space);
	vioc.out = space;
	bzero(space, (int) sizeof(space));
	int rc = pioctl(argv[1], VIOC_REPAIR, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) {
		fprintf(stderr, "Error %d for repair\n", errno);
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



