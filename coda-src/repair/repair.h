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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/repair/repair.h,v 1.1.1.1 1996/11/22 19:12:37 rvb Exp";
#endif /*_BLURB_*/






/* 
    Repair data structures and routines used only on client.
    The server component of repair does not know of these.
    Data structures shared by client and server are defined in repio.{h,c}

    Created:
	M. Satyanarayanan
	October 1989
	
*/

#define MAXVOLNAME 100   /* length in bytes of OUT volume parameters */
#define MAXHOSTS 8  /* XXXX --- get the true definition for this */
/* #define MAXPATHLEN defined in <sys/param.h>, length in bytes of OUT path parameters */

/* Element in circular, doubly-linked list of replicated volumes in repair */
struct repvol
    {
    VolumeId vid;           /* id of this volume */
    struct repvol *next;    /* next element ptr */
    struct repvol *prev;    /* previous element */
    struct rwvol *rwhead;   /* Singly-linked list of rw replicas */
    char vname[MAXVOLNAME]; /* name of this volume */
    char mnt[MAXPATHLEN];   /* permanent mount point in Coda */
    char rodir[MAXPATHLEN]; /* directory where read-write replicas are mounted */
    char local;		    /* a flag indicating whether this is a local volume entry */
    };

/* Element in null-terminated, singly-linked list of
        rw replicas of a replicated volume */
struct rwvol
    {
    VolumeId vid;         /* id of this volume */
    struct rwvol *next;   /* next element ptr */
    char vname[MAXVOLNAME]; /* name of this volume */
    char srvname[64];      /* name of server on which this rw volume is located */
    char compname[MAXNAMLEN]; /* component name corresponding to this rw id */
    };


extern struct repvol *RepVolHead; /* head of circular linked list */
extern int NewStyleRepair;

/* Routines for volume data structure manipulation */
int repair_findrep   C_ARGS((VolumeId vid, struct repvol **repv));
int repair_newrep    C_ARGS((VolumeId vid, char *mnt, struct repvol **repv));
int repair_mountrw   C_ARGS((struct repvol *repv, VolumeId *rwarray, int arraylen));
int repair_linkrep   C_ARGS((struct repvol *repv));
int repair_unlinkrep C_ARGS((struct repvol *repv));
int repair_cleanup   C_ARGS((struct repvol *repv));
int repair_finish    C_ARGS((struct repvol *repv));
int repair_countRWReplicas C_ARGS((struct repvol *repv));
int repair_getfid(char *path, ViceFid *outfid, ViceVersionVector *outvv);

    
/* Routines for path processing */
int repair_isleftmost  C_ARGS((char *path, char *realpath));
int repair_getmnt      C_ARGS((char *realpath, char *prefix, char *suffix, VolumeId *vid));
int repair_inconflict  C_ARGS((char *name, ViceFid *conflictfid));
void myperror C_ARGS((char *op, char *path, int e));
int IsInCoda	C_ARGS((char *name));

/* User-visible commands */
int beginRepair  C_ARGS((char *args));
void endRepair   C_ARGS((char *args));
int showReplicas C_ARGS((char *args));
int doRepair     C_ARGS((char *args));
int compareDirs	 C_ARGS((char *args));
int clearInc	 C_ARGS((char *args));
void quit	 C_ARGS((char *args));
int unlockVolume C_ARGS((char *args));
int removeInc	 C_ARGS((char *args));
void checkLocal      	C_ARGS((char *args));
void listLocal       	C_ARGS((char *args));
void preserveLocal   	C_ARGS((char *args));
void preserveAllLocal   C_ARGS((char *args));
void discardLocal    	C_ARGS((char *args));
void discardAllLocal  	C_ARGS((char *args));
void setLocalView    	C_ARGS((char *args));
void setGlobalView   	C_ARGS((char *args));
void setMixedView    	C_ARGS((char *args));

extern char repair_ReadOnlyPrefix[];
extern int  repair_DebugFlag;

#define DEBUG(msg)  if (repair_DebugFlag) {printf msg; fflush(stdout);}

#define NOT_IN_SESSION		0
#define	LOCAL_GLOBAL		1
#define SERVER_SERVER		2
extern int session;


