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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef __BSD44__
#include <fstab.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "coda_string.h"
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include <partition.h>
#include <rvmlib.h>

#include <vice.h>
#include "coda_flock.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include "cvnode.h"
#include "volume.h"
#include "lockqueue.h"
#include <recov_vollog.h>
#include "vldb.h"
#include "srvsignal.h"
#include "vutil.h"
#include "fssync.h"
#include "index.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"
#include "volutil.h"

extern void InitLogStorage();
extern void print_VnodeDiskObject(VnodeDiskObject *);
extern int HashLookup(VolumeId);


/* Exported Variables */
char *ThisHost;		/* This machine's hostname */
int ThisServerId = -1;	/* this server id, as found in  /vice/db/servers */
bit32 HostAddress[N_SERVERIDS];	/* Assume host addresses are 32 bits */
int VInit;		/* Set to 1 when the volume package is initialized */
int HInit;		/* Set to 1 when the volid hash table is  initialized */
char *VSalvageMessage =	  /* Common message used when the volume goes off line */
"Files in this volume are currently unavailable; call operations";

/*
  VolumeHashTable: Hash table used to store pointers to the Volume structure
 */
#define VOLUME_BITMAP_GROWSIZE	16	/* bytes, => 128vnodes */
					/* Must be a multiple of 4 (1 word) !!*/
#define VOLUME_HASH_TABLE_SIZE 128	/* Must be a power of 2!! */
#define VOLUME_HASH(volumeId) (volumeId&(VOLUME_HASH_TABLE_SIZE-1))
static Volume *VolumeHashTable[VOLUME_HASH_TABLE_SIZE];

extern void dump_storage(int level, char *s);
extern void VBumpVolumeUsage(Volume *vp);
extern int VCheckVLDB();
extern int InSkipVolumeList(VolumeId, VolumeId *, int);
extern void InitVolTable(int); // Setup (volume id -> VolumeList index) hash table


void VAddToVolumeUpdateList(Error *ec, Volume *vp);

extern int nskipvols;
extern VolumeId *skipvolnums;

static int TimeZoneCorrection; /* Number of seconds west of GMT */
static int VolumeCacheCheck = 0;  /* Incremented everytime a volume
				     goes on line--used to stamp
				     volume headers and in-core
				     vnodes.  When the volume goes
				     on-line the vnode will be
				     invalidated */

static int VolumeCacheSize = 50, VolumeGets = 0, VolumeReplacements = 0;

static void VListVolume(FILE *file, Volume *vp);
static void VAppendVolume(Volume *vp);
static void WriteVolumeHeader(Error *ec, Volume *vp);
static Volume *attach2(Error *ec, char *path, struct VolumeHeader *header,
		       struct DiskPartition *dp);
static void GetBitmap(Error *ec, Volume *vp, VnodeClass vclass);
static void VAdjustVolumeStatistics(Volume *vp);
static void VScanUpdateList();
static int GetVolumeHeader(Volume *vp);
static int AvailVolumeHeader(register Volume *vp);
static void ReleaseVolumeHeader(struct volHeader *hd);
void FreeVolumeHeader(Volume *vp);
static void AddVolumeToHashTable(Volume *vp, int hashid);
void DeleteVolumeFromHashTable(Volume *vp);
static char *vicedir;


/* InitVolUtil has a problem right now - 
   It seems to get advisory locks on these files, but
   the volume utilities don't seem to release locks after 
   they are done.  Since this is going to be deleted  most probably
   in the redesign of the volume package, I just added the 
   close() calls right now.
*/

void Vol_Init_vicedir(char *viced)
{
	if (!vicedir) 
		vicedir = (char *)malloc(MAXPATHLEN);
	CODA_ASSERT(vicedir);
	strncpy(vicedir, viced, MAXPATHLEN);
}

/* not thread safe! */
char *Vol_vicefile(char *file)
{
	static char *volpath[2] = { NULL, NULL };
	static char  vpidx = 0;

	if (!volpath[0]) {
		volpath[0] = (char *)malloc(MAXPATHLEN);
		volpath[1] = (char *)malloc(MAXPATHLEN);
	}
	CODA_ASSERT(volpath[0]);
	if (!vicedir)
		CODA_ASSERT(0);
	if (!file)
		CODA_ASSERT(0);

	/* We need at least 2 static volpaths to be able to handle:
	 *   rename(Vol_vicefile(p1), Vol_vicefile(p2);
	 * the following indexing takes care of that. -JH */
	vpidx = (vpidx + 1) & 0x1;
	snprintf(volpath[vpidx], MAXPATHLEN, "%s/%s",  vicedir, file);

	return volpath[vpidx];
}

/* invoked by all volume utilities except full salvager */
int VInitVolUtil(ProgramType pt) 
{
	int fslock, fvlock;

	fslock = -1;
	fvlock = -1;

	VLog(9, "Entering VInitVolUtil");
	fslock = open(Vol_vicefile("vol/fs.lock"), O_CREAT|O_RDWR, 0666);
	CODA_ASSERT(fslock >= 0);
	fvlock = open (Vol_vicefile("vol/volutil.lock"), O_CREAT|O_RDWR, 0666);
	CODA_ASSERT(fvlock >= 0);

	if (pt != salvager) {
		/* wait until file server is initialized */
		if (VInit != 1) {
			VLog(0, "VInitVolUtil: File Server not initialized! Aborted");
			close(fslock);
			close(fvlock);
			return(VNOSERVER);
		}
		if (myflock(fvlock, MYFLOCK_SH, MYFLOCK_NB) != 0) {
			VLog(0, "VInitvolUtil: can't grab volume utility lock");
			close(fslock);
			close(fvlock);
			return(VFAIL);
		}

		if (!VConnectFS()) {
			VLog(0, "Unable to synchronize with file server; aborted");
			close(fslock);
			close(fvlock);
			return(VFAIL);
		}
	} else {  /* pt == salvager */
		VLog(9, "VInitVolUtil: getting exclusive locks");
		if (myflock(fslock, MYFLOCK_EX, MYFLOCK_NB) != 0) {
			VLog(0, "VInitVolUtil: File Server is running: can't run full salvage");
			close(fslock);
			close(fvlock);
			return(VFAIL);
		}

		if (myflock(fvlock, MYFLOCK_EX, MYFLOCK_NB) != 0) {
			VLog(0, "VInitVolUtil: salvage aborted- someone else is running!");
			close(fslock);
			close(fvlock);
			return(VFAIL);
		}
	}

	close(fslock);
	close(fvlock);
	return(0);
}

