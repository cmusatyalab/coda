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
int repair_isleftmost(char *path, char *realpath, int len);
int repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid);
int repair_inconflict(char *name, ViceFid *conflictfid);
void repair_perror(char *op, char *path, int e);

/* User-visible commands */
void beginRepair  C_ARGS((int argc, char **largv));
void endRepair   C_ARGS((int argc, char **largv));
int showReplicas C_ARGS((char *args));
void doRepair     C_ARGS((int argc, char **largv));
void compareDirs C_ARGS((int argc, char **largv));
void clearInc	 C_ARGS((int argc, char **largv));
void quit	 C_ARGS((int argc, char **largv));
int unlockVolume C_ARGS((char *args));
void removeInc	 C_ARGS((int argc, char **largv));
void checkLocal      	C_ARGS((int argc, char **largv));
void listLocal       	C_ARGS((int argc, char **largv));
void preserveLocal   	C_ARGS((int argc, char **largv));
void preserveAllLocal   C_ARGS((int argc, char **largv));
void discardLocal    	C_ARGS((int argc, char **largv));
void discardAllLocal  	C_ARGS((int argc, char **largv));
void setLocalView    	C_ARGS((int argc, char **largv));
void setGlobalView   	C_ARGS((int argc, char **largv));
void setMixedView    	C_ARGS((int argc, char **largv));

extern char repair_ReadOnlyPrefix[];
extern int  repair_DebugFlag;

#define DEBUG(msg)  if (repair_DebugFlag) {printf msg; fflush(stdout);}

#define NOT_IN_SESSION		0
#define	LOCAL_GLOBAL		1
#define SERVER_SERVER		2
extern int session;


