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


/*****************************************
 * vutil.c			         *
 * Utility routines for offline programs *
 ****************************************/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <partition.h>
#include <viceinode.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include <recov_vollog.h>
#include "signal.h"
#include "vutil.h"
#include "recov.h"



/* Note:  the volume creation functions herein leave the destroyMe flag in the
   volume header ON:  this means that the volumes will not be attached by the
   file server and WILL BE DESTROYED the next time a system salvage 
   is performed 
*/ 

/* This must be called from within a transaction! */
/* VolumeId parentId; Should be the same as volumeId if volume is
   readwrite.  Type is the type of the volume we are creating */
Volume *VCreateVolume(Error *ec, char *partition, VolumeId volumeId, 
		      VolumeId parentId, VolumeId groupId, int type, 
		      int rvmlogsize) 
{
    VolumeDiskData vol;
    int volindex;
    struct VolumeHeader tempHeader;
    struct DiskPartition *dp;
    *ec = 0;

    /* is the partition name short enough */
    if ( strlen(partition) >= VPARTSIZE) {
        SLog(0, "VCreateVolume: partition name %s too long. Bailing out.\n", 
	       partition);
	*ec = ENAMETOOLONG;
	return NULL;
    }

    /* let's see if the partition is there before locking it; 
       if lock fails, we die */
    dp = DP_Get(partition);
    if ( dp == NULL ) {
	    SLog(0, "VCreateVolume: Cannot find partition %s. Bailing out.\n", 
		 partition);
	    *ec = ENXIO;
	    return NULL;
    }
    DP_LockPartition(partition);

    /* set up volume disk info */
    bzero((char *)&vol, sizeof (vol));
    vol.id = volumeId;
    vol.parentId = parentId;
    // This is always NULL unless volume is replicated.
    vol.groupId = groupId;
    sprintf(vol.partition, partition, strlen(partition) + 1);
    vol.destroyMe = DESTROY_ME;
    vol.copyDate = time(0);	/* The only date which really means when this
				   @i(instance) of this volume was created. 
				   Creation date does not mean this */
    if (AllowResolution && rvmlogsize) {
	LogMsg(1, SrvDebugLevel, stdout, "Creating log for volume\n");
	vol.log = new recov_vol_log(volumeId);
	CODA_ASSERT(vol.log);
	vol.ResOn = RVMRES;
	vol.maxlogentries = rvmlogsize;
    }

    /* set up volume header info */
    bzero((char *)&tempHeader, sizeof (tempHeader));
    tempHeader.id = vol.id;
    tempHeader.parent = vol.parentId;
    tempHeader.type = type;

    /* Find an empty slot in recoverable volume header array */
    if ((volindex = NewVolHeader(&tempHeader, ec)) == -1) {
        if (*ec == VVOLEXISTS) {
	    LogMsg(0, VolDebugLevel, stdout, 
		   "VCreateVolume: volume %x already exists!", vol.id);
	}
	else {
	    LogMsg(0, VolDebugLevel, stdout, 
		   "VCreateVolume: volume %x not created", vol.id);
	}
	return NULL;
    }

    /* Store new volume disk data object into new volume */
    NewVolDiskInfo(ec, volindex, &vol);
    if (*ec != 0){
	LogMsg(0, VolDebugLevel, stdout, 
	       "VCreateVolume: Unable to write volume %x; volume not created",
	       vol.id);
	*ec = VNOVOL;
	return NULL;
    }

    return (VAttachVolumeById(ec, partition, volumeId, V_SECRETLY));
}

void AssignVolumeName(register VolumeDiskData *vol, char *name, char *ext)
{
    register char *dot;
    strncpy(vol->name, name, VNAMESIZE-1);
    vol->name[VNAMESIZE-1] = '\0';
    dot = (char *) rindex(vol->name, '.');
    if (dot && (strcmp(dot,".backup") == 0 || strcmp(dot, ".readonly") == 0 || 
      strcmp(dot, ".restored") == 0))
        *dot = 0;
    if (ext)
	strncat(vol->name, ext, VNAMESIZE-1-strlen(vol->name));
}

/* This should not be called with a replicated volume */
void CopyVolumeHeader(VolumeDiskData *from, VolumeDiskData *to)
{
    /* The id, parentId, and groupId fields are not copied; these are inviolate--the to volume
       is assumed to have already been created.  The id's cannot be changed once
       creation has taken place, since they are embedded in the various inodes associated
       with the volume.  The copydate is also inviolate--it always reflects the time
       this volume was created (compare with the creation date--the creation date of
       a backup volume is the creation date of the original parent, because the backup
       is used to backup the parent volume). */
    Date_t copydate;
    VolumeId id, parent, group;
    id = to->id;
    parent = to->parentId;
    group = to->groupId;
    copydate = to->copyDate;
    bcopy((char *)from, (char *)to, sizeof(*from));
    to->id = id;
    to->parentId = parent;
    to->groupId = group;
    to->copyDate = copydate;
    to->destroyMe = DESTROY_ME;	/* Caller must always clear this!!! */
    to->stamp.magic = VOLUMEINFOMAGIC;
    to->stamp.version = VOLUMEINFOVERSION;
    to->log = NULL;
    to->ResOn = 0;
    to->maxlogentries = 0;
}

void ClearVolumeStats(register VolumeDiskData *vol)
{
    bzero((char *)vol->weekUse, sizeof(vol->weekUse));
    vol->dayUse = 0;
    vol->dayUseDate = 0;
}


