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

#include <lwp/lwp.h>
#include <lwp/lock.h>

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
void FSYNC_fsInit() 
{
	PROCESS pid;
	long rc;

	VLog(9, "Entering FSYNC_fsInit(), creating LWP");
	rc = LWP_CreateProcess((PFIC)FSYNC_sync, 5*1024, USUAL_PRIORITY,
			       0, "FSYNC_sync", &pid);
	CODA_ASSERT (rc == LWP_SUCCESS);
}

/* Wake up periodically to delete outdated relocation information */
static void FSYNC_sync() 
{

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
int FSYNC_clientInit() 
{

	LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_clientInit()");
	return(AddUtility(LWP_Index()));
}


/* Called by volume utility to terminate a dialogue with the file server */
void FSYNC_clientFinis() 
{

	LogMsg(9, VolDebugLevel, stdout,  "Entering FSYNC_clientFinis()");
	RemoveUtility(LWP_Index());
}

/* Main synchronization routine for volume utilities and file server */
/* Called by the volume utilites to make file server requests */
int FSYNC_askfs(VolumeId volume, int command, int reason) 
{
	byte rc = FSYNC_OK;
	int i;
	Error error;
	VolumeId *volumes = NULL;
	VolumeId *v = NULL;
	Volume *vp;

	VLog(9, "Entering FSYNC_askfs(%x, %d,%d)", volume, command, reason);

	volumes = OfflineVolumes[FindUtility(LWP_Index())];
	for (i = 0; i<MAXOFFLINEVOLUMES; i++) {
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
		/*    save any changes */
		if (vp)
			VPutVolume(vp);
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
					       || (!VolumeWriteable(vp) 
						   && (reason==V_CLONE 
						       || reason==V_DUMP))));

			if (!leaveonline) {
				if (command==FSYNC_NEEDVOLUME
				    && (reason==V_CLONE || reason==V_DUMP)) {
					vp->specialStatus = VBUSY;
				}
				VOffline(vp, "A volume utility is running.");
				vp = 0;
			} else {
				/* At least get volume stats right */
				VUpdateVolume(&error, vp);
				VLog(0, "FSYNC: Volume %x (%s) was left on line for an external %s request",
				     V_id(vp), V_name(vp),
				     reason == V_CLONE? "clone":
				     reason == V_READONLY? "readonly":
				     reason == V_DUMP? "dump" : "UNKNOWN");
			
			}
			if (vp)
				VPutVolume(vp);
		}
		/* This, too should be more sophisticated */
		if (!leaveonline)
			*v = volume;
		rc = FSYNC_OK;
		break;
	}
	case FSYNC_LISTVOLUMES:
		VListVolumes();
		break;
	case FSYNC_MOVEVOLUME:
		/* Yuck: the "reason" for the move is the site it got
		   moved to; sort of makes sense, doesn't it?? */
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
	int i;

	VLog(9, "Entering FSYNC_CheckRelocationSite(%x)", volumeId);
	/* Search backwards for most recent info */
	for (i = nRelocations-1; i>=0; i--)
		if (Relocations[i].vid == volumeId)
			return Relocations[i].server;
	return 0;
}

static void FSYNC_SetRelocationSite(VolumeId volumeId, int server)
{
	int nMinutes = REDIRECT_TIME;
	struct relocation *rp;

	VLog(9, "Entering FSYNC_SetRelocationSite(%x, %d)", 
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
		VLog(0, "Volume %x is now relocated to server %x", volumeId, server);
}

static void FSYNC_DeleteRelocations(int nMinutes)
{
    long cutoff = FT_ApproxTime() - nMinutes*60;
    register int i, spread;

    VLog(9, "entering FSYNC_DeleteRelocations()");
    for (spread = i = 0; i<nRelocations; i++) {
	if (Relocations[i].time > cutoff) {
	    if (spread)
		Relocations[i-spread] = Relocations[i];
	}
	else {
	    VLog(0, "Dropping volume relocation, volume %x to server %x", 
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