/* one time initialization for file server only */
void VInitVolumePackage(int nLargeVnodes, int nSmallVnodes, int DoSalvage) 
{
    struct timeval tv;
    struct timezone tz;
    ProgramType *pt;

    VLog(9, "Entering VInitVolumePackage(%d, %d, %d)",
	nLargeVnodes, nSmallVnodes, DoSalvage);

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    srandom((int)time(0));	/* For VGetVolumeInfo */
    gettimeofday(&tv, &tz);
    TimeZoneCorrection = tz.tz_minuteswest*60;
    
    dump_storage(9, "InitVolumePackage");

    /* Don't grab the lock yet in case we run the salvager */
    /* NOTE: no concurrency is allowed until we grab the lock! */

    InitLRU(VolumeCacheSize);

    /* Setup (volume id -> VolumeList index) hash table */
    InitVolTable(HASHTABLESIZE);

    /* Initialize the volume hash tables */
    bzero((void *)VolumeHashTable, sizeof(VolumeHashTable));
    
    VInitVnodes(vLarge, nLargeVnodes);
    VInitVnodes(vSmall, nSmallVnodes);

    
    /* check VLDB */
    if (VCheckVLDB() == 0) {
	VLog(29, "VInitVolPackage: successfully finished checking VLDB");
    } else {
	VLog(0, "VInitVolPackage: no VLDB! Please create a new one.");
    }

    /* invoke salvager for full salvage */
    /* MUST set *pt to salvager before vol_salvage */
    *pt = salvager;	

    CODA_ASSERT(S_VolSalvage(0, NULL, 0, DoSalvage, 1, 0) == 0);

    *pt = fileServer;

    FSYNC_fsInit();

    /* Attach all valid volumes (from all vice partitions) */
    {
	Error error;
	Volume *vp;
	VolumeHeader header;
	char thispartition[64];
	int nAttached = 0, nUnattached = 0;
	int i = 0;
	rvm_return_t camstatus;
	int maxid = (int)(SRV_RVM(MaxVolId) & 0x00FFFFFF);

	rvmlib_begin_transaction(restore);
	for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	    if (VolHeaderByIndex(i, &header) == -1) {
		VLog(0, "Bogus volume index %d (shouldn't happen)", i);
		continue;
	    }

	    if (header.stamp.magic != VOLUMEHEADERMAGIC)
		continue;

	    /* Make sure volume is in the volid hashtable */
	    VLog(9, "VInitVolumePackage: hashing (vol,idx) (0x%x,%d)\n",
		 header.id, i);
	    if (HashInsert(header.id, i) == -1) {
		VLog(10, "VInitVolPackage: HashInsert failed! Two %x volumes exist!", header.id);
	    }
	    
	    GetVolPartition(&error, header.id, i, thispartition);
	    if (error != 0) 
		    continue;	    // bogus volume
	    vp = VAttachVolumeById(&error, thispartition, header.id, V_UPDATE);
	    (*(vp?&nAttached:&nUnattached))++;
	    if (error == VOFFLINE)
		VLog(0, "Volume %x stays offline (%s/%s exists)", 
		     header.id, Vol_vicefile("offline"), 
		     VolumeExternalName(header.id));
	    /* if volume was not salvaged force it offline. */
	    /* a volume is not salvaged if it exists in the 
		/"vicedir"/vol/skipsalvage file 
		*/
	    if (vp && skipvolnums != NULL && 
		InSkipVolumeList(header.parent, skipvolnums, nskipvols)){
		VLog(0, "Forcing Volume %x Offline", header.id);
		VForceOffline(vp);
	    } else if (vp) {
		    /* initialize the RVM log vm structures */
		    if (V_RVMResOn(vp)) {
			    V_VolLog(vp)->ResetTransients(V_id(vp));
			    extern olist ResStatsList;
			    ResStatsList.insert((olink *)V_VolLog(vp)->vmrstats);
		    }
	    }
	    if (vp)
		    VPutVolume(vp);
	}
	VLog(0, "Attached %d volumes; %d volumes not attached",
	     nAttached, nUnattached);
	VListVolumes(); 	/* Update list in /"vicedir"/vol/VolumeList */
	rvmlib_end_transaction(flush, &(camstatus));
    }

    VInit = 1;
}

/* This must be called by any volume utility which needs to run while the
   file server is also running.  This is separated from VInitVolumePackage so
   that a utility can fork--and each of the children can independently
   initialize communication with the file server */
int VConnectFS() {
    int rc;
    ProgramType *pt;

    VLog(9, "Entering VConnectFS");
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    CODA_ASSERT(VInit == 1 && *pt == volumeUtility);
    rc = FSYNC_clientInit();
    return rc;
}

void VDisconnectFS() {
    ProgramType *pt;

    VLog(9, "Entering VDisconnectFS");
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    CODA_ASSERT(VInit == 1 && *pt == volumeUtility);
    FSYNC_clientFinis();
}

void VInitThisHost(char *host)
{
	struct hostent *hostent;

	ThisHost = (char *)malloc(MAXHOSTNAMELEN);
	CODA_ASSERT(ThisHost);

	if ( !host ) {
		gethostname(ThisHost, MAXHOSTNAMELEN);
	} else {
		strncpy(ThisHost, host, MAXHOSTNAMELEN);
	}
	ThisServerId = -1;
	hostent = gethostbyname(ThisHost);

	if (hostent == NULL) {
		VLog(0, "Host %s cannot be resolved. Exit.", ThisHost);
		exit(1);
	}
}

/* must be called before calling VInitVolumePackage!! */
/* Find the server id */
void VInitServerList(char *host) 
{

    char hostname[100];
    char line[200];
    char *serverList = SERVERLISTPATH;
    FILE *file;

    VInitThisHost(host);

    VLog(9, "Entering VInitServerList");
    file = fopen(serverList, "r");
    if (file == NULL) {
	VLog(0, "VInitServerList: unable to read file %s; aborted", serverList);
	exit(1);
    }
    gethostname(hostname, sizeof(hostname)-1);
#ifdef __CYGWIN32__
    /* HACK --JJK */
    /* There should be a get_canonical_hostname routine! */
    {
	char *cp = hostname;
	while (*cp) {
	    *cp = tolower(*cp);
	    cp++;
	}
    }
#endif

    while (fgets(line, sizeof(line), file) != NULL) {
        char sname[50];
	struct hostent *hostent;
        int sid;
	if (sscanf(line, "%s%d", sname, &sid) == 2) {
	    if (sid > N_SERVERIDS) {
		VLog(0, "Host %s is assigned a bogus server number (%x) in %s. Exit.",
		  sname, sid, serverList);
		exit(1);
	    }
	    if (UtilHostEq(ThisHost, sname))
		ThisServerId = sid;
	    hostent = gethostbyname(sname);
	    if (hostent == NULL) {
		VLog(0, "Host %s (listed in %s) cannot be resolved. Exiting.", sname, serverList);
		exit(1);
	    } else {
		long netaddress;
		CODA_ASSERT(hostent->h_length == 4);
		bcopy((char *)hostent->h_addr, (char *)&netaddress, 4);
		HostAddress[sid] = ntohl(netaddress);
	    }
	}
    }
    if (ThisServerId == -1) {
	VLog(0, "Hostname of this server (%s) is not listed in %s. Exiting.", ThisHost, serverList);
	exit(1);
    }
    fclose(file);
}

void VCheckVolumes()
{
    VLog(9, "Entering VCheckVolumes()");
    VListVolumes();
    /* Note: removed body surrounded by "#ifdef undef"; recover from version 2.2*/
}

void VGetVolumeInfo(Error *ec, char *key, register VolumeInfo *info)
{
    register struct vldb *vldp;
    register VolumeId *vidp;
    register int i, nReported;
    bit32 *serverList;

    VLog(9, "Entering VGetVolumeInfo, key = %s", key);

    *ec = 0;
    bzero((void *)info, sizeof(VolumeInfo));
    vldp = VLDBLookup(key);
    if (vldp == NULL) {
	*ec = VNOVOL;
	VLog(9, "VGetVolumeInfo: VLDBLookup failed");
	return;
    }
    CODA_ASSERT(vldp->volumeType < MAXVOLTYPES);
    info->Vid = ntohl(vldp->volumeId[vldp->volumeType]);
    info->Type = vldp->volumeType;
    for (i = 0, vidp = &info->Type0; i<MAXVOLTYPES; )
	*vidp++ = ntohl((unsigned long) vldp->volumeId[i++]);
    CODA_ASSERT(vldp->nServers <= MAXVOLSERVERS);
    serverList = (bit32 *) &info->Server0;
    for (nReported = i = 0; i<vldp->nServers; i++) {
	register unsigned long serverAddress;
	serverAddress = HostAddress[vldp->serverNumber[i]];
	if (serverAddress)
	    serverList[nReported++] = serverAddress;
    }
    if (nReported == 0) {
	*ec = VNOVOL;
	VLog(9, "VGetVolumeInfo: no reported servers for volume %x",
			info->Vid);
	return;
    }
    /* Sort the servers into random order.  This is a good idea only if the
       number of servers is low.  After that, we'll have to figure out a better
       way to point a client at an appropriate server */
    for (i = nReported; i>1; ) {
        register bit32 temp;
        long s = random() % i;
	temp = serverList[s];
	for(i--; s<i; s++)
	    serverList[s] = serverList[s+1];
	serverList[i] = temp;
    }
    for (i = nReported; i<MAXVOLSERVERS; )
        serverList[i++] = 0;
    info->ServerCount = nReported;
    if (nReported == 1) {
	long movedto = FSYNC_CheckRelocationSite(info->Vid);
	if (movedto)
	    serverList[0] = movedto;
    }
    return;
}

static void VListVolume(register FILE *file, register Volume *vp)
{
    register int volumeusage, i;

    VLog(9, "Entering VListVolume for volume %x", V_id(vp));

    VAdjustVolumeStatistics(vp);
    for (volumeusage = i = 0; i<7; i++)
	volumeusage += V_weekUse(vp)[i];
    fprintf(file, "%c%s I%lx H%x P%s m%x M%x U%x W%lx C%lx D%lx B%lx A%x\n",
      V_type(vp) == readwriteVolume? 'W':V_type(vp) == readonlyVolume? 'R':
	V_type(vp) == backupVolume? 'B':'?',
      V_name(vp), V_id(vp), ThisServerId, vp->partition->name,
      V_minquota(vp), V_maxquota(vp), V_diskused(vp),
      V_parentId(vp), V_creationDate(vp), V_copyDate(vp), V_backupDate(vp),
      volumeusage
    );
}

