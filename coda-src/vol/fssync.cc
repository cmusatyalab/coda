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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/fssync.cc,v 4.6 1998/10/21 22:23:51 braam Exp $";
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



/*
   fssync.c
   File server synchronization with volume utilities.
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include "voltypes.h"
#include "errors.h"
#include "fssync.h"
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"

#define USUAL_PRIORITY (LWP_MAX_PRIORITY - 2)

#define MAXUTILITIES	2	/* Up to 2 clients; must be at least
				   2, so that */
				/* move = dump+restore can run on
                                   single server */
				/* NOTE: this code assumes MAXUTILITES
                                   lwps are assigned */
				/* to handle volume utility requests */
#define MAXOFFLINEVOLUMES 30	/* This needs to be as big as the
				   maximum number that would be
				   offline for 1 operation.  Current
				   winner is salvage, which needs all
				   cloned read-only copies offline
				   when salvaging a single read-write
				   volume */

#define MAXRELOCATIONS	20	/* Maximum number of volume relocations that may be
				   extant at any time */
#define REDIRECT_TIME	60	/* Volumes are redirected for 60 minutes */
static int nRelocations = 0;	/* Actual number of volume relocations */

static struct relocation {
    VolumeId	vid;		/* volume to be relocated */
    long	server;		/* Internet address of server it has moved to */
    long	time;		/* Time of relocation--used to age the entry */
} Relocations[MAXRELOCATIONS];

static VolumeId OfflineVolumes[MAXUTILITIES][MAXOFFLINEVOLUMES];

int FSYNC_clientInit();
void FSYNC_clientFinis();
int FSYNC_askfs(VolumeId volume, int com, int reason);
void FSYNC_fsInit();
unsigned int FSYNC_CheckRelocationSite(VolumeId volumeId);

static void FSYNC_sync();
static void FSYNC_SetRelocationSite(VolumeId volumeId, int server);
static void FSYNC_DeleteRelocations(int nMinutes);
static void InitUtilities();
static int AddUtility (int myid);
static int FindUtility (int myid);
static int RemoveUtility (int myid);

/* File server synchronization initialization. Starts up an lwp to
   watch over the synchronization. This is bogus, since now that it
   doesn't have to listen on a socket this is only performing a timer
   function */
void FSYNC_fsInit() {
    PROCESS pid;
    long rc;

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_fsInit(), creating LWP");
    rc = LWP_CreateProcess((PFIC)FSYNC_sync, 5*1024, USUAL_PRIORITY,
					    0, "FSYNC_sync", &pid);
    CODA_ASSERT (rc == LWP_SUCCESS);
}

/* Wake up periodically to delete outdated relocation information */
static void FSYNC_sync() {

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_sync()");
    while (!VInit)	// Wait for fileserver initialization to complete
        LWP_DispatchProcess();
    InitUtilities();
    while (1) {
	struct timeval *timep, timeout;
	timep = 0;
        if (nRelocations) {
	    FSYNC_DeleteRelocations(REDIRECT_TIME);
	    if (nRelocations) {
		timeout.tv_sec = 10*60;
		timeout.tv_usec = 0;
		timep = &timeout;
	    }
	}
	/* Note: this call is just being used as a timer */
        CODA_ASSERT(IOMGR_Select(0, NULL, 0, 0, timep) == 0);
    }
}


/* Called by volume utility to initiate a dialogue with the file server */
/* (combination of old clientInit and newconnection routines) */
int FSYNC_clientInit() {

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_clientInit()");
    return(AddUtility(LWP_Index()));
}


/* Called by volume utility to terminate a dialogue with the file server */
void FSYNC_clientFinis() {

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_clientFinis()");
    RemoveUtility(LWP_Index());
}

