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
 * recovc.c:
 * Routines for accessing volume abstractions in recoverable storage
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include "coda_string.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <struct.h>
#include <lwp.h>
#include <lock.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <vice.h>
#include <util.h>
#include <olist.h>
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"

extern void dump_storage(int, char*);

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
	CODA_ASSERT(0);
    }
    close(fd);
}
#endif NOTDEF

int coda_init() 
{
    VnodeDiskObject *svnodes[SMALLFREESIZE];
    VnodeDiskObject *lvnodes[LARGEFREESIZE];
    char buf1[SIZEOF_SMALLDISKVNODE];
    char buf2[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *zerovnode;
    rvm_return_t status = RVM_SUCCESS;
    int i = 0;

    if (ThisServerId == -1) {
	VLog(0, "ThisServerId is uninitialized!!! Exiting.");
	exit(1);
    }

    /* the VMCounter constant is used for generating strictly incrementing
       store id's during resolution. */
    if (!VMCounter) {
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv, &tz);
        startuptime = tv.tv_sec;
        VMCounter = 1;
    }

    if (SRV_RVM(already_initialized) == TRUE)
	    return 0;

    /* initialize RVM on a new server */
    rvmlib_begin_transaction(restore);

    RVMLIB_MODIFY(SRV_RVM(already_initialized), TRUE);

    /* Initialize MaxVolId to zero + (serverid << 24) */
    RVMLIB_MODIFY(SRV_RVM(MaxVolId), (VolumeId)(ThisServerId << 24));
    VLog(29, "coda_init: MaxVolId = %x", SRV_RVM(MaxVolId));

    /* allocate vnodediskobject structures to fill the large and small */
    /* vnode free lists, and set freelist pointers */
    zerovnode = (VnodeDiskObject *)buf1;
    bzero((void *)zerovnode, sizeof(buf1));
    for(i = 0; i < SMALLFREESIZE; i++) {
	    svnodes[i] = (VnodeDiskObject *)rvmlib_rec_malloc(SIZEOF_SMALLDISKVNODE);
	    rvmlib_modify_bytes(svnodes[i], zerovnode, sizeof(buf1));
    }
    rvmlib_modify_bytes(SRV_RVM(SmallVnodeFreeList), svnodes, sizeof(svnodes));
    RVMLIB_MODIFY(SRV_RVM(SmallVnodeIndex), SMALLFREESIZE - 1);
    VLog(29, "Storing SmallVnodeIndex = %d", SRV_RVM(SmallVnodeIndex));

    zerovnode = (VnodeDiskObject *)buf2;
    bzero((void *)zerovnode, sizeof(buf2));
    for(i = 0; i < LARGEFREESIZE; i++) {
	    lvnodes[i] = (VnodeDiskObject *)rvmlib_rec_malloc(SIZEOF_LARGEDISKVNODE);
	    rvmlib_modify_bytes(lvnodes[i], zerovnode, sizeof(buf2));
    }
    rvmlib_modify_bytes(SRV_RVM(LargeVnodeFreeList), lvnodes, sizeof(lvnodes));
    RVMLIB_MODIFY(SRV_RVM(LargeVnodeIndex), LARGEFREESIZE - 1);
    VLog(29, "Storing LargeVnodeIndex = %d", SRV_RVM(LargeVnodeIndex));

    dump_storage(49, "Finished coda initialization\n");
    rvmlib_end_transaction(flush, &(status));

    CODA_ASSERT(status == RVM_SUCCESS);	/* Should never abort. */
    return(0);
}

/* check the info in the VolumeData structure for validity, */
/* mainly to check for existence of VolumeDiskData */
void CheckVolData(Error *ec, int volindex) {
    VolumeData *data;
    VolumeId maxid;
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering CheckVolData for volindex %d", volindex);

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "CheckVolData: bogus volume index %d", volindex);
	*ec = VNOVOL;	// invalid volume index
	return;
    }

    data = &SRV_RVM(VolumeList[volindex]).data;
    if (data->volumeInfo->stamp.magic != VOLUMEINFOMAGIC)
	LogMsg(0, VolDebugLevel, stdout,  "CheckVolumeData: bogus VolumeDiskData for volume %d, index %d!",
	    SRV_RVM(VolumeList[volindex]).header.id, volindex);
    CODA_ASSERT(data->smallVnodeLists != NULL);
    CODA_ASSERT(data->largeVnodeLists!= NULL);
}    


