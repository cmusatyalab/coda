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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vol/recovc.cc,v 4.2 1997/02/26 16:03:54 rvb Exp $";
#endif /*_BLURB_*/








/*
 * recovc.c:
 * Routines for accessing volume abstractions in recoverable storage
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <struct.h>

#include <lwp.h>
#include <lock.h>

#ifdef __MACH__
#include <mach.h>
#endif /* __MACH__ */
#ifdef __cplusplus
}
#endif __cplusplus

#include <rvmlib.h>

#include <vice.h>
#include <callback.h>
#include <util.h>
#include <olist.h>
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"

extern olist *VolLogPtrs[MAXVOLS];
extern void dump_storage(int, char*);

PRIVATE void DeleteVolHeader(int);
PRIVATE void DeleteVolData(int);

unsigned long VMCounter = 0;
unsigned long startuptime = 0;

/* perform first-time initialization for coda recoverable storage */
/* Allocate vnodes and store them on the large and small vnode free lists. */
/* Hash table initialization is done is VInitVolumePackage */

#ifdef NOTDEF

NOTE: The Recovery Log stuff in RVM modifies data structures in RVM without
    the use of transaction because the data is transient. Thus we do not need
    transactions for it. This in turn means that checking vm via checkvm()
    will not work, since the state after Salvage *should not* match the dataseg.
    
extern int nodumpvm;
extern rvm_offset_t _Rvm_DataLength;

extern int rds_rvmsize;
extern char *rds_startaddr;
void checkvm() {

    int fd = open("/vicepa/dumpvm", 000, 0);
    if (fd < 1) {
	LogMsg(0, VolDebugLevel, stdout,  "Couldn't open dumpvm %d", errno);
	return;
    }

    char *p = rds_startaddr;
    char buf[4096];
    int errorsfound = 0;
    int readerror = 0;
    int i, j;
    for (i = 0, j = 4096; j < rds_rvmsize; i+= 4096, j+= 4096, p+= 4096) {
	if (read(fd, buf, 4096) != 4096) {
	    LogMsg(0, VolDebugLevel, stdout,  "Read failed i %d, err %d", i, errno);
	    readerror = 1;
	    break;
	}
	if (bcmp(buf, p, 4096) != 0) 
	    for (int k = 0; k < 4096; k += 4) 
		if (bcmp(&buf[k], &p[k], 4)) {
		    LogMsg(0, VolDebugLevel, stdout,  "CheckVM: Addr = 0x%x Dataseg value = 0x%x, Dumpfile value = 0x%x",
			&p[k], *((int *)&p[k]), (*(int *)&buf[k]));
		    errorsfound++;
		}
    }
    
    if (readerror && !errorsfound) return;

    int nbytes = rds_rvmsize - i;
    if (!readerror && nbytes) {
	if (read(fd, buf, nbytes) != nbytes) {
	    LogMsg(0, VolDebugLevel, stdout,  "Checkvm: Read failed address 0x%x size = %d",
		p, nbytes);
	    readerror = 1;
	}
	if (!readerror && bcmp(buf, p, nbytes) != 0) 
	    for (int k = 0; k < nbytes; k++) 
		if ((int)buf[k] != (int)p[k]) {
		    errorsfound++;
		    LogMsg(0, VolDebugLevel, stdout,  "CheckVM: Addr = 0x%x Dataseg value = 0x%x, dumpfile value = 0x%x", 
			&p[k], (int)p[k], (int)buf[k]);
		}
    }
    if (errorsfound) {
	LogMsg(0, VolDebugLevel, stdout,  "CheckVM: Number of errors = %d", errorsfound);
	assert(0);
    }
    close(fd);
}
#endif NOTDEF