void VListVolumes() 
{
    FILE *file;
    struct DiskPartition *part;
    struct dllist_head *tmp;
    int i;

    VLog(9, "Entering VListVolumes()");

    file = fopen(Vol_vicefile("vol/VolumeList.temp"), "w");
    CODA_ASSERT(file != NULL);

    tmp = &DiskPartitionList;
    while ( (tmp = tmp->next) != &DiskPartitionList) {
	    part = list_entry(tmp, struct DiskPartition, dp_chain);
	    fprintf(file, "P%s H%s T%x F%x\n",
		    part->name, ThisHost, part->totalUsable, part->free);
    }
    for (i=0; i<VOLUME_HASH_TABLE_SIZE; i++) {
	register Volume *vp, *tvp;
        Error error;
	vp = VolumeHashTable[i];
	while (vp) {
	    tvp = VGetVolume(&error, vp->hashid);
	    if (tvp) {
	        VListVolume(file,tvp);
		VPutVolume(tvp);
	    }
	    vp = vp->hashNext;
	}
    }
    fclose(file);
    rename(Vol_vicefile("vol/VolumeList.temp"),
	   Vol_vicefile("vol/VolumeList"));
}

static void VAppendVolume(Volume *vp)
{
    FILE *file;

    VLog(9, "Entering VAppendVolume for volume %x", V_id(vp));
    file = fopen(Vol_vicefile("vol/VolumeList"), "a");
    if (file == NULL)
        return;
    VListVolume(file, vp);
    fclose(file);
}

extern int nodumpvm;
extern rvm_offset_t _Rvm_DataLength;
extern long rds_rvmsize;
extern char *rds_startaddr;

void dumpvm()
{
    int i, j;
    
    int fd = open("/vicepa/dumpvm", O_TRUNC | O_WRONLY | O_CREAT, 0666);
    if (fd < 1) {
	VLog(0, "Couldn't open dumpvm %d", errno);
	return;
    }

    /* write out RVM */
    char *p = rds_startaddr;
    for (i = 0, j = 102400; j < rds_rvmsize; i+=102400, j += 102400, p+= 102400) {
	if (write(fd, (char *)p, 102400) != 102400) {
	    VLog(0, "Write failed i %d, err %d", i, errno);
	    return ;
	}
    }
    long nbytes = rds_rvmsize - i;
    if (nbytes) 
	if (write(fd, (char *)p, (int)nbytes) != nbytes) {
	    VLog(0, "Write failed for address 0x%x size %d", 
		p, nbytes);
	}

    close(fd);
}

void VShutdown() {
    int i;
    rvm_return_t camstatus;

    VLog(0, "VShutdown:  shutting down on-line volumes...");

    for (i=0; i<VOLUME_HASH_TABLE_SIZE; i++) {
        register Volume *vp, *p;
	p = VolumeHashTable[i];
	while (p) {
	    Error error;
	    rvmlib_begin_transaction(restore);
	    vp = VGetVolume(&error, p->hashid);
	    if ((error != 0) || (!vp)) {
		VLog(0, "VShutdown: Error %d getting volume %x!",error,p->hashid);
		rvmlib_abort(-1);
	    }
	    else
		VLog(0, "VShutdown: Taking volume %s(0x%x) offline...",
		     V_name(vp), V_id(vp));
	    if (vp)
	        VOffline(vp, "File server was shut down");
	    VLog(0, "... Done");
	    rvmlib_end_transaction(flush, &(camstatus));
	    p = p->hashNext;
	}
    }

    /* dump vm to a file so we can check for a recovery bug. */
    if (!nodumpvm) {
	/* check to see if there are any outstanding transactions. */
	if (RvmType == RAWIO || RvmType == UFS) {
	    rvm_options_t curopts;
	    int i;
	    rvm_return_t ret;
	    
	    rvm_init_options(&curopts);
	    ret = rvm_query(&curopts, NULL);
	    if (ret != RVM_SUCCESS)
		VLog(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));  
	    else {
		VLog(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	
		for (i = 0; i < curopts.n_uncommit; i++) {
		    rvm_abort_transaction(&(curopts.tid_array[i]));
		    if (ret != RVM_SUCCESS) 
			VLog(0, 0, stdout,
			       "ERROR: abort failed, code: %s", rvm_return(ret));
		}
	    
		ret = rvm_query(&curopts, NULL);
		if (ret != RVM_SUCCESS)
		    VLog(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));	
		else 
		    VLog(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	    }
	    rvm_free_options(&curopts);
	}

	dumpvm();
    }    
    VLog(0, "VShutdown:  complete.");
}


static void WriteVolumeHeader(Error *ec, Volume *vp)
{
	rvm_return_t status = RVM_SUCCESS;
	*ec = 0;

	VLog(9, "Entering WriteVolumeHeader for volume %x", V_id(vp));
	if ( rvmlib_in_transaction() ) 
		ReplaceVolDiskInfo(ec, V_volumeindex(vp), &V_disk(vp));
	else {
		rvmlib_begin_transaction(restore);
		ReplaceVolDiskInfo(ec, V_volumeindex(vp), &V_disk(vp));
		rvmlib_end_transaction(flush, &status);
	}

	if (*ec != 0 || status)
		*ec = VSALVAGE;
}

/*
 * Attach an existing volume, given its volume id, and return a
 * pointer to the volume header information.  The volume also
 * normally goes online at this time.  An offline volume
 * must be reattached to make it go online.
 * This must be called from within a transaction.
 */
Volume *
VAttachVolumeById(Error *ec, char *partition, VolumeId volid, int mode)
{
	register Volume *vp;
	int rc,listVolume = 0;
	struct VolumeHeader header;
	struct DiskPartition *dp;
	char name[32];
	ProgramType *pt;


	VLog(9, "Entering VAttachVolumeById() for volume %x", volid);
	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

	dp = DP_Get(partition);
	if (!dp) {
		*ec = VNOVOL;
		return NULL;
	}

	*ec = 0;
	if (*pt == volumeUtility) {
		VLog(19, "running as volume utility");
		CODA_ASSERT(VInit == 1);
		DP_LockPartition(partition);
	}
	if (*pt == fileServer) {
		VLog(19, "running as fileserver");
		vp = VGetVolume(ec, volid);
		if (vp) {
			if (V_inUse(vp)) {
				VLog(1, "VAttachVolumeById: volume %x already in use",
				     V_id(vp));
				return vp;
			}
			VDetachVolume(ec, vp);
			listVolume = 0;	    
		} else
			listVolume = 1;
	}
	*ec = 0;
    
	sprintf(name, VFORMAT, volid);
	if (ExtractVolHeader(volid, &header) != 0) {
		VLog(0, "VAttachVolumeById: Cannot read volume %s, part %s\n", 
		     name, partition);
		*ec = VNOVOL;
		return NULL;
	}

	if (header.stamp.magic != VOLUMEHEADERMAGIC) {
		VLog(0, "VAttachVolumeById: Error reading volume header for %s", name);
		*ec = VSALVAGE;
		return NULL;
	}
	if (header.stamp.version != VOLUMEHEADERVERSION) {
		VLog(0, "VAttachVolumeById: Volume %s, version number %u is incorrect; volume needs salvage", name, header.stamp.version);
		*ec = VSALVAGE;
		return NULL;
	}
	if (*pt == volumeUtility && mode != V_SECRETLY) {
		/* modify lwp program type for duration of FSYNC_askfs call */
		*pt = fileServer;
		rc = FSYNC_askfs(header.id, FSYNC_NEEDVOLUME, mode);
		*pt = volumeUtility;
		if (rc == FSYNC_DENIED) {
			VLog(0, "VAttachVolumeById: attach of volume %x apparently denied by file server",
			     header.id);
			*ec = VNOVOL; /* XXXX */
			return NULL;
		}
	}
	vp = attach2(ec, name, &header, dp);
	if (vp == NULL)
		VLog(9, "VAttachVolumeById: attach2 returns vp == NULL");

	if (*pt == volumeUtility && vp == NULL && mode != V_SECRETLY) {
		/* masquerade as fileserver for FSYNC_askfs call */
		*pt = fileServer;
		FSYNC_askfs(header.id, FSYNC_ON, 0);
		*pt = volumeUtility;
	}	
	else if (*pt == fileServer && vp) {
		VUpdateVolume(ec,vp);
		if (*ec) {
			if (vp)
				VPutVolume(vp);
			return NULL;
		}
		if (VolumeWriteable(vp) && V_dontSalvage(vp) == 0) {
			/* This is a hack: by temporarily settint the incore
			   dontSalvage flag ON, the volume will be put back on the
			   Update list (with dontSalvage OFF again).  It will then
			   come back in N minutes with DONT_SALVAGE eventually
			   set.  This is the way that volumes that have never had
			   it set get it set; or that volumes that have been
			   offline without DONT SALVAGE having been set also
			   eventually get it set */
			V_dontSalvage(vp) = DONT_SALVAGE;
			VAddToVolumeUpdateList(ec,vp);
			if (*ec) {
				if (vp)
					VPutVolume(vp);
				return NULL;
			}
		}
		VLog(0, "VAttachVolumeById: vol %x (%s) attached and online",
		     V_id(vp), V_name(vp));
		if (listVolume && VInit)
			VAppendVolume(vp);
	}
	VLog(29, "returning from VAttachVolumeById()");
	return vp;
}

