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

#ifndef _REPCMDS_H_
#define _REPCMDS_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <inodeops.h>
#include <netinet/in.h>
#include <parser.h>
#include <rpc2/rpc2.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <auth2.h>
#include <avenus.h>
#include "coda_assert.h"
#include "coda_string.h"
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <inconsist.h>
#include <repio.h>
#include <resolve.h>
#include <venusioctl.h>

#define MAXVOLNAME     100   /* length in bytes of OUT volume parameters */
#define MAXHOSTS       8     /* XXXX --- get the true definition for this */
#define HOSTNAMLEN     64    /* XXXX -- get the true definition for this */
/* MAXPATHLEN defined in <sys/param.h>, length in bytes of OUT path parameters */
#define DEF_BUF        2048  /* XXXX -- temporary buffer size */

/* Replicated volume under repair */
struct repvol {
    VolumeId vid;           /* id of this volume */
    struct volrep *rwhead;  /* Singly-linked list of volume replicas */
    char vname[MAXVOLNAME]; /* name of this volume */
    char mnt[MAXPATHLEN];   /* permanent mount point in Coda */
    char rodir[MAXPATHLEN]; /* directory where replicas are mounted */
    char local;	  /* a flag indicating whether this is a local volume entry 
		   *  -- if true it's a local/global conflict, else server/server */
    char dirconf; /* a flag indicating whether this is a directory conflict
		   *  -- if true it's a directory conflict, else a file conflict */
};

/* Element in singly-linked list of volume replicas of a replicated volume */
struct volrep {
    VolumeId vid;             /* id of this volume */
    struct volrep *next;      /* next element ptr */
    char vname[MAXVOLNAME];   /* name of this volume */
    char srvname[HOSTNAMLEN]; /* name of server on which this rw volume is located */
    char compname[MAXNAMLEN]; /* component name corresponding to this rw id */
};

extern int  repair_DebugFlag;

/* Non-interactive repair calls */
extern int BeginRepair(char *pathname, struct repvol **repv, char *msg, int msgsize);
extern int ClearInc(struct repvol *repv, char *msg, int msgsize);
extern int DiscardAllLocal(struct repvol *repv, char *msg, int msgsize);
extern int EndRepair(struct repvol *repv, int commit, char *msg, int msgsize);
extern int RemoveInc(struct repvol *repv, char *msg, int msgsize);

/* Volume data structure manipulation routines -- rvol.cc */
extern int  repair_cleanup(struct repvol *repv);
extern struct repvol *repair_findrep(VolumeId vid);
extern void repair_finish(struct repvol *repv);
extern int  repair_getfid(char *path, ViceFid *outfid, ViceVersionVector *outvv);
extern int  repair_linkrep(struct repvol *repv);
extern int  repair_mountrw(struct repvol *repv, VolumeId *rwarray, int arraylen, char *msg, int msgsize);
extern int  repair_newrep(char *reppath, struct repvol **repv, char *msg, int msgsize);
extern int  repair_unlinkrep(struct repvol *repv);
extern int  srvstr(char *rwpath, char *retbuf, int size);

/* Path processing routines -- path.cc */
extern int  repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid);
extern int  repair_inconflict(char *name, ViceFid *conflictfid);
extern int  repair_isleftmost(char *path, char *realpath, int len);
extern void repair_perror(char *op, char *path, int e);

#define freeif(pointer)				\
do {						\
 if (pointer != NULL) {				\
   free(pointer);				\
   pointer = NULL;				\
 } 						\
} while (0)

#define strerr(str, len, msg...)		\
do {						\
  if (str != NULL) 				\
    snprintf(str, len, ##msg);			\
} while (0)

#endif