int coda_init() {

    VnodeDiskObject *svnodes[SMALLFREESIZE];
    VnodeDiskObject *lvnodes[LARGEFREESIZE];
    char buf1[SIZEOF_SMALLDISKVNODE];
    char buf2[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *zerovnode;
    VolumeId volid = 0;
    int status = 0;	    // transaction status variable
    int i = 0;

    if (ThisServerId == -1) {
	LogMsg(0, VolDebugLevel, stdout,  "ThisServerId is uninitialized!!! Exiting.");
	assert(0);
    }

#ifdef NOTDEF
    if (CAMLIB_REC(already_initialized) == TRUE) {
	if (!nodumpvm)
	checkvm(); 
	unlink("/vicepa/dumpvm");
    }
#endif NOTDEF

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_OVNV_STANDARD)

    CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(already_initialized)), CLS, CAMLIB_LOCK_MODE_WRITE);

    if (CAMLIB_REC(already_initialized) == FALSE) {
	CAMLIB_MODIFY(CAMLIB_REC(already_initialized), TRUE);

	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(SmallVnodeFreeList)), CLS, CAMLIB_LOCK_MODE_WRITE);
	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(LargeVnodeFreeList)), CLS, CAMLIB_LOCK_MODE_WRITE);
	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(SmallVnodeIndex)), CLS, CAMLIB_LOCK_MODE_WRITE);
	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(LargeVnodeIndex)), CLS, CAMLIB_LOCK_MODE_WRITE);
	CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(MaxVolId)), CLS, CAMLIB_LOCK_MODE_WRITE);

	/* Initialize MaxVolId to zero but with the serverid  */
	/* in the high byte */
	CAMLIB_MODIFY(CAMLIB_REC(MaxVolId), (VolumeId)(ThisServerId << 24));
	LogMsg(29, VolDebugLevel, stdout,  "coda_init: MaxVolId = %x", CAMLIB_REC(MaxVolId));

	/* allocate vnodediskobject structures to fill the large and small */
	/* vnode free lists, and set freelist pointers */
	zerovnode = (VnodeDiskObject *)buf1;
	bzero(zerovnode, sizeof(buf1));
	for(i = 0; i < SMALLFREESIZE; i++) {
	    svnodes[i] = (VnodeDiskObject *)CAMLIB_REC_MALLOC(SIZEOF_SMALLDISKVNODE);
	    CAMLIB_MODIFY_BYTES(svnodes[i], zerovnode, sizeof(buf1));
	}
	CAMLIB_MODIFY_BYTES(CAMLIB_REC(SmallVnodeFreeList), svnodes, sizeof(svnodes));
	CAMLIB_MODIFY(CAMLIB_REC(SmallVnodeIndex), SMALLFREESIZE - 1);
	LogMsg(29, VolDebugLevel, stdout,  "Storing SmallVnodeIndex = %d", CAMLIB_REC(SmallVnodeIndex));

	zerovnode = (VnodeDiskObject *)buf2;
	bzero(zerovnode, sizeof(buf2));
	for(i = 0; i < LARGEFREESIZE; i++) {
	    lvnodes[i] = (VnodeDiskObject *)CAMLIB_REC_MALLOC(SIZEOF_LARGEDISKVNODE);
	    CAMLIB_MODIFY_BYTES(lvnodes[i], zerovnode, sizeof(buf2));
	}
	CAMLIB_MODIFY_BYTES(CAMLIB_REC(LargeVnodeFreeList), lvnodes, sizeof(lvnodes));
	CAMLIB_MODIFY(CAMLIB_REC(LargeVnodeIndex), LARGEFREESIZE - 1);
	LogMsg(29, VolDebugLevel, stdout,  "Storing LargeVnodeIndex = %d", CAMLIB_REC(LargeVnodeIndex));
    }

    dump_storage(49, "Finished coda initialization\n");
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status);
    assert(status == 0);	/* Should never abort. */
    if (!VMCounter) {
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv, &tz);
        startuptime = tv.tv_sec;
        VMCounter = 1;
    }
    return(0);
}

/* check the info in the VolumeData structure for validity, */
/* mainly to check for existence of VolumeDiskData */
void CheckVolData(Error *ec, int volindex) {
    VolumeData *data;
    int status = 0;	// transaction status variable
    VolumeId maxid;
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering CheckVolData for volindex %d", volindex);

    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "CheckVolData: bogus volume index %d", volindex);
	*ec = VNOVOL;	// invalid volume index
	return;
    }

    data = &CAMLIB_REC(VolumeList[volindex]).data;
    if (data->volumeInfo->stamp.magic != VOLUMEINFOMAGIC)
	LogMsg(0, VolDebugLevel, stdout,  "CheckVolumeData: bogus VolumeDiskData for volume %d, index %d!",
	    CAMLIB_REC(VolumeList[volindex]).header.id, volindex);
    assert(data->smallVnodeLists != NULL);
    assert(data->largeVnodeLists!= NULL);
}    