static Volume *attach2(Error *ec, char *name, 
		       struct VolumeHeader *header,
		       struct DiskPartition *dp)
{
	Volume *vp;
	ProgramType *pt;

	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	VLog(9, "Entering attach2(); %s running as fileServer",
	     (*pt==fileServer)?"":"not");

	vp = (Volume *) calloc(1, sizeof(Volume));
	CODA_ASSERT(vp != NULL);

	vp->partition = dp;
	if (vp->partition == NULL) {
		FreeVolume(vp);
		return NULL;
	}
    
	vp->specialStatus = 0;
	vp->cacheCheck = ++VolumeCacheCheck;
	vp->shuttingDown = 0;
	vp->goingOffline = 0;
	vp->nUsers = 1;
	/* Initialize the volume level lock  for resolution/repair */
	V_VolLock(vp).IPAddress = 0;
	Lock_Init(&(V_VolLock(vp).VolumeLock));
	vp->nReintegrators = 0;	
	vp->reintegrators = NULL;	
	/* Initalize WriteBack flags */
	vp->WriteBackRevokeInProgress = 0;
	vp->WriteBackRevokeDone = 0;
	list_head_init(&vp->WriteBackHolders);

	GetVolumeHeader(vp);    /* get a VolHeader from LRU list */

	/* get the volume index and the VolumeDiskInfo from
           recoverable storage */
	vp ->vol_index = VolDiskInfoById(ec, header->id, &V_disk(vp));
	if (*ec) {
		VLog(0, "returned from VolDiskInfoById for id %x with *ec = %d",
		     header->id, *ec);
		VLog(0, "VAttachVolume: Error attaching volume %s; salvage volume!",
		     name);
		FreeVolume(vp);
		return NULL;
	}
    
	if (V_needsSalvaged(vp)) {
		VLog(0, "VAttachVolume: volume salvage flag is ON for %s; volume needs salvage", name);
		*ec = VSALVAGE;
		return NULL;
	}
	if (*pt == fileServer) {
		if (V_inUse(vp) && VolumeWriteable(vp)) {
			FreeVolume(vp);
			VLog(0, "VAttachVolume: volume %s needs to be salvaged; not attached.", name);
			*ec = VSALVAGE;
			return NULL;
		}
		if (V_destroyMe(vp) == DESTROY_ME) {
			FreeVolume(vp);
			VLog(0, "VAttachVolume: volume %s should be destoyed at next salvage", name);
			*ec = VNOVOL;
			return NULL;
		}
		V_inUse(vp) =
			(V_blessed(vp) && V_inService(vp) && !V_needsSalvaged(vp));
		VLog(9, "VAttachVolume: setting V_inUse(vp) = %d for volume %x",
		     V_inUse(vp), V_id(vp));
		if (V_inUse(vp))
			V_offlineMessage(vp)[0] = '\0';
	}

	AddVolumeToHashTable(vp, (int)V_id(vp));
	vp->nextVnodeUnique = V_uniquifier(vp);
	vp->vnIndex[vSmall].bitmap = vp->vnIndex[vLarge].bitmap = NULL;

	if (VolumeWriteable(vp)) {
		int i;
		for (i = 0; i<nVNODECLASSES; i++) {
			GetBitmap(ec,vp,i); 
			if (*ec) {
				FreeVolume(vp);
				return NULL;
			}
		}
	}
	VLog(29, "Leaving attach2()");
	return vp;
}
/* Attach an existing volume.
   The volume also normally goes online at this time.
   An offline volume must be reattached to make it go online.
 */

Volume *
VAttachVolume(Error *ec, VolumeId volumeId, int mode)
{
	char part[VMAXPATHLEN];
	int myind;
    
	VLog(9, "Entering VAttachVolume() for volume %x", volumeId);
	if ((myind = HashLookup(volumeId)) == -1) {
		VLog(0, "VAttachVolume: Volume %x not in index hash table!", 
		     volumeId);
		*ec = VNOVOL;
		return NULL;
	}
    
	GetVolPartition(ec, volumeId, myind, part);
	if (*ec) {
		Volume *vp;
		Error error;
		vp = VGetVolume(&error, volumeId);
		if (vp) {
			CODA_ASSERT(V_inUse(vp) == 0);
			VDetachVolume(ec, vp);
		}
		return NULL;
	}
	return VAttachVolumeById(ec, part, volumeId, mode);
}

/* Get a pointer to an attached volume.  The pointer is returned regardless
   of whether or not the volume is in service or on/off line.  An error
   code, however, is returned with an indication of the volume's status */
Volume *VGetVolume(Error *ec, VolumeId volumeId)
{
    Volume *vp;
    ProgramType *pt;
    struct stat status;	/* temp debugging stuff */
    int headerExists = 0;

    VLog(9, "Entering VGetVolume for volume %x", volumeId);
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    for(;;) {
	*ec = 0;
	for (vp = VolumeHashTable[VOLUME_HASH(volumeId)];
	     vp && vp->hashid != volumeId; vp = vp->hashNext)
		;
	if (!vp) {
	    VLog(29, "VGetVolume: Didnt find id %x in hashtable",
		 volumeId);
	    *ec = VNOVOL;
	    break;
	}
	VolumeGets++;
	VLog(19, "VGetVolume: nUsers == %d", vp->nUsers);
	if (vp->nUsers == 0) {

	    VLog(29, "VGetVolume: Calling AvailVolumeHeader()");
	    if (AvailVolumeHeader(vp)) {
		VLog(29, "VGetVolume: Calling GetVolumeHeader()");
		headerExists = GetVolumeHeader(vp);
		VLog(29, "VGetVolume: Finished GetVolumeHeader()");
	    } else if (vp->nUsers == 0) {
		/* must wrap transaction around volume replacement */
		rvm_return_t cstat;
		VLog(29, "VGetVolume: Calling GetVolumeHeader()");
		if (rvmlib_in_transaction()) {
		    headerExists = GetVolumeHeader(vp);
		} else {
		    rvmlib_begin_transaction(restore);
		    headerExists = GetVolumeHeader(vp);
		    rvmlib_end_transaction(flush, &(cstat));
		    if (cstat){
			VLog(0, "VGetVolume: WriteVolumeHeader failed!");
			CODA_ASSERT(0);
		    }
		}
		VLog(29, "VGetVolume: Finished GetVolumeHeader()");
	    }

	    if (!headerExists) {
		VolumeReplacements++;
		ExtractVolDiskInfo(ec, vp->vol_index, &V_disk(vp));

		if (*ec) {
		    /* Only log the error if it was a totally unexpected
		     * error.  Simply a missing inode is likely to be caused
		     * by the volume being deleted
		     */
		    if (errno != ENXIO || VolDebugLevel)
			VLog(0, "Volume %x: couldn't reread volume header",
								vp->hashid);
		    FreeVolume(vp);
		    vp = 0;
		    break;
		}
	    }
	}
	
    /* temp debugging stuff */
	VLog(39, "VGetVolume: partition name for volume %x is %s",
				V_id(vp), V_partname(vp));
	CODA_ASSERT(stat(V_partname(vp), &status) == 0);
	VLog(39, "VGetVolume: vp->partition->device = %u, disk.device = %u",
				V_device(vp),  status.st_dev);

	if (vp->shuttingDown) {
	    VLog(29, "VGetVolume: volume %x is shutting down",
		V_id(vp));
	    *ec = VNOVOL;
	    vp = 0;
	    break;
	}
	if (vp->goingOffline) {
	    VLog(29, "VGetVolume: Volume %x is going offline",
		V_id(vp));
	    LWP_WaitProcess((char *)VPutVolume);
	    continue;
	}
	if (vp->specialStatus){
	    VLog(29, "VGetVolume: Volume %x has special status",
		V_id(vp));
	    *ec = vp->specialStatus;
	}
	else if (V_inService(vp)==0 || V_blessed(vp)==0){
	    VLog(29, "VGetVolume: Vol %x not in service",
		V_id(vp));
	    *ec = VNOSERVICE; /* Either leave vp set or do ReleaseVolHeader */
	    /* Not sure which is better... */
	}
	else if (V_inUse(vp)==0){
	    VLog(29, "VGetVolume: Vol %x is offline", V_id(vp));
	    *ec = VOFFLINE;
	}
	vp->nUsers++;               
	break;
    }
    if (*ec && *pt == fileServer) {
	unsigned int volsite = FSYNC_CheckRelocationSite(volumeId);
	if (volsite != 0 && volsite != HostAddress[ThisServerId]) {
	    *ec = VMOVED;
	}
	if (vp) {
	    VPutVolume(vp);
	    vp = 0;
	}
    }
    return vp;
}