/* Returns the number of vnode slots available in the given class in the */
/* specified volume, or -1 if the volume index is invalid */
int ActiveVnodes(int volindex, int vclass) {
    bit32 vnodes;
    VolumeId maxid = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering ActiveVnodes for index %d, vclass = %d",
						volindex, vclass);
    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ActiveVnodes: bogus volume index %d", volindex);
	return(-1);	// invalid volume index
    }

    switch(vclass) {
	case vSmall:
		    vnodes = SRV_RVM(VolumeList[volindex]).data.nsmallLists;
		    break;
	case vLarge:
		    vnodes = SRV_RVM(VolumeList[volindex]).data.nlargeLists;
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
    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "AllocatedVnodes: bogus volume index %d", volindex);
	return(-1);	// invalid volume index
    }

    switch(vclass) {
	case vSmall:
		    vnodes = SRV_RVM(VolumeList[volindex]).data.nsmallvnodes;
		    break;
	case vLarge:
		    vnodes = SRV_RVM(VolumeList[volindex]).data.nlargevnodes;
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

    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering GetVolPartition for volid %x", volid);
    if ((myind < 0) || (myind > MAXVOLS) ||
	(SRV_RVM(VolumeList[myind]).header.id != volid)) {
	LogMsg(0, VolDebugLevel, stdout,  "GetVolPartition: bad index %d for volume %x", myind, volid);
	*ec = VNOVOL;
	return;
    }

    voldata = SRV_RVM(VolumeList[myind]).data.volumeInfo;
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

    if ((SRV_RVM(MaxVolId) & 0xFFFFFF) == 0xFFFFFF) {
	LogMsg(0, VolDebugLevel, stdout,  "VAllocateVolumeId: Out of volume numbers for this server; create aborted");
	*ec = VNOVOL;
	rvmlib_abort(VFAIL);
    }
    else {
	unsigned long temp = SRV_RVM(MaxVolId) + 1;
	RVMLIB_MODIFY(SRV_RVM(MaxVolId), temp);
    }
    return(status?(long unsigned int)-1:SRV_RVM(MaxVolId));
}


/*
 * Return the highest used volume number
 */
VolumeId VGetMaxVolumeId()
{
    return (SRV_RVM(MaxVolId));
}


/*
 * Force a new volume MaxVolId.  Return 0 an error occurs or if the new 
 * volume id is lower than the current MaxVolId, otherwise, return 1.
 */
void VSetMaxVolumeId(VolumeId newid) {
    LogMsg(9, VolDebugLevel, stdout,  "Entering VSetMaxVolumeId ()");

    RVMLIB_MODIFY(SRV_RVM(MaxVolId), newid);
}


/*
 * Called whenever vnode bitmap grows to make sure that bitmap size never
 * exceeds vnode array size.
 */
