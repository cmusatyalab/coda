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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/



/* movedb -- atomically updates a set of files into the current directory
	First flock()s the current directory (or specified lock file )
	For each file,  if its timestamp is different from that in the
	source directory, moves the current copy of the file into .BAK,
	then copies the new version in.
	Files are moved in the order in which they are specified in the
	command line.

	If ANY kind of error is found, processing is stopped immediately. 
*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/param.h>
#ifdef __BSD44__
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

extern int errno;
int VerboseFlag, ForceUpdates;

void BadArgs();

main(int argc, char *argv[])
    {
    register int i, rc,  pfd, firstarg;
    char *lockfile;
    char cmd[MAXPATHLEN+MAXPATHLEN+50];
    char ebuf[MAXPATHLEN];
    struct stat statbuf1, statbuf2;
    struct timeval tp[2];
    
    
    /* Obtain invocation options */
	/* lock the current directory by default */
    lockfile = (char *)getcwd(ebuf, MAXPATHLEN);
    if (argc < 2) BadArgs();

    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-l") == 0 && i < argc -1)
	    {
	    lockfile = argv[++i];
	    continue;
	    }
	if (strcmp(argv[i], "-u") == 0)
	    {
	    ForceUpdates++;
	    continue;
	    }
	if (strcmp(argv[i], "-v") == 0)
	    {
	    VerboseFlag++;
	    continue;
	    }
	if (argv[i][0] == '-') BadArgs();
	    
	break;	/* we have hit the first non-option */
	}
    firstarg = i;


    pfd = open(lockfile, O_RDONLY, 0);
#ifndef DJGPP
    if (pfd < 0 || flock(pfd, LOCK_EX) < 0)
	{
	perror(lockfile);
	exit(-1);
	}
#endif
	
    for (i = firstarg+1; i < argc; i++)
	{
	register char  *srcfile, *destfile;

	destfile = argv[i];
	if ((srcfile = (char *)index(argv[i], '=')) == NULL)   srcfile = destfile;
	else  *srcfile++ = 0;	/* it's ok, call by value! */

	sprintf(cmd, "%s/%s", argv[firstarg], srcfile);	/* stat the source file */
	if(::stat(cmd, &statbuf1) < 0)
	    {
	    perror(cmd);
	    break;
	    }

	rc = stat(destfile, &statbuf2);	/* check target exists before moving it to .BAK */
	if (rc < 0 && errno != ENOENT)
	    {
	    perror(destfile);
	    break;
	    }
	if (rc == 0 && ForceUpdates == 0 && statbuf1.st_mtime == statbuf2.st_mtime)
		continue;	/* don't update */

	if (rc == 0)
	    {/* target exists already; copy it to .BAK */
	    sprintf(cmd, "mv %s %s.BAK", destfile, destfile);
	    if (VerboseFlag) fprintf(stderr, "%s.......", cmd);
	    if ((rc = system(cmd)) != 0) break;
	    if (VerboseFlag) fprintf(stderr, "OK\n");
	    }

	sprintf(cmd, "cp %s/%s %s", argv[firstarg], srcfile, destfile);
	if (VerboseFlag) fprintf(stderr, "%s.......", cmd);
	if ((rc = system(cmd)) != 0) break;
	tp[0].tv_sec = statbuf1.st_atime;	
	tp[0].tv_usec = 0;
	tp[1].tv_sec = statbuf1.st_mtime;
	tp[1].tv_usec = 0;
	if (utimes(destfile, tp) < 0)
	    {
	    perror(destfile);
	    break;
	    }
	else close(pfd);
	if (VerboseFlag) fprintf(stderr, "OK\n");
	}

#ifndef DJGPP
	flock(pfd, LOCK_UN);	/* ignore error returns */
#endif
	close(pfd);
    }



void BadArgs()
    {
    printf("Usage: mvdb  [-l lockfile] [-u] [-v] srcdir f1 f2 ..... or\n");
    printf("       mvdb  [-l lockfile] [-u] [-v] srcdir f1'=f1 f2'=f2  .....\n");
    exit(-1);
    }
