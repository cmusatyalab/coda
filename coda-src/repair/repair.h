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
    Repair data structures and routines used only on client.
    The server component of repair does not know of these.
    Data structures shared by client and server are defined in repio.{h,c}

    Created:
	M. Satyanarayanan
	October 1989
	
*/

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

#define MAXVOLNAME 100 /* length in bytes of OUT volume parameters */
#define MAXHOSTS 8     /* XXXX --- get the true definition for this */
#define HOSTNAMLEN 64  /* XXXX -- get the true definition for this */
/* MAXPATHLEN defined in <sys/param.h>, length in bytes of OUT path parameters */
#define NOT_IN_SESSION		0
#define	LOCAL_GLOBAL		1
#define SERVER_SERVER		2
#define DEF_BUF 2048 /* XXXX -- temporary buffer size */

/* Element in circular, doubly-linked list of replicated volumes in repair */
struct repvol {
  VolumeId vid;           /* id of this volume */
  struct repvol *next;
  struct repvol *prev;
  struct volrep *rwhead;  /* Singly-linked list of volume replicas */
  char vname[MAXVOLNAME]; /* name of this volume */
  char mnt[MAXPATHLEN];   /* permanent mount point in Coda */
  char rodir[MAXPATHLEN]; /* directory where replicas are mounted */
  char local;		  /* a flag indicating whether this is a local volume entry */
};

/* Element in null-terminated, singly-linked list of volume replicas of a replicated volume */
struct volrep {
  VolumeId vid;             /* id of this volume */
  struct volrep *next;      /* next element ptr */
  char vname[MAXVOLNAME];   /* name of this volume */
  char srvname[HOSTNAMLEN]; /* name of server on which this rw volume is located */
  char compname[MAXNAMLEN]; /* component name corresponding to this rw id */
};

extern struct repvol *RepVolHead; /* head of circular linked list */
extern int session;
extern char repair_ReadOnlyPrefix[];
extern int  repair_DebugFlag;

/* User-visible commands -- repair.cc */
extern int BeginRepair(char *userpath, struct repvol **repv);
extern void checkLocal(int argc, char **largv);
extern void clearInc(int argc, char **largv);
extern void compareDirs(int argc, char **largv);
extern void discardAllLocal(int argc, char **largv);
extern void discardLocal(int argc, char **largv);
extern void doRepair(int argc, char **largv);
extern int EndRepair(struct repvol *rvhead, int type, int commit);
extern void listLocal(int argc, char **largv);
extern void preserveLocal(int argc, char **largv);
extern void preserveAllLocal(int argc, char **largv);
extern void removeInc(int argc, char **largv);
extern void setGlobalView(int argc, char **largv);
extern void setLocalView(int argc, char **largv);
extern void setMixedView(int argc, char **largv);
extern int  showReplicas(char *args);
extern int  unlockVolume(char *args);
extern void rep_BeginRepair(int argc, char **largv);
extern void rep_Exit(int argc, char **largv);
extern void rep_EndRepair(int argc, char **largv);

/* Volume data structure manipulation routines -- rvol.cc */
extern int repair_cleanup(struct repvol *repv);
extern int repair_countRWReplicas (struct repvol *repv);
extern struct repvol *repair_findrep(VolumeId vid);
extern void repair_finish(struct repvol *repv);
extern int repair_getfid(char *path, ViceFid *outfid, ViceVersionVector *outvv);
extern int repair_linkrep(struct repvol *repv);
extern int repair_mountrw(struct repvol *repv, VolumeId *rwarray, int arraylen);
extern int repair_newrep(VolumeId vid, char *mnt, struct repvol **repv);
extern int repair_unlinkrep(struct repvol *repv);
    
/* Path processing routines -- path.cc */
extern int  repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid);
extern int  repair_inconflict(char *name, ViceFid *conflictfid);
extern int  repair_isleftmost(char *path, char *realpath, int len);
extern void repair_perror(char *op, char *path, int e);