void VPutVolume(register Volume *vp)
{
    ProgramType *pt;

    VLog(9, "Entering VPutVolume for volume %x", V_id(vp));
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    CODA_ASSERT(--(vp->nUsers) >= 0);
    if (vp->nUsers == 0) {
        Error error;

	ReleaseVolumeHeader(vp->header);
        if (vp->goingOffline) {
	    CODA_ASSERT(*pt == fileServer);
	    vp->goingOffline = 0;
	    V_inUse(vp) = 0;
	    VLog(1, "VPutVolume: writing volume %x; going offline", V_id(vp));
	    VUpdateVolume(&error, vp);	 /* write out the volume disk data */
	    if (VolDebugLevel) {
		VLog(0, "VPutVolume: Volume %x (%s) is now offline",
		    V_id(vp), V_name(vp));
		if (V_offlineMessage(vp)[0])
		    VLog(0, " (%s)", V_offlineMessage(vp));
		VLog(0, "");
	    }
	    LWP_SignalProcess((char *)VPutVolume);
	}
	if (vp->shuttingDown) {
	    FreeVolume(vp);
	    if (*pt == fileServer)
	        LWP_SignalProcess((char *)VPutVolume);
	}
    }
    else 
	    VLog(1, "VPutVolume (%x): NO-OP since vp->nUsers = %d", 
		 V_id(vp), vp->nUsers + 1);
}

/* Force the volume offline, set the salvage flag.  No further references to */
/* the volume through the volume package will be honored. */
void VForceOffline(Volume *vp)
{
	Error error;

	VLog(0, "VForceOffline for volume %x", V_id(vp));
	if (!V_inUse(vp))
		return;
	strcpy(V_offlineMessage(vp), "Forced offline due to internal error: volume needs to be salvaged");
	VLog(0, "Volume %x forced offline:  it needs to be salvaged!", V_id(vp));
	V_inUse(vp) = 0;
	VLog(1, "VForceOffline: setting V_inUse(%x) = 0 and writing out voldiskinfo", V_id(vp));
	vp->goingOffline = 0;    
	V_needsSalvaged(vp) = 1;
	VUpdateVolume(&error, vp);
	LWP_SignalProcess((char *)VPutVolume);
}

/* The opposite of VAttachVolume.  The volume header is written to disk, with
   the inUse bit turned off.  A copy of the header is maintained in memory,
   however (which is why this is VOffline, not VDetach).
 */   
void VOffline(Volume *vp, char *message)
{
    Error error;
    VolumeId vid = V_id(vp);
    ProgramType *pt;

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    VLog(9, "Entering VOffline for volume %x, running as %s",
	 V_id(vp), (*pt == fileServer)?"fileServer":((*pt == volumeUtility)?"volumeUtility":"fileUtility"));

    /* if called by volumeUtility, have fileserver put volume */
    /* back on line */
    if (*pt == volumeUtility) {
	VLog(9, "VOffline: volumeUtility relinquishing volume %x to fileServer",
		V_id(vp));
	*pt = fileServer;
	FSYNC_askfs(V_id(vp), FSYNC_ON, 0);
	*pt = volumeUtility;
	return;
    }

    if (!V_inUse(vp))
        return;
    if (V_offlineMessage(vp)[0] == '\0')
        strncpy(V_offlineMessage(vp),message,
		sizeof(V_offlineMessage(vp)));
    V_offlineMessage(vp)[sizeof(V_offlineMessage(vp))-1] = '\0';
    vp->goingOffline = 1;
    VPutVolume(vp);
    vp = VGetVolume(&error, vid);	/* Wait for it to go offline */
    if (vp)  /* In case it was reattached... */
        VPutVolume(vp);
}

/*
 * This gets used for the most part by utility routines that don't want
 * to keep all the volume headers around.  Generally, the file server won't
 * call this routine, because then the offline message in the volume header
 * (or other information) will still be available to clients
 */
void VDetachVolume(Error *ec, Volume *vp)
{
    VolumeId volume;
    int notifyServer = 0;
    ProgramType *pt;

    VLog(9, "Entering VDetachVolume() for volume %x", V_id(vp));
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    *ec = 0;	/* always "succeeds" */
    if (*pt == volumeUtility)
        notifyServer = (V_destroyMe(vp) != DESTROY_ME);
    volume = V_id(vp);
    DeleteVolumeFromHashTable(vp);
    vp->shuttingDown = 1;
    VPutVolume(vp);
    /* Will be detached sometime in the future--this is OK since volume is offline */


    if (*pt == volumeUtility && notifyServer) {
	/* Note: The server is not notified in the case of a bogus
   	   volume explicitly to make it possible to create a volume,
   	   do a partial restore, then abort the operation without ever
   	   putting the volume online.  This is essential in the case
   	   of a volume move operation between two partitions on the
   	   same server.  In that case, there would be two instances of
   	   the same volume, one of them bogus, which the file server
   	   would attempt to put on line */
	/* masquerade as fileserver for FSYNC_askfs call */
	*pt = fileServer;
	FSYNC_askfs(volume, FSYNC_ON, 0);
	*pt = volumeUtility;
    }
}

int VAllocBitmapEntry(Error *ec, Volume *vp, struct vnodeIndex *index,
		       int stride, int ix, int count)
{
    *ec = 0;

    VLog(9, "VAllocBitmapEntry: volume = %x, count = %d, stride = %d, ix = %d",
	 V_id(vp), count, stride, ix);
    CODA_ASSERT(count > 0 && stride > 0 && ix >= 0);

    if (index->bitmap == NULL) {
	VLog(0, "VAllocBitmapEntry: uninitialized bitmap");
	*ec = VFAIL;
	return(0);
    }

    VLog(19, "VAllocBitmapEntry: bitmapOffset = %d, bitmapSize = %d",
	 index->bitmapOffset, index->bitmapSize);
    byte *bp = index->bitmap + index->bitmapOffset;	/* ptr to first byte of first word */
							/* containing a free bit */
    int	bbn = index->bitmapOffset * 8;			/* first bit of word containing first free bit */
    byte *ep = index->bitmap + index->bitmapSize;	/* ptr to first byte beyond current bitmap */
    int	ebn = index->bitmapSize	* 8;			/* one past the last bit in the map */

    /* Compute the starting bit number of a sequence of free bits, count entries long, which satisfies the */
    /* specified <stride, ix> requirements.  Note that sbn may be beyond the current end of the bitmap! */
    int sbn;
    {
	int cbn	= bbn;			/* current bit under consideration */
	int free = 0;			/* free bits in the sequence beginning with sbn */

	/* Current bit may need to be incremented to satisfy <stride, ix>! */
	cbn += (cbn < stride ? ix : ((stride - (cbn % stride) + ix) % stride));
	sbn = cbn;
	for (; free < count && cbn < ebn; cbn += stride) {
	    byte *cbp = (index->bitmap + cbn / 8);

	    if ((*cbp) & (1 << (cbn % 8))) {
		sbn = cbn + stride;
		free = 0;
	    }
	    else {
		free++;
	    }
	}
    }
    VLog(19, "VAllocBitmapEntry: sbn = %d (bbn = %d, ebn = %d)", sbn, bbn, ebn);

    /* Compute the number of bitmap bytes needed to satisfy this allocation, and grow the map if needed. */
    int newsize = (((sbn + (count - 1) * stride) / 32 + 1) * 4);
    int growsize = (newsize - index->bitmapSize);
    if (growsize > 0) {
	if (growsize < VOLUME_BITMAP_GROWSIZE) {
	    growsize = VOLUME_BITMAP_GROWSIZE;
	    newsize = index->bitmapSize + growsize;
	}

	VLog(1, "VAllocBitmapEntry: realloc'ing from %x to %x",
	    index->bitmapSize, newsize);
	index->bitmap = (byte *)realloc(index->bitmap, newsize);
	CODA_ASSERT(index->bitmap != NULL);
	bzero(index->bitmap + index->bitmapSize, growsize);
	index->bitmapSize = newsize;

	bp = index->bitmap + index->bitmapOffset;
	ep = index->bitmap + index->bitmapSize;
    }

    /* Set the specified sequence of bits, starting with sbn. */
    int cbn = sbn;
    for (; count > 0; cbn += stride, count--) {
	byte *cbp = (index->bitmap + (cbn / 8));
	byte mask = (1 << (cbn % 8));
	CODA_ASSERT((*cbp & mask) == 0);
	*cbp |= mask;
    }

    /* Update the bitmapOffset if necessary. */
    while (bp < ep && *((bit32 *)bp) == 0xffffffff)
	bp = (byte *)((bit32 *)bp + 1);

    return(sbn);
}

