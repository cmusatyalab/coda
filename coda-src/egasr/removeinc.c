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
 * removeinc - removes an inconsistent file (after repairing with an empty file) 
 */ 
 
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "coda_string.h"
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

int main(int argc, char **argv) 
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



