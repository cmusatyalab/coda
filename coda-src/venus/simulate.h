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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/








/*
 *
 *    Specification of the Venus Simulation package.
 *
 */


#ifndef	_VENUS_SIM_H_
#define _VENUS_SIM_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifndef LINUX
#include <tracelib.h>
#else
#include "tracelib.h"
#endif

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

#include "fso.h"
#include "vproc.h"


const int DIR_SIZE = 3072;
const int SYMLINK_SIZE = 1024;


class simulator : public vproc {
  friend void Simulate();

  public:
    simulator();
    simulator(simulator&);	
    operator=(simulator&);
    virtual ~simulator();

    void main(void *);

    fsobj *GetFso(ViceFid *, int, ViceDataType, unsigned long, ViceFid *, char *);
    void PutFso(fsobj **);
    fsobj *CreateFso(ViceFid *, ViceDataType, ViceFid *, char *);
    fsobj *CreateFso(ViceFid *, ViceDataType, fsobj *, char *);

    void FlushObject(fsobj *);
    void TranslateFid(ViceFid *);
    void NameInsertion(ViceFid *, char *, fsobj *);
    void NameRemoval(fsobj *);
    void NameRemoval(fsobj *, char *, fsobj *);

    void InferStore(fsobj *, unsigned long);
    fsobj *InferCreate(ViceFid *, ViceDataType, ViceFid *, char *);
    void InferDelete(fsobj *);
    void InferNameRemoval(fsobj *, char *);
    void InferNameRemoval(fsobj *, char *, fsobj *);
    void InferOtherNameRemoval(fsobj *, fsobj *, char *);
    void InferRename(fsobj *, ViceFid *, char *);
    void InferLink(fsobj *, ViceFid *, char *);
    void InferCloses(fsobj *);
    void LogTmpRename(time_t, fsobj *, char *, fsobj *, fsobj *, char *, fsobj *);

    int CreatedObject(volent *, ViceFid *);
    int DirtyDirEntry(volent *, ViceFid *, char *);
    void OutputSimulationFiles(volent *, FILE *, FILE *);
    void UnperformNamingOperations(volent *);
    void UnperformInsertion(ViceFid *, char *, ViceFid *);
    void UnperformRemoval(ViceFid *, char *, ViceFid *);
    void OutputSkeletonFile(volent *, FILE *);
    void MarkAncestors(fsobj *);
    void Skeletize(fsobj *, char *, FILE *);
    void OutputReplayFile(volent *, FILE *);

    void ReperformInsertion(ViceFid *, char *, ViceFid *);
    void ReperformRemoval(ViceFid *, char *, ViceFid *);
    void GetPath(ViceFid *, char *, char * =0);
    int IsTmpFile(fsobj *);
};


extern int Simulating;
extern unsigned long SimTime;
extern char *SimInfilename;
extern char *SimOutfilename;
extern char *SimFilterfilename;
extern char *SimAtSys;
extern char *SimTmpFid;
extern unsigned long SimStartTime;
extern unsigned long SimEndTime;
extern dfs_header_t *recPtr;

extern void SimInit();
extern void SimExit();
extern void SimReport();
extern void Simulate();

#if __NetBSD__ || LINUX
/* (Satya, 8/12/96): these definitions are found in sys/inode.h in Mach.
   The NetBSD equivalent file (sys/mount.h) has none of these definitions;
   they are assumed by the dfstrace package and are used in tracelib.h.
   I've put these definitions here for now, to get on with the NetBSD port.
   
   Porting dfstrace to NetBSD will involve cleaning this up.
*/

#define	ITYPE_UFS	0
#define	ITYPE_NFS	1
#define	ITYPE_AFS	2
#define	ITYPE_BDEV	3
#define	ITYPE_SPEC	4
#define ITYPE_CFS	5
#endif __NetBSD__


#endif not _VENUS_SIM_H_