int VAllocBitmapEntry(Error *ec, Volume *vp, struct vnodeIndex *index, VnodeId vnode) {
    *ec = 0;

    VLog(9, "VAllocBitmapEntry: volume = %x, vnode = %x",
	 V_id(vp), vnode);

    if (index->bitmap == NULL) {
	*ec = VFAIL;
	VLog(0, "VAllocBitmapEntry: uninitialized bitmap");
	return(0);
    }

    VLog(19, "VAllocBitmapEntry: bitmapOffset = %d, bitmapSize = %d",
	 index->bitmapOffset, index->bitmapSize);
    int cbn = (int)vnodeIdToBitNumber(vnode);
    byte *cbp = index->bitmap + (cbn / 8);		/* ptr to byte containing requested bit */
    byte *bp = index->bitmap + index->bitmapOffset;	/* ptr to first byte of first word */
							/* containing a free bit */
    byte *ep = index->bitmap + index->bitmapSize;	/* ptr to first byte beyond current bitmap */

    /* Grow bitmap if requested bit is beyond end. */
    if (cbp >= ep) {
	int newsize = ((cbn / 32 + 1) * 4);		/* in bytes */
	int growsize = newsize - index->bitmapSize;	/* in bytes */
	CODA_ASSERT(growsize >= 4);
	if (growsize < VOLUME_BITMAP_GROWSIZE) {
	    growsize = VOLUME_BITMAP_GROWSIZE;
	    newsize = index->bitmapSize + growsize;
	}

	VLog(1, "VAllocBitmapEntry: realloc'ing from %x to %x",
	    index->bitmapSize, newsize);
	index->bitmap = (byte *)realloc(index->bitmap, newsize);
	CODA_ASSERT(index->bitmap != NULL);
	bzero(index->bitmap + index->bitmapSize, growsize);
	index->bitmapSize = newsize;

	cbp = index->bitmap + (cbn / 8);
	bp = index->bitmap + index->bitmapOffset;
	ep = index->bitmap + index->bitmapSize;
    }

    /* Set the requested bit. */
    int offset = (cbn % 8);
    byte mask = (1 << offset);
    *cbp |= mask;

    /* Update the bitmapOffset if necessary. */
    while (bp < ep && *((bit32 *)bp) == 0xffffffff)
	bp = (byte *)((bit32 *)bp + 1);

    return(cbn);
}

void VFreeBitMapEntry(Error *ec, register struct vnodeIndex *index, int bitNumber)
{
     int offset;
     *ec = 0;

    VLog(9, "Entering VFreeBitMapEntry() for bitNumber %d", bitNumber);
     offset = bitNumber>>3;
     if (offset > index->bitmapSize) {
	*ec = VNOVNODE;
	return;
     }
     if (offset < index->bitmapOffset)
     	index->bitmapOffset = offset&~3;	/* Truncate to nearest bit32 */
     *(index->bitmap + offset) &= ~(1 << (bitNumber & 0x7));
}

/* Write out volume disk data; force off line on failure */
void VUpdateVolume(Error *ec, Volume *vp)
{
	ProgramType *pt;

	VLog(9, "Entering VUpdateVolume() for volume %x", V_id(vp));
	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	*ec = 0;

	WriteVolumeHeader(ec, vp);
	if (*ec) {
		VLog(0, "VUpdateVolume/WriteVolumeHeader err: %d vol: %x (%s)",
		     *ec, V_id(vp), V_name(vp));
		VForceOffline(vp);
	}
}

/*
  PrintVolumesInHashTable: Print summary of all volumes in the hash table
 */
void PrintVolumesInHashTable()
{
	if (VolDebugLevel < 50)
		return;
	for (int i = 0; i < VOLUME_HASH_TABLE_SIZE; i++){
		printf("PrintVolumesInHashTable: Lookint at index %d\n", i);
		Volume *vp = VolumeHashTable[i];
		while(vp){
			printf("PVInHT: volume id %lx hashid %lu is in HT\n",
			       V_id(vp), vp->hashid);
			vp = vp->hashNext;
		}
	}
}

void FreeVolume(Volume *vp)
{
	int i;
	ProgramType *pt;

	VLog(9, "Entering FreeVolume for volume %x", V_id(vp));
	CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
	if (!vp)
		return;
	for (i = 0; i<nVNODECLASSES; i++)
		if (vp->vnIndex[i].bitmap)
			free(vp->vnIndex[i].bitmap);
	FreeVolumeHeader(vp);
	DeleteVolumeFromHashTable(vp);
	free((char *)vp);
}

extern int bitmap_flag;

/* Create a bitmap of the appropriate size for the specified vnode index */
/* Create a new bitmap patterned on the specified  vnode array */
/* in recoverable storage */
static void GetBitmap(Error *ec, Volume *vp, VnodeClass vclass)
{
	register struct VnodeClassInfo *vcp = &VnodeClassInfo_Array[vclass];
	register struct vnodeIndex *vip = &vp->vnIndex[vclass];
	char bigbuf[SIZEOF_LARGEDISKVNODE];
	VnodeDiskObject *vnode;
	register Unique_t unique = 0;
	int bitNumber = 0;

	VLog(9, "Entering GetBitmap() for volume %x, vclass = %d",
	     V_id(vp), vclass);
	*ec = 0;
	vindex vol_index(V_id(vp), vclass, V_device(vp), vcp->diskSize);
	vindex_iterator vnext(vol_index);
	int slots = vol_index.elts();

	/* The vnode array size (slots / 8) must be a multiple */
	/* of 4 bytes! */
	vip->bitmapSize = slots / 8;    /* in bytes */

	vnode = (VnodeDiskObject *)bigbuf;

	vip->bitmap = (byte *) malloc(vip->bitmapSize);
	VLog(9, "GetBitmap: allocating bitmap of %d bytes; array size %d",
	     vip->bitmapSize, slots);
	CODA_ASSERT(vip->bitmap != NULL);
	bzero(vip->bitmap, vip->bitmapSize);
	vip->bitmapOffset = 0;
	if ((vip->bitmapSize << 3) > slots) {
		VLog(1, "GetBitmap: bitmapSize = %d bits, only %d array slots",
		     (vip->bitmapSize << 3), slots);
		GrowVnodes(V_id(vp), vclass, vip->bitmapSize);
	}

	/* iterate through all vnodes in specified class index */
	bitmap_flag = 1;
	while ((bitNumber = vnext(vnode)) != -1) {
		if (vnode->vnodeMagic != vcp->magic) {
			VLog(0, "GetBitmap: addled vnode index in volume %s; volume needs salvage", V_name(vp));
			VLog(0, "GetBitmap: vnodeMagic = 0x%x; vcp magic = 0x%x", 
			     vnode->vnodeMagic, vcp->magic);
			VLog(0, "GetBitmap: Printing vnode(addr: 0x%x) at bitnumber = 0x%x",
			     vnode, bitNumber);
			print_VnodeDiskObject(vnode);
			VLog(0, "GetBitmap: Print Vnode returned");
			*ec = VSALVAGE;
			break;
		}
		VLog(29, "GetBitmap: found vnode at index %d", bitNumber);
		*(vip->bitmap + (bitNumber>>3)) |= (1 << (bitNumber & 0x7));
		VLog(29, "results of or: *(vip->bitmap + bitNumber >> 3) = %o",
		     *(vip->bitmap + (bitNumber >> 3)));
		if (unique <= vnode->uniquifier)
			unique = vnode->uniquifier + 1; 
	}

	if (vp->nextVnodeUnique < unique) {
		VLog(0, "GetBitmap: bad volume uniquifier for volume %s; volume needs salvage", V_name(vp));
		*ec = VSALVAGE;
	}
}