/* Main synchronization routine for volume utilities and file server */
/* Called by the volume utilites to make file server requests */
int FSYNC_askfs(VolumeId volume, int command, int reason) {

    byte rc = FSYNC_OK;
    int i;
    Error error;
    register VolumeId *volumes, *v;
    Volume *vp;

    LogMsg(9, VolDebugLevel, stdout, "Entering FSYNC_askfs(%x, %d,%d)", 
	   volume, command, reason);

    volumes = OfflineVolumes[FindUtility(LWP_Index())];
    for (v = 0, i = 0; i<MAXOFFLINEVOLUMES; i++) {
	if (volumes[i] == volume) {
	    v = &volumes[i];
	    break;
	}
    }

    switch(command) {
	case FSYNC_ON:
	    if (v)
		*v = 0;
	    vp = VAttachVolume(&error, volume, V_UPDATE);
	    if (vp)
	        VPutVolume(vp);	    // save any changes
	    break;
	case FSYNC_OFF:
	case FSYNC_NEEDVOLUME: {
	    int leaveonline = 0;
	    if (!v) {
		for (i = 0; i<MAXOFFLINEVOLUMES; i++) {
		    if (volumes[i] == 0) {
			v = &volumes[i];
			break;
		    }
		}
	    }
	    if (!v) {
	        rc = FSYNC_DENIED;
	        break;
	    }
	    vp = VGetVolume(&error, volume);
	    if (vp) {
		int leaveonline = (command==FSYNC_NEEDVOLUME
			&& (reason==V_READONLY
		        || (!VolumeWriteable(vp) && (reason==V_CLONE || reason==V_DUMP))));

		if (!leaveonline) {
		    if (command==FSYNC_NEEDVOLUME
		        && (reason==V_CLONE || reason==V_DUMP)) {
			    vp->specialStatus = VBUSY;
			}
	            VOffline(vp, "A volume utility is running.");
		    vp = 0;
		}
		else {
		    VUpdateVolume(&error, vp);	/* At least get volume stats right */
		    if (VolDebugLevel) {
			LogMsg(0, VolDebugLevel, stdout,  "FSYNC: Volume %x (%s) was left on line for an external %s request",
			    V_id(vp), V_name(vp),
			    reason == V_CLONE? "clone":
			    reason == V_READONLY? "readonly":
			    reason == V_DUMP? "dump" : "UNKNOWN");
		    }
		}
		if (vp)
		    VPutVolume(vp);
	    }
	    if (!leaveonline)	/* This, too should be more sophisticated */
	        *v = volume;
	    rc = FSYNC_OK;
	    break;
	}
	case FSYNC_LISTVOLUMES:
	    VListVolumes();
	    break;
	case FSYNC_MOVEVOLUME:
	    /* Yuch:  the "reason" for the move is the site it got moved to;
	       sort of makes sense, doesn't it?? */
	    FSYNC_SetRelocationSite(volume, reason);
	    vp = VGetVolume(&error, volume);
	    if (vp) {
		vp->specialStatus = VMOVED;
		VPutVolume(vp);
	    }
	    break;
	default:
	    rc = FSYNC_DENIED;
	    break;
    }
    return(rc);
}



unsigned int FSYNC_CheckRelocationSite(VolumeId volumeId)
{
    register int i;

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_CheckRelocationSite(%x)", volumeId);
    for (i = nRelocations-1; i>=0; i--) /* Search backwards for most recent info */
	if (Relocations[i].vid == volumeId)
	    return Relocations[i].server;
    return 0;
}

static void FSYNC_SetRelocationSite(VolumeId volumeId, int server)
{
    register int nMinutes = REDIRECT_TIME;
    register struct relocation *rp;

    LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_SetRelocationSite(%x, %d)", 
				    volumeId, server);
    while (nRelocations == MAXRELOCATIONS) {
	FSYNC_DeleteRelocations(nMinutes);
	nMinutes -= 10;
    }
    rp = &Relocations[nRelocations++];
    rp->vid = volumeId;
    rp->server = server;
    rp->time = FT_ApproxTime();
    if (VolDebugLevel)
	LogMsg(0, VolDebugLevel, stdout,  "Volume %x is now relocated to server %x", volumeId, server);
}

static void FSYNC_DeleteRelocations(int nMinutes)
{
    long cutoff = FT_ApproxTime() - nMinutes*60;
    register int i, spread;

    LogMsg(9, VolDebugLevel, stdout,  "entering FSYNC_DeleteRelocations()");
    for (spread = i = 0; i<nRelocations; i++) {
	if (Relocations[i].time > cutoff) {
	    if (spread)
		Relocations[i-spread] = Relocations[i];
	}
	else {
	    LogMsg(0, VolDebugLevel, stdout,  "Dropping volume relocation, volume %x to server %x", 
		Relocations[i].vid, Relocations[i].server);
	    spread++;
	}
    }
    nRelocations -= spread;
}

/* Routines for managing per utility data */

static int UtilityId[MAXUTILITIES];

static void InitUtilities ()
{
    register int i;
    for(i=0;i<MAXUTILITIES;i++)
	UtilityId[i] = -1;
}
	
static int AddUtility (int myid)
{
    register int i;
    for(i=0;i<MAXUTILITIES;i++)
        if (UtilityId[i] < 0) break;
    if (i>=MAXUTILITIES) return 0;
    UtilityId[i] = myid;
    return 1;
}

static int FindUtility (register int myid)
{
    register int i;
    for(i=0;i<MAXUTILITIES;i++)
        if (UtilityId[i] == myid) return i;
    CODA_ASSERT(1 == 2);
    return -1;
}

static int RemoveUtility (register int myid)
{
    UtilityId[FindUtility(myid)] = -1;
    return 1;
}