/* Returns the number of vnode slots available in the given class in the */
/* specified volume, or -1 if the volume index is invalid */
int ActiveVnodes(int volindex, int vclass) {
    bit32 vnodes;
    VolumeId maxid = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering ActiveVnodes for index %d, vclass = %d",
						volindex, vclass);
    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ActiveVnodes: bogus volume index %d", volindex);
	return(-1);	// invalid volume index
    }

    switch(vclass) {
	case vSmall:
		    vnodes = CAMLIB_REC(VolumeList[volindex]).data.nsmallLists;
		    break;
	case vLarge:
		    vnodes = CAMLIB_REC(VolumeList[volindex]).data.nlargeLists;
		    break;
	default:
		    LogMsg(0, VolDebugLevel, stdout,  "ActiveVnodes: bogus vnode type %d", vclass);
		    return(-1);	    // invalid vnode type
    }

    return ((int)vnodes);
}

/* Retruns the number of vnode slots in the array that */
/* are actually in use */
int AllocatedVnodes(int volindex, int vclass) {
    bit32 vnodes = 0;
    VolumeId maxid = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering AllocatedVnodes for index %d, vclass %d", 
					    volindex, vclass);
    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "AllocatedVnodes: bogus volume index %d", volindex);
	return(-1);	// invalid volume index
    }

    switch(vclass) {
	case vSmall:
		    vnodes = CAMLIB_REC(VolumeList[volindex]).data.nsmallvnodes;
		    break;
	case vLarge:
		    vnodes = CAMLIB_REC(VolumeList[volindex]).data.nlargevnodes;
		    break;
	default:
		    LogMsg(0, VolDebugLevel, stdout,  "AllocatedVnodes: bogus vnode type %d", vclass);
		    return(-1);	    // invalid vnode type
    }

    return ((int)vnodes);
}

/* Return the name of the physical partition containing the specified volume's */
/* data inodes. ec is set if the volume is not found */
void GetVolPartition(Error *ec, VolumeId volid, int myind, char partition[]) {
    VolumeDiskData *voldata;
    int status = 0;	// transaction status variable

    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering GetVolPartition for volid %x", volid);
    if ((myind < 0) || (myind > MAXVOLS) ||
	(CAMLIB_REC(VolumeList[myind]).header.id != volid)) {
	LogMsg(0, VolDebugLevel, stdout,  "GetVolPartition: bad index %d for volume %x", myind, volid);
	*ec = VNOVOL;
	return;
    }

    voldata = CAMLIB_REC(VolumeList[myind]).data.volumeInfo;
    strncpy(partition, voldata->partition, strlen(voldata->partition) + 1);
}

/* Increment and return the value of MaxVolId, the maximum volume id allocated */
/* on this server. Returns -1 if no more volumes can be allocated */
/* With recoverable storage, the check for server number can be left */
/* out, since that was basically a check for corrupted files. */
VolumeId VAllocateVolumeId(Error *ec) {
    int status = 0;	    /* transaction status variable */
    
    LogMsg(9, VolDebugLevel, stdout,  "Entering VAllocateVolumeId()");
    *ec = 0;
    CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(MaxVolId)), CLS,
						CAMLIB_LOCK_MODE_WRITE);

    if ((CAMLIB_REC(MaxVolId) & 0xFFFFFF) == 0xFFFFFF) {
	LogMsg(0, VolDebugLevel, stdout,  "VAllocateVolumeId: Out of volume numbers for this server; create aborted");
	*ec = VNOVOL;
	CAMLIB_ABORT(VFAIL);
    }
    else {
	unsigned long temp = CAMLIB_REC(MaxVolId) + 1;
	CAMLIB_MODIFY(CAMLIB_REC(MaxVolId), temp);
    }
    return(status?-1:CAMLIB_REC(MaxVolId));
}