int VolumeNumber(char *name)
{
	VLog(9, "Entering VolumeNumber for volume %s", name);
	if (*name == '/')
		name++;
	return atoi(name+1);
}

char *VolumeExternalName(VolumeId volumeId)
{
	static char name[15];
	VLog(9, "Entering VolumeExternalName for volume %x", volumeId);
	sprintf(name,VFORMAT,volumeId);
	return name;
}

#define OneDay	(24*60*60)		/* 24 hours */
#define Midnight(date) ((date-TimeZoneCorrection)/OneDay*OneDay+TimeZoneCorrection)

static void VAdjustVolumeStatistics(register Volume *vp)
{
	unsigned int now = FT_ApproxTime();

	VLog(9, "Entering VAdjustVolumeStatistics for volume %x", V_id(vp));
	if (now - V_dayUseDate(vp) > OneDay) {
		register long ndays, i;
		ndays = (now - V_dayUseDate(vp)) / OneDay;
		for (i = 6; i>ndays-1; i--)
			V_weekUse(vp)[i] = V_weekUse(vp)[i-ndays];
		for (i = 0; i<ndays-1 && i<7; i++)
			V_weekUse(vp)[i] = 0;
		if (ndays <= 7)
			V_weekUse(vp)[ndays-1] = V_dayUse(vp);
		V_dayUse(vp) = 0;
		V_dayUseDate(vp) = Midnight(now);
	}
}

void VBumpVolumeUsage(register Volume *vp)
{
	unsigned int now = FT_ApproxTime();
	rvm_return_t status;

	VLog(9, "Entering VBumpVolumeUsage for volume %x", V_id(vp));
	if (now - V_dayUseDate(vp) > OneDay)
		VAdjustVolumeStatistics(vp);
	if ((V_dayUse(vp)++ & 127) == 0) {
		Error error;
		VLog(1, "VBumpVolumeUsage: writing out VolDiskInfo (vol %x)",
		     V_id(vp));
		rvmlib_begin_transaction(restore);
		VUpdateVolume(&error, vp);
		rvmlib_end_transaction(flush, &(status));
	}
}

void VSetDiskUsage() 
{
	static int FifteenMinuteCounter;

	VLog(9, "Entering VSetDiskUsage()");
	DP_ResetUsage();
	if (++FifteenMinuteCounter == 3) {
		FifteenMinuteCounter = 0;
		VScanUpdateList();
	}
}

/* The number of minutes that a volume hasn't been updated before the
   "Dont salvage" flag in the volume header will be turned on */

#define SALVAGE_INTERVAL	(10*60)

static VolumeId *UpdateList;	/* Pointer to array of Volume ID's */
static int nUpdatedVolumes;	/* Updated with entry in UpdateList, salvage after crash flag on */
static int updateSize;		/* number of entries possible */
#define UPDATE_LIST_SIZE 100	/* size increment */

void VAddToVolumeUpdateList(Error *ec, register Volume *vp)
{
    VLog(9, "Entering VAddToVolumeUpdateList for volume %x", V_id(vp));

    *ec = 0;
    vp->updateTime = FT_ApproxTime();
    if (V_dontSalvage(vp) == 0){
	VLog(29, "VAddToVolumeUpdateList: salvage was 0 - so not updating volume");
	VLog(29, "Leaving VAddToVolumeUpdateList()");
	return;
    }
    V_dontSalvage(vp) = 0;

    int camstatus = 0;
    VUpdateVolume(ec, vp);

    if (*ec || camstatus){
	VLog(29, "VAddToVolumeUpdateList: Error during Update Volume; returning");
	return;
    }
    if (!UpdateList) {
	updateSize = UPDATE_LIST_SIZE;
	UpdateList = (VolumeId *) malloc(sizeof (VolumeId) * updateSize);
    } else {
	if (nUpdatedVolumes == updateSize) {
	    updateSize += UPDATE_LIST_SIZE;
	    UpdateList = (VolumeId *) realloc((char *)UpdateList, sizeof (VolumeId) * updateSize);
	}
    }
    VLog(29, "VAddToVolumeUpdateList: Adding volume %x to update list", 
	 V_id(vp));
    UpdateList[nUpdatedVolumes++] = V_id(vp);
    VLog(29, "Leaving VAddToVolumeUpdateList()");
}

static void VScanUpdateList() 
{
    register int i, gap;
    register Volume *vp;
    Error error;
    long now = FT_ApproxTime();

    VLog(9, "Entering VScanUpdateList()");
    /* Be careful with this code, since it works with interleaved calls to AddToVolumeUpdateList */
    for (i = gap = 0; i<nUpdatedVolumes; i++) {
	VLog(29, "ScanUpdateList: Going to call VGetVolume ");
        vp = VGetVolume(&error, UpdateList[i-gap] = UpdateList[i]);
	VLog(29, "ScanUpdateList: Just returned from VGetVolume");
	if (error) {
	    VLog(29, "ScanUpdateList: Error %d in VGetVolume", error);
	    gap++;
	}
	else if (vp->nUsers == 1 && now - vp->updateTime > SALVAGE_INTERVAL) {
	    VLog(29, "ScanUpdateList: Going to set salvage flag for volume %x", V_id(vp));
	    rvm_return_t cstat;
	    V_dontSalvage(vp) = DONT_SALVAGE;
	    rvmlib_begin_transaction(restore);
	    VUpdateVolume(&error, vp); /* No need to fsync--not critical */
	    rvmlib_end_transaction(flush, &(cstat));
	      VLog(29, "ScanUpdateList: Finished UPdating Volume %x",
		  V_id(vp));
	    gap++;
	}
	if (vp){
	    VLog(29, "ScanUpdateList: Going to Put volume %x", 
		V_id(vp));
	    VPutVolume(vp);
	}
	LWP_DispatchProcess();
    }
    nUpdatedVolumes -= gap;
    VLog(29, "ScanUpdateList: nUpdatedVolumes = %d",
	 nUpdatedVolumes);
    VLog(29, "Leaving ScanUpdateList()");
}

/***************************************************/
/* Add on routines to manage a volume header cache */
/***************************************************/

static struct volHeader *volumeLRU;

/* Allocate a bunch of headers; string them together */
/* should be called ONLY by fileserver */
void InitLRU(int howMany)
{
	register struct volHeader *hp;
	hp = (struct volHeader *)(calloc(howMany, sizeof(struct volHeader)));
	while (howMany--)
		ReleaseVolumeHeader(hp++);
}

/* Get a volume header from the LRU list:
   -  update an old one if necessary
   -  do not fill in the new data yet
   -  returns 1 if the correct header was already there
   -  remove the header from the LRU list.
*/
static int GetVolumeHeader(register Volume *vp)
{
	Error error;
	struct volHeader *hd;
	int old;
	
	VLog(9, "Entering GetVolumeHeader()");
	old = (vp->header != 0);
	
	if (old) {
		hd = vp->header;
		/* the joy of list headers Coda style: we are about to 
		   take hd out of the list */
		if (volumeLRU == hd)
			volumeLRU = hd->next;
		CODA_ASSERT(hd->back == vp);
	} else {
		/* not currently in use and least recently used */
		hd = volumeLRU->prev; 
		if (hd->back) {
			if (hd->diskstuff.inUse) {
				VLog(1, "storing VolumeDiskInfo (%x) to reclaim cache slot",
				     hd->diskstuff.id);
				WriteVolumeHeader(&error, hd->back);
				/* Ignore errors; catch them later */
				/* XXX Yeahhh: we won't ignore anymore */
				CODA_ASSERT(error == 0);
			}
			hd->back->header = 0;
		}
		hd->back = vp;
		vp->header = hd;
	}
	hd->prev->next = hd->next;
	hd->next->prev = hd->prev;
	hd->next = hd->prev = 0;

	return old;
}

/* see if there is a volHeader available */
static int AvailVolumeHeader(Volume *vp)
{
	struct volHeader *hd;

	VLog(9, "Entering AvailVolumeHeader()");
	if (vp->header != 0) {
		VLog(29, "AvailVolumeHeader returns 1");
		return 1;
	}

	/* least recently used */
	hd = volumeLRU->prev;
	if (hd->back && hd->diskstuff.inUse) {
		VLog(29, "AvailVolumeHeader returns 0");
		return(0);
	}

	VLog(29, "AvailVolumeHeader returns 1");
	return (1);
}

