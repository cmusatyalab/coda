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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/Attic/listinodes.cc,v 4.4 1997/10/23 19:25:55 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#define ITC	/* Required by inode.h */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>

#ifdef __MACH__
#include <sys/fs.h>
#include <sys/inode.h>
#include <sys/file.h>
#endif /* __MACH__ */

#ifdef	__linux__
#include <linux/fs.h>
#endif /* __linux__*/

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#include <inodeops.h>
#endif

#include <libc.h>
#include <sysent.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <viceinode.h>
#ifdef __cplusplus
}
#endif __cplusplus


#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <vutil.h>


/* Notice:  parts of this module have been cribbed from fsck.c */


PRIVATE char *partition;
extern Testing;

PRIVATE int bread(int fd, char *buf, daddr_t blk, long size);

int ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode)(struct ViceInodeInfo*, VolumeId), 
		   int judgeParam)
{
#ifdef __MACH__
   union {
       struct fs fs;
       char block[SBSIZE];
   } super;
   int tfd, pfd, i, c, e, bufsize;
   FILE *inodeFile = NULL;
   char testFile[50], dev[50], rdev[51], err[200];
   struct dinode *inodes = NULL, *einodes;

   LogMsg(9, VolDebugLevel, stdout, "Entering ListViceInodes(%s, %s, %s, 0x%x, %u)",
		    devname, mountedOn, resultFile, judgeInode, judgeParam);
   partition = mountedOn;
   sprintf(dev, "/dev/%s", devname);
   sprintf(rdev, "/dev/r%s", devname);

   /* Check that the file system is writeable (not mounted read-only) */
   sprintf(testFile, "%s/.....zzzzz.....", mountedOn);
   if ((tfd = open(testFile,O_WRONLY|O_CREAT,0)) == -1) {
       LogMsg(0, VolDebugLevel, stdout, "File system \"%s\" is not writeable (is it mounted read only?)",
       	    mountedOn);
       return -1;
   }
   close(tfd);
   unlink(testFile);
   
    sync(); sync(); sync();	/* replace sleep with extra sync's */

   pfd = open(rdev, O_RDONLY, 0);
   if (pfd == NULL) {
       sprintf(err, "Could not read device %s to get inode list\n", rdev);
       perror(err);
       return -1;
   }
   
   if (bread(pfd, super.block, SBLOCK, SBSIZE) == -1) {
       LogMsg(0, VolDebugLevel, stdout, "Unable to read superblock, paritition %s", partition);
       goto out;
   }
   
   /*
    * run a few consistency checks of the superblock
    * (Cribbed from fsck)
    */
   if (
      (super.fs.fs_magic != FS_MAGIC)
   || (super.fs.fs_ncg < 1)
   || (super.fs.fs_cpg < 1 || super.fs.fs_cpg > MAXCPG)
   || (super.fs.fs_ncg * super.fs.fs_cpg < super.fs.fs_ncyl ||
	(super.fs.fs_ncg - 1) * super.fs.fs_cpg >= super.fs.fs_ncyl)
   || (super.fs.fs_sbsize > SBSIZE)) {
       LogMsg(0, VolDebugLevel, stdout, "There's something wrong with the superblock for partition %s; run fsck", partition);
       goto out;
   }

   /* Run through the cylinder groups */
   inodeFile = fopen(resultFile, "w");
   if (inodeFile == NULL) {
       LogMsg(0, VolDebugLevel, stdout, "Unable to create inode description file %s", resultFile);
       goto out;
   }
   bufsize = super.fs.fs_ipg * sizeof(struct dinode);   
   inodes = (struct dinode *) malloc(bufsize);
   einodes = (struct dinode *) (((char *)inodes) + bufsize);
   if (inodes == NULL) {
       LogMsg(0, VolDebugLevel, stdout, "Unable to allocate enough memory to scan inodes; help!");
       goto out;
   }
   LogMsg(0, VolDebugLevel, stdout, "Scanning inodes on device %s...", rdev);
   for (c = 0; c < super.fs.fs_ncg; c++) {
	i = c*super.fs.fs_ipg; e = i+super.fs.fs_ipg;
	if (lseek(pfd, dbtob(fsbtodb(&super.fs,itod(&super.fs,i))), L_SET) == -1) {
	    LogMsg(0, VolDebugLevel, stdout, "Error reading inodes for partition %s; run fsck", partition);
	    goto out;
	}
	while (i<e) {
	    struct dinode *p;
	    if (read(pfd, (char *)inodes, bufsize) != bufsize) {
		LogMsg(0, VolDebugLevel, stdout, "Error reading inodes for partition %s; run fsck", partition);
		goto out;
	    }
	    for (p=inodes; p<einodes && i<e; i++,p++) {
		struct ViceInodeInfo info;
	        if (p->di_vicemagic == VICEMAGIC
		  && (p->di_mode&IFMT) == IFREG) {
	            info.inodeNumber = i;
	            info.byteCount = p->di_size;
	            info.linkCount = p->di_nlink;
	            info.u.param[0] = p->di_vicep1;
	            info.u.param[1] = p->di_vicep2;
	            info.u.param[2] = p->di_vicep3;
	            info.u.param[3] = p->di_vicep4;
		    if ((judgeInode != NULL) && (*judgeInode)(&info, judgeParam) == 0)
		        continue;
	            if (fwrite((char *)&info, sizeof info, 1, inodeFile) != 1) {
	                LogMsg(0, VolDebugLevel, stdout, "Error writing inode file for partition %s", partition);
		        goto out;
		    }
	        }
	    }
	}
   }
   close(pfd);
   fclose(inodeFile);
   free((char *)inodes);
   return 0;

out:
   close(pfd);
   if (inodeFile)
   	fclose(inodeFile);
   if (inodes)
        free((char *)inodes);
   return -1;
#else /* __MACH__ */
/* Abort on platforms to which this code has not been ported yet */

   LogMsg(0, VolDebugLevel, stdout, "Arrgh..... ListViceInodes() has not been ported yet!!!");
   assert(0);
#endif /* __MACH__ */
}


int bread(int fd, char *buf, daddr_t blk, long size)
{
#ifdef __MACH__
	if (lseek(fd, (long)dbtob(blk), L_SET) < 0
	  || read(fd, buf, size) != size) {
	     LogMsg(0, VolDebugLevel, stdout, "Unable to read block %d, partition %s", blk, partition);
		return -1;
	  }
	return 0;
#else /* __MACH__ */
/* Abort on platforms to which this code has not been ported yet */

   LogMsg(0, VolDebugLevel, stdout, "Arrgh..... bread() has not been ported yet!!!");
   assert(0);
#endif /* __MACH__ */
}