void GrowVnodes(VolumeId volid, int vclass, short newBMsize) 
{
    rec_smolist *newvlist;
    int myind, newvnodes;
    bit32 cursize, newsize;

    LogMsg(9, VolDebugLevel, stdout,  "Entering GrowVnodes for volid %x, vclass %d", volid, vclass);

    if ((myind = HashLookup(volid)) == -1) {
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: bogus volume id %x (not in hash table)", volid);
	CODA_ASSERT(0);
    }

    newvnodes = newBMsize << 3;   // multiply by 8 since newBMsize is in bytes


    if (vclass == vSmall) {
	cursize = SRV_RVM(VolumeList[myind]).data.nsmallLists;
	newsize = cursize + SMALLGROWSIZE;
	if (newvnodes % SMALLGROWSIZE)
	    newvnodes += SMALLGROWSIZE - (newvnodes % SMALLGROWSIZE);
	if (newvnodes > (int)newsize) newsize = newvnodes;
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: growing Small list from %d to %d for volume 0x%x",
					    cursize, newsize, volid);
	/* create a new larger list and zero out its tail */
	newvlist = (rec_smolist *) rvmlib_rec_malloc(sizeof(rec_smolist) * newsize);

	char *tmpslist = (char *)malloc(sizeof(rec_smolist) * (int)(newsize - cursize));
	CODA_ASSERT(tmpslist);
	bzero(tmpslist, sizeof(rec_smolist) * (int)(newsize - cursize));
	rvmlib_modify_bytes(&(newvlist[cursize]), tmpslist,
			    sizeof(rec_smolist) * (newsize-cursize));
	free(tmpslist);
	
	/* copy the existing vnode pointers into the new list */
	rvmlib_modify_bytes(newvlist,
		    SRV_RVM(VolumeList[myind]).data.smallVnodeLists,
		    cursize * sizeof(rec_smolist));
	rvmlib_rec_free((char *)SRV_RVM(VolumeList[myind]).data.smallVnodeLists);
	/* copy the list pointer into the VolumeData structure */
	RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.smallVnodeLists,
								newvlist);
	/* update the list size in recoverable storage and cache */
	RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.nsmallLists,
								newsize);
    }

    else {	/* vclass == vLarge */
	cursize = SRV_RVM(VolumeList[myind]).data.nlargeLists;
	newsize = cursize + LARGEGROWSIZE;
	if (newvnodes % LARGEGROWSIZE)
	    newvnodes += LARGEGROWSIZE - (newvnodes % LARGEGROWSIZE);
	if (newvnodes > (int)newsize) newsize = newvnodes;
	LogMsg(0, VolDebugLevel, stdout,  "GrowVnodes: growing Large list from %d to %d for volume 0x%x",
		cursize, newsize, volid);
	/* create a new larger list and zero out its tail */
	newvlist = (rec_smolist *)rvmlib_rec_malloc(sizeof(rec_smolist) * newsize);

	char *tmpllist = (char *)malloc(sizeof(rec_smolist) * (int)(newsize - cursize));
	CODA_ASSERT(tmpllist);
	bzero(tmpllist, sizeof(rec_smolist) * (int)(newsize - cursize));
	rvmlib_modify_bytes(&(newvlist[cursize]), tmpllist, 
			    sizeof(rec_smolist) * (newsize - cursize));
	free(tmpllist);
	
	/* copy the existing vnode pointers into the new list */
	rvmlib_modify_bytes(newvlist, 
			    SRV_RVM(VolumeList[myind]).data.largeVnodeLists,
			    cursize * sizeof(rec_smolist));
	rvmlib_rec_free((char *)SRV_RVM(VolumeList[myind]).data.largeVnodeLists);
	/* copy the list pointer into the VolumeData structure */
	RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.largeVnodeLists,
		      newvlist);
	/* update the list size in recoverable storage and in cache */
	RVMLIB_MODIFY(SRV_RVM(VolumeList[myind]).data.nlargeLists,
		      newsize);

    }
}

/* Lookup volume disk info for specified volume */
void ExtractVolDiskInfo(Error *ec, int volindex, VolumeDiskData *vol) {
    VolumeId maxid = 0;
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout,  "Entering ExtractVolDiskInfo for volindex %u", volindex);

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVolDiskInfo: bogus volume index %d", volindex);
	*ec = VNOVOL;	// invalid volume index
	return;
    }


    bcopy((char *)SRV_RVM(VolumeList[volindex]).data.volumeInfo, (char *)vol,
						    sizeof(VolumeDiskData));
    if (vol->stamp.magic != VOLUMEINFOMAGIC ||
		    vol->stamp.version != VOLUMEINFOVERSION) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVolDiskInfo: bogus version stamp!");
	*ec = VSALVAGE;
	LogMsg(0, VolDebugLevel, stdout,  "recoverable version stamp for volindex %x = %u, %u", volindex,
	    SRV_RVM(VolumeList[volindex]).data.volumeInfo->stamp.magic,
	    SRV_RVM(VolumeList[volindex]).data.volumeInfo->stamp.version);
    }
}

/* returns 1 if the slot is available, 0 if it's in use */
/* if Uniquifier parameter is 0 then check if entire slot is empty */
int AvailVnode(int volindex, int vclass, VnodeId vnodeindex, Unique_t u)
{
    VolumeId maxid = 0;
    rec_smolist *vlist;

    maxid = (SRV_RVM(MaxVolId) & 0x00FFFFFF);
    if (volindex < 0 || volindex > (int)maxid || volindex > MAXVOLS) {
	LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus volume index %d", volindex);
	return(0);
    }
    if (vclass == vSmall) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nsmallLists) {
	    LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus small vnode index %d", vnodeindex);
	    return(0);
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.smallVnodeLists[vnodeindex]);
    }
    else if (vclass == vLarge) {
	if (vnodeindex >= SRV_RVM(VolumeList[volindex]).data.nlargeLists) {
	    LogMsg(0, VolDebugLevel, stdout,  "ExtractVnode: bogus large vnode index %d", vnodeindex);
	    return(0);
	}
	vlist = &(SRV_RVM(VolumeList[volindex]).data.largeVnodeLists[vnodeindex]);
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
	while ((p = next())) {
	    VnodeDiskObject *vdo;
	    vdo = strbase(VnodeDiskObject, p, nextvn);
	    if (vdo->uniquifier == u)
		return(0);
	}
	return(1);
    }
}