/* Put it at the top of the LRU chain */
static void ReleaseVolumeHeader(register struct volHeader *hd)
{
    VLog(61, "Entering ReleaseVolumeHeader");
    if (!hd || hd->next) /* no header, or header already released */
	return;
    if (!volumeLRU) {
	hd->next = hd->prev = hd;
    } 
    else {
	hd->prev = volumeLRU->prev;
	hd->next = volumeLRU;
        hd->prev->next = hd->next->prev = hd;
    }
    volumeLRU = hd;
}

void FreeVolumeHeader(register Volume *vp)
{
    register struct volHeader *hd = vp->header;

    VLog(9, "Entering FreeVolumeHeader for volume %x", V_id(vp));
    if (!hd)
	return;

    ReleaseVolumeHeader(hd);
    hd->back = 0;
    vp->header = 0;
}


/***************************************************/
/* Routines to add volume to hash chain, delete it */
/***************************************************/
/*
  AddVolumeToHashTable: Add the volume (*vp) to the hash table
  As used, hashid is always the id of the volume.  
*/
static void AddVolumeToHashTable(register Volume *vp, int hashid)
{
    int hash = VOLUME_HASH(hashid);
    Volume *vptr;
    VLog(9, "Entering AddVolumeToHashTable for volume %x, hashid %u",
					V_id(vp), hashid);

    /* Do some sanity checking before performing insert. */
    if (hashid != (long)V_id(vp)) {
	VLog(0, "VolHashTable: hashid %x != V_id(vp).", hashid, V_id(vp));
	CODA_ASSERT(0);
    }

    vptr = VolumeHashTable[hash];	/* Check the bucket for duplicates. */
    while (vptr) {
	if (vptr->hashid == vp->hashid) {
	    VLog(0, "VolHashTable: Adding another volume with id %x.", hashid);
	}
	vptr = vptr->hashNext;
    }

    vp->hashid = hashid;
    vp->hashNext = VolumeHashTable[hash];
    VolumeHashTable[hash] = vp;
    vp->vnodeHashOffset = VolumeHashOffset();
}    

/*
  DeleteVolumeFromHashTable: Delete the Volume *vp from the hash table.
 */
void DeleteVolumeFromHashTable(register Volume *vp)
{
    unsigned long hash = VOLUME_HASH(vp->hashid);

    VLog(9, "Entering DeleteVolumeFromHashTable");

    if (vp->header)  /* Put LRU entry back on queue */
	ReleaseVolumeHeader(vp->header);

    if (VolumeHashTable[hash] == vp){
	VLog(29, "DeleteVolumeFromHashTable: Deleting volume %x from hash table",
	    vp->hashid);
	VolumeHashTable[hash] = vp->hashNext;
    } else {
	Volume *tvp = VolumeHashTable[hash];
	if (tvp == NULL)
	    return;
	while (tvp->hashNext && tvp->hashNext != vp)
	    tvp = tvp->hashNext;
	if (tvp->hashNext == NULL)
	    return;
	tvp->hashNext = vp->hashNext;
	VLog(29, "DeleteVolumeHashTable: Deleting volume %x from hashtable", vp->hashid);
    }
    vp->hashid = 0;
}

void VPrintCacheStats(FILE *fp) 
{
    register struct VnodeClassInfo *vcp;
    vcp = &VnodeClassInfo_Array[vLarge];
    VLog(0, "Large vnode cache, %d entries, %d allocs, "
	 "%d gets (%d reads), %d writes",
	 vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    vcp = &VnodeClassInfo_Array[vSmall];
    VLog(0, "Small vnode cache,%d entries, %d allocs, "
	 "%d gets (%d reads), %d writes",
	 vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    VLog(0,"Volume header cache, %d entries, %d gets, %d replacements",
	 VolumeCacheSize, VolumeGets, VolumeReplacements);
}

void SetVolDebugLevel(int level) {
    VolDebugLevel = level;
}


/* Quota enforcement: since the return value of close is not often
   checked we set ec only when we are already over quota. If a store
   operation exceeds the quota the next open will fail.
   We could enforce quota more strictly with the clause: 
      (V_maxquota(vp) && (V_diskused(vp) + blocks > V_maxquota(vp)))
 */
void VAdjustDiskUsage(Error *ec, Volume *vp, int blocks)
{
    *ec = 0;
    if (blocks > 0) {
	if (vp->partition->free - blocks < 0)
	    *ec = VDISKFULL;
	else if (V_maxquota(vp) && (V_diskused(vp) >= V_maxquota(vp)))
	    *ec = EDQUOT;
    }    
    vp->partition->free -= blocks;
    V_diskused(vp) += blocks;
}

void VCheckDiskUsage(Error *ec, Volume *vp, int blocks)
{
    *ec = 0;
    if (blocks > 0){
	if (vp->partition->free - blocks < 0)
	    *ec = VDISKFULL;
	else if (V_maxquota(vp) && (V_diskused(vp) >= V_maxquota(vp)))	
	    *ec = EDQUOT;
    }
}

void VGetPartitionStatus(Volume *vp, int *totalBlocks, int *freeBlocks)
{
    *totalBlocks = vp->partition->totalUsable;
    *freeBlocks = vp->partition->free;
}


int GetVolObj(VolumeId Vid, Volume **volptr, 
	      int LockLevel, int Enque, int LockerAddress) 
{
    
    int errorCode = 0;
    *volptr = 0;

    *volptr = VGetVolume((Error *)&errorCode, Vid);
    if (errorCode) {
	    SLog(0, "GetVolObj: VGetVolume(%x) error %d", Vid, errorCode);
	    goto FreeLocks;
    }

    switch(LockLevel) {
    case VOL_NO_LOCK:
	    break;

    case VOL_SHARED_LOCK:
	    if (V_VolLock(*volptr).IPAddress != 0) {
		    SLog(0, "GetVolObj: Volume (%x) already write locked", Vid);
		    VPutVolume(*volptr);
		    *volptr = 0;
		    errorCode = EWOULDBLOCK;
		    goto FreeLocks;
	    }
	    ObtainReadLock(&(V_VolLock(*volptr).VolumeLock));
	    break;

    case VOL_EXCL_LOCK:
	    CODA_ASSERT(LockerAddress);
	    if (V_VolLock(*volptr).IPAddress != 0) {
		    SLog(0, "GetVolObj: Volume (%x) already write locked", Vid);
		    VPutVolume(*volptr);
		    *volptr = 0;
		    errorCode = EWOULDBLOCK;
		    goto FreeLocks;
	    }
	    V_VolLock(*volptr).IPAddress = LockerAddress;
	    ObtainWriteLock(&(V_VolLock(*volptr).VolumeLock));
	    CODA_ASSERT((long)V_VolLock(*volptr).IPAddress == LockerAddress);
	    if (Enque) {
		    lqent *lqep = new lqent(Vid);
		    LockQueueMan->add(lqep);
	    }
	    break;
    default:
	    CODA_ASSERT(0);
    }
    
 FreeLocks:
    SLog(9, "GetVolObj: returns %d", errorCode);
    
    return(errorCode);
}

/*
  PutVolObj: Unlock a volume
*/
void PutVolObj(Volume **volptr, int LockLevel, int Dequeue)
{
    if (*volptr == 0) return;
    switch (LockLevel) {
      case VOL_NO_LOCK:
	break;
      case VOL_SHARED_LOCK:
	SLog(9, "PutVolObj: One less locker");
	ReleaseReadLock(&(V_VolLock(*volptr).VolumeLock));
	break;
      case VOL_EXCL_LOCK:
	if (Dequeue) {
	    lqent *lqep = LockQueueMan->findanddeq(V_id(*volptr));
	    if (!lqep) 
		SLog(0, "PutVolObj: Couldn't find entry %x on lock queue", 
			V_id(*volptr));
	    else {
		LockQueueMan->remove(lqep);
		delete lqep;
	    }
	}
	if (V_VolLock(*volptr).IPAddress) {
	    V_VolLock(*volptr).IPAddress = 0;
	    ReleaseWriteLock(&(V_VolLock(*volptr).VolumeLock));
	}
	break;
      default:
	CODA_ASSERT(0);
    }

    VPutVolume(*volptr);
    *volptr = 0;
    SLog(10, "Returning from PutVolObj");
}