/*
 * Return the highest used volume number
 */
VolumeId VGetMaxVolumeId()
{
    return (CAMLIB_REC(MaxVolId));
}


/*
 * Force a new volume MaxVolId.  Return 0 an error occurs or if the new 
 * volume id is lower than the current MaxVolId, otherwise, return 1.
 */
void VSetMaxVolumeId(VolumeId newid) {
    LogMsg(9, VolDebugLevel, stdout,  "Entering VSetMaxVolumeId ()");
    CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(MaxVolId)), CLS,
						CAMLIB_LOCK_MODE_WRITE);

    CAMLIB_MODIFY(CAMLIB_REC(MaxVolId), newid);
}


/*
 * Called whenever vnode bitmap grows to make sure that bitmap size never
 * exceeds vnode array size.
 */
void GrowVnodes(VolumeId volid, int vclass, short newBMsize) {
    rec_smolist *newvlist;
    int myind, newvnodes;
    bit32 cursize, newsize;
    int status = 0;		    // transaction status variable

    LogMsg(9, VolDebugLevel, stdout,  "Entering GrowVnodes for volid %x, vclass %d", volid, vclass);

    if ((myind = HashLookup(volid)) == -1) {
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: bogus volume id %x (not in hash table)", volid);
	assert(0);
    }

    newvnodes = newBMsize << 3;   // multiply by 8 since newBMsize is in bytes

    CAMLIB_LOCK(CAMLIB_LOCK_NAME(CAMLIB_REC(VolumeList[myind]).data),
					    CLS, CAMLIB_LOCK_MODE_WRITE);

    if (vclass == vSmall) {
	cursize = CAMLIB_REC(VolumeList[myind]).data.nsmallLists;
	newsize = cursize + SMALLGROWSIZE;
	if (newvnodes % SMALLGROWSIZE)
	    newvnodes += SMALLGROWSIZE - (newvnodes % SMALLGROWSIZE);
	if (newvnodes > newsize) newsize = newvnodes;
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: growing Small list from %d to %d for volume 0x%x",
					    cursize, newsize, volid);
	/* create a new larger list and zero out its tail */
	newvlist = (rec_smolist *) CAMLIB_REC_MALLOC(sizeof(rec_smolist) * newsize);

	char *tmpslist = (char *)malloc(sizeof(rec_smolist) * (int)(newsize - cursize));
	assert(tmpslist);
	bzero(tmpslist, sizeof(rec_smolist) * (int)(newsize - cursize));
	CAMLIB_MODIFY_BYTES(&(newvlist[cursize]), tmpslist,
			    sizeof(rec_smolist) * (newsize-cursize));
	free(tmpslist);
	
	/* copy the existing vnode pointers into the new list */
	CAMLIB_MODIFY_BYTES(newvlist,
		    CAMLIB_REC(VolumeList[myind]).data.smallVnodeLists,
		    cursize * sizeof(rec_smolist));
	CAMLIB_REC_FREE((char *)CAMLIB_REC(VolumeList[myind]).data.smallVnodeLists);
	/* copy the list pointer into the VolumeData structure */
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.smallVnodeLists,
								newvlist);
	/* update the list size in recoverable storage and cache */
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.nsmallLists,
								newsize);
    }

    else {	/* vclass == vLarge */
	cursize = CAMLIB_REC(VolumeList[myind]).data.nlargeLists;
	newsize = cursize + LARGEGROWSIZE;
	if (newvnodes % LARGEGROWSIZE)
	    newvnodes += LARGEGROWSIZE - (newvnodes % LARGEGROWSIZE);
	if (newvnodes > newsize) newsize = newvnodes;
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: growing Large list from %d to %d for volume 0x%x",
		cursize, newsize, volid);
	/* create a new larger list and zero out its tail */
	newvlist = (rec_smolist *)CAMLIB_REC_MALLOC(sizeof(rec_smolist) * newsize);

	char *tmpllist = (char *)malloc(sizeof(rec_smolist) * (int)(newsize - cursize));
	assert(tmpllist);
	bzero(tmpllist, sizeof(rec_smolist) * (int)(newsize - cursize));
	CAMLIB_MODIFY_BYTES(&(newvlist[cursize]), tmpllist, 
			    sizeof(rec_smolist) * (newsize - cursize));
	free(tmpllist);
	
	/* copy the existing vnode pointers into the new list */
	CAMLIB_MODIFY_BYTES(newvlist, 
			    CAMLIB_REC(VolumeList[myind]).data.largeVnodeLists,
			    cursize * sizeof(rec_smolist));
	CAMLIB_REC_FREE((char *)CAMLIB_REC(VolumeList[myind]).data.largeVnodeLists);
	/* copy the list pointer into the VolumeData structure */
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.largeVnodeLists,
		      newvlist);
	/* update the list size in recoverable storage and in cache */
	CAMLIB_MODIFY(CAMLIB_REC(VolumeList[myind]).data.nlargeLists,
		      newsize);

	/* grow the res log vm headers too */
	/* THERE IS A PROBLEM IF THIS TRANSACTION ABORTS */
	/* THIS IS ONLY TEMPORARY ANYWAY */
	if (AllowResolution && VolLogPtrs[myind]) {
	    olist *newrl = new olist[newsize];
	    bcopy(VolLogPtrs[myind], newrl, sizeof(olist) * (int)cursize);
	    /* DO NOT DELETE THIS BECAUSE the destructor deallocates the 
	       entire list;
	    delete[cursize] VolLogPtrs[myind];
	    THIS IS A MEMORY LEAK IN THE RESOLUTION SYSTEM 
	    */
	    VolLogPtrs[myind] = newrl;
	}
    }
}

