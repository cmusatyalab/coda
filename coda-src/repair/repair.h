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

#ifndef _REPAIR_H_
#define _REPAIR_H_

#include "repcmds.h"

#define NOT_IN_SESSION 0
#define	LOCAL_GLOBAL   1
#define SERVER_SERVER  2

extern struct repvol *RepairVol; /* volume under repair */
extern int allowclear, interactive, repair_DebugFlag, session;

void GetArgs(int argc, char *argv[]);
void INT(int, int, struct sigcontext *);

/* User-visible parser commands (possibly interactive) */
void rep_BeginRepair     (int argc, char **largv);
extern void rep_CheckLocal      (int argc, char **largv);
void rep_ClearInc        (int argc, char **largv);
extern void rep_CompareDirs     (int argc, char **largv);
extern void rep_DiscardLocal    (int argc, char **largv);
void rep_DiscardAllLocal (int argc, char **largv);
extern void rep_DoRepair        (int argc, char **largv);
void rep_EndRepair       (int argc, char **largv);
void rep_Exit            (int argc, char **largv);
void rep_Help            (int argc, char **largv);
extern void rep_ListLocal       (int argc, char **largv);
extern void rep_PreserveLocal   (int argc, char **largv);
extern void rep_PreserveAllLocal(int argc, char **largv);
void rep_RemoveInc       (int argc, char **largv);
extern void rep_SetGlobalView   (int argc, char **largv);
extern void rep_SetLocalView    (int argc, char **largv);
extern void rep_SetMixedView    (int argc, char **largv);

/* Volume data structure manipulation routines -- rvol.cc */
extern int  repair_cleanup(struct repvol *repv);
extern int  repair_countRWReplicas (struct repvol *repv);
extern void repair_finish(struct repvol *repv);
extern int  repair_getfid(char *path, ViceFid *outfid, ViceVersionVector *outvv);
extern int  repair_mountrw(struct repvol *repv, VolumeId *rwarray, int arraylen, char *msg, int msgsize);
extern int  repair_newrep(VolumeId vid, char *mnt, struct repvol **repv);

/* Path processing routines -- path.cc */
extern int  repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid);
extern int  repair_inconflict(char *name, ViceFid *conflictfid);
extern int  repair_isleftmost(char *path, char *realpath, int len);
extern void repair_perror(char *op, char *path, int e);

#endif