/* Lookup volume disk info for specified volume */
void ExtractVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol) {
    int status = 0;	// transaction status variable
    VolumeId maxid = 0;
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering ExtractVolDiskInfo for volindex %u", volindex);

    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVolDiskInfo: bogus volume index %d", volindex);
	*ec = VNOVOL;	// invalid volume index
	return;
    }


    bcopy((char *)CAMLIB_REC(VolumeList[volindex]).data.volumeInfo, (char *)vol,
						    sizeof(VolumeDiskData));
    if (vol->stamp.magic != VOLUMEINFOMAGIC ||
		    vol->stamp.version != VOLUMEINFOVERSION) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVolDiskInfo: bogus version stamp!");
	*ec = VSALVAGE;
	LogMsg(0, VolDebugLevel, stdout,  "recoverable version stamp for volindex %x = %u, %u", volindex,
	    CAMLIB_REC(VolumeList[volindex]).data.volumeInfo->stamp.magic,
	    CAMLIB_REC(VolumeList[volindex]).data.volumeInfo->stamp.version);
    }
}

/* returns 1 if the slot is available, 0 if it's in use */
/* if Uniquifier parameter is 0 then check if entire slot is empty */
int AvailVnode(int volindex, int vclass, VnodeId vnodeindex, Unique_t u)
{
    VolumeId maxid = 0;
    rec_smolist *vlist;

    maxid = (CAMLIB_REC(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus volume index %d", volindex);
	return(0);
    }
    if (vclass == vSmall) {
	if (vnodeindex >= CAMLIB_REC(VolumeList[volindex]).data.nsmallLists) {
	    LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus small vnode index %d", vnodeindex);
	    return(0);
	}
	vlist = &(CAMLIB_REC(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
    }
    else if (vclass == vLarge) {
	if (vnodeindex >= CAMLIB_REC(VolumeList[volindex]).data.nlargeLists) {
	    LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus large vnode index %d", vnodeindex);
	    return(0);
	}
	vlist = &(CAMLIB_REC(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
    }
    else {
	return(0);
    }

    /* check the lists for vnode existence */
    if (u == 0) 
	return(vlist->IsEmpty());
    else {
	/* check if vnode matching uniquifier exists in list */
	rec_smolist_iterator next(*vlist);
	rec_smolink *p;
	while (p = next()) {
	    VnodeDiskObject *vdo;
	    vdo = strbase(VnodeDiskObject, p, nextvn);
	    if (vdo->uniquifier == u)
		return(0);
	}
	return(1);
    }
}


