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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/volume.cc,v 4.12 1998/08/26 21:22:29 braam Exp $";
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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef __BSD44__
#include <fstab.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <util.h>
#include <partition.h>
#include <rvmlib.h>

#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus


#include "cvnode.h"
#include "volume.h"
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
  BEGIN_HTML
  <a name="VolumeHashTable">
  <strong>Hash table used to store pointers to the Volume structure</strong>
  </a>
  END_HTML
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
static int VolumeCacheCheck = 0;	/* Incremented everytime a volume goes on line--used to stamp
			   volume headers and in-core vnodes.  When the volume goes on-line
			   the vnode will be invalidated */

static int VolumeCacheSize = 50, VolumeGets = 0, VolumeReplacements = 0;

static void VListVolume(FILE *file, Volume *vp);
static void VAppendVolume(Volume *vp);
static void WriteVolumeHeader(Error *ec, Volume *vp);
static Volume *attach2(Error *ec, char *path, struct VolumeHeader *header,
			Device device, char *partition);
static void VSyncVolume(Error *ec, Volume *vp);
static void GetBitmap(Error *ec, Volume *vp, VnodeClass vclass);
static void VAdjustVolumeStatistics(Volume *vp);
static void VScanUpdateList();
//static void InitLRU(int howMany); -- Make it public for norton.
static int GetVolumeHeader(Volume *vp);
static int AvailVolumeHeader(register Volume *vp);
static void ReleaseVolumeHeader(struct volHeader *hd);
void FreeVolumeHeader(Volume *vp);
static void AddVolumeToHashTable(Volume *vp, int hashid);
void DeleteVolumeFromHashTable(Volume *vp);

static int MountedAtRoot(char *path);


/* InitVolUtil has a problem right now - 
   It seems to get advisory locks on these files, but
   the volume utilities don't seem to release locks after 
   they are done.  Since this is going to be deleted  most probably
   in the redesign of the volume package, I just added the 
   close() calls right now.
*/
/* invoked by all volume utilities except full salvager */
int VInitVolUtil(ProgramType pt) {
    int fslock, fvlock;

    fslock = -1;
    fvlock = -1;

    LogMsg(9, VolDebugLevel, stdout, "Entering VInitVolUtil");
    fslock = open("/vice/vol/fs.lock", O_CREAT|O_RDWR, 0666);
    assert(fslock >= 0);
    fvlock = open ("/vice/vol/volutil.lock", O_CREAT|O_RDWR, 0666);
    assert(fvlock >= 0);

    if (pt != salvager) {
	/* wait until file server is initialized */
	if (VInit != 1) {
	    LogMsg(0, VolDebugLevel, stdout, "VInitVolUtil: File Server not initialized! Aborted");
	    close(fslock);
	    close(fvlock);
	    return(VNOSERVER);
	}
	if (flock(fvlock, LOCK_SH |LOCK_NB) != 0) {
	    LogMsg(0, VolDebugLevel, stdout, "VInitvolUtil: can't grab volume utility lock");
	    close(fslock);
	    close(fvlock);
	    return(VFAIL);
	}

	if (!VConnectFS()) {
	    LogMsg(0, VolDebugLevel, stdout, "Unable to synchronize with file server; aborted");
	    close(fslock);
	    close(fvlock);
	    return(VFAIL);
	}
    }
    else {  /* pt == salvager */
	LogMsg(9, VolDebugLevel, stdout, "VInitVolUtil: getting exclusive locks");
	if (flock(fslock, LOCK_EX | LOCK_NB) != 0) {
	    LogMsg(0, VolDebugLevel, stdout, "VInitVolUtil: File Server is running: can't run full salvage");
	    close(fslock);
	    close(fvlock);
	    return(VFAIL);
	}

	if (flock(fvlock, LOCK_EX |LOCK_NB) != 0) {
	    LogMsg(0, VolDebugLevel, stdout, "VInitVolUtil: salvage aborted- someone else is running!");
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
void VInitVolumePackage(int nLargeVnodes, int nSmallVnodes, int DoSalvage) {
    struct timeval tv;
    struct timezone tz;
    ProgramType *pt;
    long rc = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering VInitVolumePackage(%d, %d, %d)",
	nLargeVnodes, nSmallVnodes, DoSalvage);

    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

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

    if (AllowResolution)
	/* Initialize the resolution storage structures */
	InitLogStorage();
    
    /* check VLDB */
    if (VCheckVLDB() == 0) {
	LogMsg(29, VolDebugLevel, stdout, "VInitVolPackage: successfully finished checking VLDB");
    }
    else {
	LogMsg(0, VolDebugLevel, stdout, "VInitVolPackage: no VLDB! Create a new one.");
    }

    /* Setting Debug to 1 and List to 0; maybe remove later ***ehs***/
    /* invoke salvager for full salvage */
    *pt = salvager;	/* MUST set *pt to salvager before vol_salvage */

    assert(S_VolSalvage(0, NULL, 0, DoSalvage, 1, 0) == 0);

    *pt = fileServer;

    /* grab exclusive lock */
/* Punted for getting salvager to run during shutdown 
    int fslock = open("/vice/vol/fs.lock", O_CREAT|O_RDWR, 0666);
    assert(fslock >= 0);
    while (flock(fslock, LOCK_EX) != 0)
	assert(errno == EINTR);
*/
    FSYNC_fsInit();

    /* Attach all valid volumes (from all vice partitions) */
    {
	Error error;
	Volume *vp;
	VolumeHeader header;
	char thispartition[64];
	int nAttached = 0, nUnattached = 0;
	int i = 0;
	int camstatus = 0;
	int maxid = (int)(SRV_RVM(MaxVolId) & 0x00FFFFFF);

	RVMLIB_BEGIN_TRANSACTION(restore)
	for (i = 0; (i < maxid) && (i < MAXVOLS); i++) {
	    if (VolHeaderByIndex(i, &header) == -1) {
		LogMsg(0, VolDebugLevel, stdout, "Bogus volume index %d (shouldn't happen)", i);
		continue;
	    }

	    if (header.stamp.magic != VOLUMEHEADERMAGIC)
		continue;

	    /* Make sure volume is in the volid hashtable */
	    LogMsg(9, VolDebugLevel, stdout, "VInitVolumePackage: inserting vol %x into hashtable for index %d",
		header.id, i);
	    if (HashInsert(header.id, i) == -1) {
		LogMsg(10, VolDebugLevel, stdout, "VInitVolPackage: HashInsert failed! Two %x volumes exist!", header.id);
	    }
	    
	    GetVolPartition(&error, header.id, i, thispartition);
	    if (error != 0) continue;	    // bogus volume
	    vp = VAttachVolumeById(&error, thispartition, header.id, V_UPDATE);
	    (*(vp?&nAttached:&nUnattached))++;
	    if (error == VOFFLINE)
		LogMsg(0, VolDebugLevel, stdout, "Volume %x stays offline (/vice/offline/%s exists)", 
			    header.id, VolumeExternalName(header.id));
	    /* if volume was not salvaged force it offline. */
	    /* a volume is not salvaged if it exists in the 
		/vice/vol/skipsalvage file 
		*/
	    if (vp && skipvolnums != NULL && 
		InSkipVolumeList(header.parent, skipvolnums, nskipvols)){
		LogMsg(0, VolDebugLevel, stdout, "Forcing Volume %x Offline", header.id);
		VForceOffline(vp);
	    }
	    else if (vp) {
		extern void InitVolLog(int);
		/* initialize the VM log/vnode for resolution */
		InitVolLog(i);
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
	LogMsg(0, VolDebugLevel, stdout, "Attached %d volumes; %d volumes not attached",
				nAttached, nUnattached);
	VListVolumes();			/* Update list in /vice/vol/VolumeList */
    RVMLIB_END_TRANSACTION(flush, &(camstatus));
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

    LogMsg(9, VolDebugLevel, stdout, "Entering VConnectFS");
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    assert(VInit == 1 && *pt == volumeUtility);
    rc = FSYNC_clientInit();
    return rc;
}

void VDisconnectFS() {
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Entering VDisconnectFS");
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    assert(VInit == 1 && *pt == volumeUtility);
    FSYNC_clientFinis();
}

void VInitThisHost()
{
        char hostname[MAXHOSTNAMELEN];
	struct hostent *hostent;
	long netaddress;

	gethostname(hostname, sizeof(hostname)-1);
#if 0
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
	ThisServerId = -1;
	ThisHost = (char *) malloc((int)strlen(hostname)+1);
	strcpy(ThisHost, hostname);
	hostent = gethostbyname(ThisHost);
	if (hostent == NULL) {
		LogMsg(0, VolDebugLevel, stdout, "Host %s cannot be resolved. Exiting.", ThisHost);
		exit(1);
	}
}

/* must be called before calling VInitVolumePackage!! */
/* Find the server id */
void VInitServerList() 
{

    char hostname[100];
    char line[200];
    char *serverList = SERVERLISTPATH;
    FILE *file;

    VInitThisHost();

    LogMsg(9, VolDebugLevel, stdout, "Entering VInitServerList");
    file = fopen(serverList, "r");
    if (file == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "VInitServerList: unable to read file %s; aborted", serverList);
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
    ThisHost = (char *) malloc((int)strlen(hostname)+1);
    strcpy(ThisHost, hostname);

    while (fgets(line, sizeof(line), file) != NULL) {
        char sname[50];
	struct hostent *hostent;
        int sid;
	if (sscanf(line, "%s%d", sname, &sid) == 2) {
	    if (sid > N_SERVERIDS) {
		LogMsg(0, VolDebugLevel, stdout, "Host %s is assigned a bogus server number (%x) in %s. Exit.",
		  sname, sid, serverList);
		exit(1);
	    }
	    if (UtilHostEq(ThisHost, sname))
		ThisServerId = sid;
	    hostent = gethostbyname(sname);
	    if (hostent == NULL) {
		LogMsg(0, VolDebugLevel, stdout, "Host %s (listed in %s) cannot be resolved. Exiting.", sname, serverList);
		exit(1);
	    } else {
		long netaddress;
		assert(hostent->h_length == 4);
		bcopy((char *)hostent->h_addr, (char *)&netaddress, 4);
		HostAddress[sid] = ntohl(netaddress);
	    }
	}
    }
    if (ThisServerId == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Hostname of this server (%s) is not listed in %s. Exiting.", ThisHost, serverList);
	exit(1);
    }
    fclose(file);
}

void VCheckVolumes()
{
    LogMsg(9, VolDebugLevel, stdout, "Entering VCheckVolumes()");
    VListVolumes();
    /* Note: removed body surrounded by "#ifdef undef"; recover from version 2.2*/
}

void VGetVolumeInfo(Error *ec, char *key, register VolumeInfo *info)
{
    register struct vldb *vldp;
    register VolumeId *vidp;
    register int i, nReported;
    bit32 *serverList;

    LogMsg(9, VolDebugLevel, stdout, "Entering VGetVolumeInfo, key = %s", key);

    *ec = 0;
    bzero((void *)info, sizeof(VolumeInfo));
    vldp = VLDBLookup(key);
    if (vldp == NULL) {
	*ec = VNOVOL;
	LogMsg(9, VolDebugLevel, stdout, "VGetVolumeInfo: VLDBLookup failed");
	return;
    }
    assert(vldp->volumeType < MAXVOLTYPES);
    info->Vid = ntohl(vldp->volumeId[vldp->volumeType]);
    info->Type = vldp->volumeType;
    for (i = 0, vidp = &info->Type0; i<MAXVOLTYPES; )
	*vidp++ = ntohl((unsigned long) vldp->volumeId[i++]);
    assert(vldp->nServers <= MAXVOLSERVERS);
    serverList = (bit32 *) &info->Server0;
    for (nReported = i = 0; i<vldp->nServers; i++) {
	register unsigned long serverAddress;
	serverAddress = HostAddress[vldp->serverNumber[i]];
	if (serverAddress)
	    serverList[nReported++] = serverAddress;
    }
    if (nReported == 0) {
	*ec = VNOVOL;
	LogMsg(9, VolDebugLevel, stdout, "VGetVolumeInfo: no reported servers for volume %x",
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

    LogMsg(9, VolDebugLevel, stdout, "Entering VListVolume for volume %x", V_id(vp));

    VAdjustVolumeStatistics(vp);
    for (volumeusage = i = 0; i<7; i++)
	volumeusage += V_weekUse(vp)[i];
    fprintf(file, "%c%s I%x H%x P%s m%x M%x U%x W%x C%x D%x B%x A%x\n",
      V_type(vp) == readwriteVolume? 'W':V_type(vp) == readonlyVolume? 'R':
	V_type(vp) == backupVolume? 'B':'?',
      V_name(vp), V_id(vp), ThisServerId, vp->partition->name,
      V_minquota(vp), V_maxquota(vp), V_diskused(vp),
      V_parentId(vp), V_creationDate(vp), V_copyDate(vp), V_backupDate(vp),
      volumeusage
    );
}

void VListVolumes() {
    FILE *file;
    register struct DiskPartition *part;
    int i;

    LogMsg(9, VolDebugLevel, stdout, "Entering VListVolumes()");

    file = fopen("/vice/vol/VolumeList.temp", "w");
    assert(file != NULL);
    for (part = DiskPartitionList; part; part = part->next) {
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
    rename("/vice/vol/VolumeList.temp","/vice/vol/VolumeList");
}

static void VAppendVolume(Volume *vp)
{
    FILE *file;

    LogMsg(9, VolDebugLevel, stdout, "Entering VAppendVolume for volume %x", V_id(vp));
    file = fopen("/vice/vol/VolumeList", "a");
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
	LogMsg(0, VolDebugLevel, stdout, "Couldn't open dumpvm %d", errno);
	return;
    }

    /* write out RVM */
    char *p = rds_startaddr;
    for (i = 0, j = 102400; j < rds_rvmsize; i+=102400, j += 102400, p+= 102400) {
	if (write(fd, (char *)p, 102400) != 102400) {
	    LogMsg(0, VolDebugLevel, stdout, "Write failed i %d, err %d", i, errno);
	    return ;
	}
    }
    long nbytes = rds_rvmsize - i;
    if (nbytes) 
	if (write(fd, (char *)p, (int)nbytes) != nbytes) {
	    LogMsg(0, VolDebugLevel, stdout, "Write failed for address 0x%x size %d", 
		p, nbytes);
	}

    close(fd);
}

void VShutdown() {
    int i, camstatus = 0;

    LogMsg(0, VolDebugLevel, stdout, "VShutdown:  shutting down on-line volumes...");

    for (i=0; i<VOLUME_HASH_TABLE_SIZE; i++) {
        register Volume *vp, *p;
	p = VolumeHashTable[i];
	while (p) {
	    Error error;
	    RVMLIB_BEGIN_TRANSACTION(restore)
	    vp = VGetVolume(&error, p->hashid);
	    if ((error != 0) || (!vp)) {
		LogMsg(0, VolDebugLevel, stdout, "VShutdown: Error %d getting volume %x!",error,p->hashid);
		rvmlib_abort(-1);
	    }
	    LogMsg(0, VolDebugLevel, stdout, "VShutdown: Taking volume %s(0x%x) offline...",
		V_name(vp), V_id(vp));
	    if (vp)
	        VOffline(vp, "File server was shut down");
	    LogMsg(0, VolDebugLevel, stdout, "... Done");
	    RVMLIB_END_TRANSACTION(flush, &(camstatus));
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
		LogMsg(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));  
	    else {
		LogMsg(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	
		for (i = 0; i < curopts.n_uncommit; i++) {
		    rvm_abort_transaction(&(curopts.tid_array[i]));
		    if (ret != RVM_SUCCESS) 
			LogMsg(0, 0, stdout,
			       "ERROR: abort failed, code: %s", rvm_return(ret));
		}
	    
		ret = rvm_query(&curopts, NULL);
		if (ret != RVM_SUCCESS)
		    LogMsg(0, 0, stdout,  "rvm_query returned %s", rvm_return(ret));	
		else 
		    LogMsg(0, 0, stdout,  "Uncommitted transactions: %d", curopts.n_uncommit);
	    }
	    rvm_free_options(&curopts);
	}

	dumpvm();
    }    
    LogMsg(0, VolDebugLevel, stdout, "VShutdown:  complete.");
}


private void WriteVolumeHeader(Error *ec, Volume *vp)
{
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering WriteVolumeHeader for volume %x", V_id(vp));
    ReplaceVolDiskInfo(ec, V_volumeindex(vp), &V_disk(vp));
    if (*ec != 0)
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
    int rc,listVolume;
    struct stat status;
    struct VolumeHeader header;
    char path[64];
    char name[32];
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Entering VAttachVolumeById() for volume %x", volid);
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    *ec = 0;
    if (*pt == volumeUtility) {
	LogMsg(19, VolDebugLevel, stdout, "running as volume utility");
	assert(VInit == 1);
	VLockPartition(partition);
    }
    if (*pt == fileServer) {
	LogMsg(19, VolDebugLevel, stdout, "running as fileserver");
	vp = VGetVolume(ec, volid);
	if (vp) {
	    if (V_inUse(vp)) {
		LogMsg(1, VolDebugLevel, stdout, "VAttachVolumeById: volume %x already in use",
		    V_id(vp));
		return vp;
	    }
	    VDetachVolume(ec, vp);
	    listVolume = 0;	    
	}
	else
	    listVolume = 1;
    }
    *ec = 0;
    
    sprintf(name, VFORMAT, volid);
    strcpy(path, partition);
    strcat(path, "/");
    strcat(path, name);
    if (ExtractVolHeader(volid, &header) != 0) {
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolumeById: Cannot read volume header %s", path);
	*ec = VNOVOL;
	return NULL;
    }

    if (header.stamp.magic != VOLUMEHEADERMAGIC) {
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolumeById: Error reading volume header for %s", path);
	*ec = VSALVAGE;
	return NULL;
    }
    if (header.stamp.version != VOLUMEHEADERVERSION) {
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolumeById: Volume %s, version number %u is incorrect; volume needs salvage",path, header.stamp.version);
	*ec = VSALVAGE;
	return NULL;
    }
    if (*pt == volumeUtility && mode != V_SECRETLY) {
	/* modify lwp program type for duration of FSYNC_askfs call */
	*pt = fileServer;
	rc = FSYNC_askfs(header.id, FSYNC_NEEDVOLUME, mode);
	*pt = volumeUtility;
	if (rc == FSYNC_DENIED) {
	    LogMsg(0, VolDebugLevel, stdout, "VAttachVolumeById: attach of volume %x apparently denied by file server",
	        header.id);
	    *ec = VNOVOL; /* XXXX */
	    return NULL;
	}
    }
    assert(stat(partition, &status) == 0);
    vp = attach2(ec, path, &header, status.st_dev, partition);
    if (vp == NULL)
	LogMsg(9, VolDebugLevel, stdout, "VAttachVolumeById: attach2 returns vp == NULL");

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
	if (VolDebugLevel)
	    LogMsg(0, VolDebugLevel, stdout, "VAttachVolumeById:  volume %x (%s) attached and online",
		V_id(vp), V_name(vp));
	if (listVolume && VInit)
	    VAppendVolume(vp);
    }
    LogMsg(29, VolDebugLevel, stdout, "returning from VAttachVolumeById()");
    return vp;
}

static Volume *attach2(Error *ec, char *path, register struct VolumeHeader *header,
			    Device device, char *partition)
{
    register Volume *vp;
    ProgramType *pt;

    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    LogMsg(9, VolDebugLevel, stdout, "Entering attach2(); %s running as fileServer",
	    (*pt==fileServer)?"":"not");



    vp = (Volume *) calloc(1, sizeof(Volume));
    assert(vp != NULL);

    vp->partition = VGetPartition(partition);
    if (vp->partition == NULL) {
	FreeVolume(vp);
	return NULL;
    }
    
    vp->specialStatus = 0;
    vp->device = device;
    vp->cacheCheck = ++VolumeCacheCheck;
    vp->shuttingDown = 0;
    vp->goingOffline = 0;
    vp->nUsers = 1;
    /* Initialize the volume level lock  for resolution/repair */
    V_VolLock(vp).IPAddress = 0;
    Lock_Init(&(V_VolLock(vp).VolumeLock));
    vp->nReintegrators = 0;	
    vp->reintegrators = NULL;	
    GetVolumeHeader(vp);    /* get a VolHeader from LRU list */

    /* get the volume index and the VolumeDiskInfo from recoverable storage */
    vp ->vol_index = VolDiskInfoById(ec, header->id, &V_disk(vp));
    if (*ec) {
	LogMsg(0, VolDebugLevel, stdout, "returned from VolDiskInfoById for id %x with *ec = %d",
	    header->id, *ec);
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolume: Error attaching volume %s; salvage volume!",
	    path);
	FreeVolume(vp);
	return NULL;
    }
    
    if (V_needsSalvaged(vp)) {
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolume: volume salvage flag is ON for %s; volume needs salvage", path);
	*ec = VSALVAGE;
	return NULL;
    }
    if (*pt == fileServer) {
	if (V_inUse(vp) && VolumeWriteable(vp)) {
	    FreeVolume(vp);
	    LogMsg(0, VolDebugLevel, stdout, "VAttachVolume: volume %s needs to be salvaged; not attached.", path);
	    *ec = VSALVAGE;
	    return NULL;
	}
	if (V_destroyMe(vp) == DESTROY_ME) {
	    FreeVolume(vp);
	    LogMsg(0, VolDebugLevel, stdout, "VAttachVolume: volume %s is junk; it should be destoyed at next salvage", path);
	    *ec = VNOVOL;
	    return NULL;
	}
    	V_inUse(vp) =
            (V_blessed(vp) && V_inService(vp) && !V_needsSalvaged(vp));
	LogMsg(9, VolDebugLevel, stdout, "VAttachVolume: setting V_inUse(vp) = %d for volume %x",
		    V_inUse(vp), V_id(vp));
    	if (V_inUse(vp))
    	    V_offlineMessage(vp)[0] = '\0';
    }
    /* bug fix part ii (11/88): don't redo what's already been done */
/*    if (*pt == volumeUtility && !V_inUse(vp)) { */
    AddVolumeToHashTable(vp, (int)V_id(vp));
    vp->nextVnodeUnique = V_uniquifier(vp);
    vp->vnIndex[vSmall].bitmap = vp->vnIndex[vLarge].bitmap = NULL;
/*    } */
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
    LogMsg(29, VolDebugLevel, stdout, "Leaving attach2()");
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
    
    LogMsg(9, VolDebugLevel, stdout, "Entering VAttachVolume() for volume %x", volumeId);
    if ((myind = HashLookup(volumeId)) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "VAttachVolume: Volume %x not in index hash table!", volumeId);
	*ec = VNOVOL;
	return NULL;
    }
    
    GetVolPartition(ec, volumeId, myind, part);
    if (*ec) {
        register Volume *vp;
	Error error;
	vp = VGetVolume(&error, volumeId);
	if (vp) {
	    assert(V_inUse(vp) == 0);
	    VDetachVolume(ec, vp);
	}
        return NULL;
    }
    return VAttachVolumeById(ec, part, volumeId, mode);
}

/* Get a pointer to an attached volume.  The pointer is returned regardless
   of whether or not the volume is in service or on/off line.  An error
   code, however, is returned with an indication of the volume's status */
Volume *VGetVolume(Error *ec, register VolumeId volumeId)
{
    register Volume *vp;
    ProgramType *pt;
    struct stat status;	/* temp debugging stuff */
    int headerExists = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering VGetVolume for volume %x", volumeId);
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    for(;;) {
	*ec = 0;
	for (vp = VolumeHashTable[VOLUME_HASH(volumeId)];
	     vp && vp->hashid != volumeId; vp = vp->hashNext)
	    ;
	if (!vp) {
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Didnt find volume id %x in hashtable",
		volumeId);
	    *ec = VNOVOL;
	    break;
	}
	VolumeGets++;
	LogMsg(19, VolDebugLevel, stdout, "VGetVolume: nUsers == %d", vp->nUsers);
	if (vp->nUsers == 0) {
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Calling AvailVolumeHeader()");
	    if (AvailVolumeHeader(vp)){
		LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Calling GetVolumeHeader()");
		headerExists = GetVolumeHeader(vp);
		LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Finished GetVolumeHeader()");
	    }
	    else if (vp->nUsers == 0) {
		/* must wrap transaction around volume replacement */
		int cstat = 0;
		LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Calling GetVolumeHeader()");
		if (rvmlib_in_transaction()) 
		    headerExists = GetVolumeHeader(vp);
		else {
		    RVMLIB_BEGIN_TRANSACTION(restore)
		    headerExists = GetVolumeHeader(vp);
		    RVMLIB_END_TRANSACTION(flush, &(cstat));
		}
		LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Finished GetVolumeHeader()");
		if (cstat){
   		    LogMsg(0, VolDebugLevel, stdout, "VGetVolume: WriteVolumeHeader failed!");
		    assert(0);
		}
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
			LogMsg(0, VolDebugLevel, stdout, "Volume %x: couldn't reread volume header",
								vp->hashid);
		    FreeVolume(vp);
		    vp = 0;
		    break;
		}
	    }
	}
	
    /* temp debugging stuff */
	LogMsg(39, VolDebugLevel, stdout, "VGetVolume: partition name for volume %x is %s",
				V_id(vp), V_partname(vp));
	assert(stat(V_partname(vp), &status) == 0);
	LogMsg(39, VolDebugLevel, stdout, "VGetVolume: vp->device = %u, disk.device = %u",
				V_device(vp),  status.st_dev);

	if (vp->shuttingDown) {
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: volume %x is shutting down",
		V_id(vp));
	    *ec = VNOVOL;
	    vp = 0;
	    break;
	}
	if (vp->goingOffline) {
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Volume %x is going offline",
		V_id(vp));
	    LWP_WaitProcess((char *)VPutVolume);
	    continue;
	}
	if (vp->specialStatus){
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Volume %x has special status",
		V_id(vp));
	    *ec = vp->specialStatus;
	}
	else if (V_inService(vp)==0 || V_blessed(vp)==0){
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Vol %x not in service",
		V_id(vp));
	    *ec = VNOSERVICE; /* Either leave vp set or do ReleaseVolHeader */
	    /* Not sure which is better... */
	}
	else if (V_inUse(vp)==0){
	    LogMsg(29, VolDebugLevel, stdout, "VGetVolume: Vol %x is offline", V_id(vp));
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

    LogMsg(9, VolDebugLevel, stdout, "Entering VPutVolume for volume %x", V_id(vp));
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    assert(--(vp->nUsers) >= 0);
    if (vp->nUsers == 0) {
        Error error;
	/* this is bogus- we're freeing it then referencing it ***ehs***7/88*/
	ReleaseVolumeHeader(vp->header);    /* return it to the lru queue */
        if (vp->goingOffline) {
	    assert(*pt == fileServer);
	    vp->goingOffline = 0;
	    V_inUse(vp) = 0;
	    LogMsg(1, VolDebugLevel, stdout, "VPutVolume: writing volume %x; going offline", V_id(vp));
	    VUpdateVolume(&error, vp);	 /* write out the volume disk data */
	    if (VolDebugLevel) {
		LogMsg(0, VolDebugLevel, stdout, "VPutVolume: Volume %x (%s) is now offline",
		    V_id(vp), V_name(vp));
		if (V_offlineMessage(vp)[0])
		    LogMsg(0, VolDebugLevel, stdout, " (%s)", V_offlineMessage(vp));
		LogMsg(0, VolDebugLevel, stdout, "");
	    }
	    LWP_SignalProcess((char *)VPutVolume);
	}
	if (vp->shuttingDown) {
	    FreeVolume(vp);
	    if (*pt == fileServer)
	        LWP_SignalProcess((char *)VPutVolume);
	}
    }
    else LogMsg(1, VolDebugLevel, stdout, "VPutVolume (%x): NO-OP since vp->nUsers = %d", 
						V_id(vp), vp->nUsers + 1);
}

/* Force the volume offline, set the salvage flag.  No further references to */
/* the volume through the volume package will be honored. */
void VForceOffline(Volume *vp)
{
    Error error;

    LogMsg(9, VolDebugLevel, stdout, "Entering VForceOffline() for volume %x", V_id(vp));
    if (!V_inUse(vp))
       return;
    strcpy(V_offlineMessage(vp), "Forced offline due to internal error: volume needs to be salvaged");
    LogMsg(0, VolDebugLevel, stdout, "Volume %x forced offline:  it needs to be salvaged!", V_id(vp));
    V_inUse(vp) = 0;
    LogMsg(1, VolDebugLevel, stdout, "VForceOffline: setting V_inUse(%x) = 0 and writing out voldiskinfo", V_id(vp));
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

    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    LogMsg(9, VolDebugLevel, stdout, "Entering VOffline for volume %x, running as %s",
			V_id(vp), (*pt == fileServer)?"fileServer":((*pt == volumeUtility)?"volumeUtility":"fileUtility"));

    /* if called by volumeUtility, have fileserver put volume */
    /* back on line */
    if (*pt == volumeUtility) {
	LogMsg(9, VolDebugLevel, stdout, "VOffline: volumeUtility relinquishing volume %x to fileServer",
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
    int notifyServer;
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Entering VDetachVolume() for volume %x", V_id(vp));
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
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

    LogMsg(9, VolDebugLevel, stdout, "VAllocBitmapEntry: volume = %x, count = %d, stride = %d, ix = %d",
	 V_id(vp), count, stride, ix);
    assert(count > 0 && stride > 0 && ix >= 0);

    if (index->bitmap == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "VAllocBitmapEntry: uninitialized bitmap");
	*ec = VFAIL;
	return(0);
    }

    LogMsg(19, VolDebugLevel, stdout, "VAllocBitmapEntry: bitmapOffset = %d, bitmapSize = %d",
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
    LogMsg(19, VolDebugLevel, stdout, "VAllocBitmapEntry: sbn = %d (bbn = %d, ebn = %d)", sbn, bbn, ebn);

    /* Compute the number of bitmap bytes needed to satisfy this allocation, and grow the map if needed. */
    int newsize = (((sbn + (count - 1) * stride) / 32 + 1) * 4);
    int growsize = (newsize - index->bitmapSize);
    if (growsize > 0) {
	if (growsize < VOLUME_BITMAP_GROWSIZE) {
	    growsize = VOLUME_BITMAP_GROWSIZE;
	    newsize = index->bitmapSize + growsize;
	}

	LogMsg(1, VolDebugLevel, stdout, "VAllocBitmapEntry: realloc'ing from %x to %x",
	    index->bitmapSize, newsize);
	index->bitmap = (byte *)realloc(index->bitmap, newsize);
	assert(index->bitmap != NULL);
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
	assert((*cbp & mask) == 0);
	*cbp |= mask;
    }

    /* Update the bitmapOffset if necessary. */
    while (bp < ep && *((bit32 *)bp) == 0xffffffff)
	bp = (byte *)((bit32 *)bp + 1);

    return(sbn);
}

int VAllocBitmapEntry(Error *ec, Volume *vp, struct vnodeIndex *index, VnodeId vnode) {
    *ec = 0;

    LogMsg(9, VolDebugLevel, stdout, "VAllocBitmapEntry: volume = %x, vnode = %x",
	 V_id(vp), vnode);

    if (index->bitmap == NULL) {
	*ec = VFAIL;
	LogMsg(0, VolDebugLevel, stdout, "VAllocBitmapEntry: uninitialized bitmap");
	return(0);
    }

    LogMsg(19, VolDebugLevel, stdout, "VAllocBitmapEntry: bitmapOffset = %d, bitmapSize = %d",
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
	assert(growsize >= 4);
	if (growsize < VOLUME_BITMAP_GROWSIZE) {
	    growsize = VOLUME_BITMAP_GROWSIZE;
	    newsize = index->bitmapSize + growsize;
	}

	LogMsg(1, VolDebugLevel, stdout, "VAllocBitmapEntry: realloc'ing from %x to %x",
	    index->bitmapSize, newsize);
	index->bitmap = (byte *)realloc(index->bitmap, newsize);
	assert(index->bitmap != NULL);
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

    LogMsg(9, VolDebugLevel, stdout, "Entering VFreeBitMapEntry() for bitNumber %d", bitNumber);
     offset = bitNumber>>3;
     if (offset > index->bitmapSize) {
	*ec = VNOVNODE;
	return;
     }
     if (offset < index->bitmapOffset)
     	index->bitmapOffset = offset&~3;	/* Truncate to nearest bit32 */
     *(index->bitmap + offset) &= ~(1 << (bitNumber & 0x7));
}

/* If the uniquifier needs to be bumped the CALLER MUST do that ! */
/* Write out volume disk data  (ok)*/
/* This must be called from within a transaction */
void VUpdateVolume(Error *ec,Volume *vp)
{
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Entering VUpdateVolume() for volume %x", V_id(vp));
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    *ec = 0;
/*
    if (*pt == fileServer) 
   	V_uniquifier(vp) = (V_inUse(vp)? V_nextVnodeUnique(vp) + 200: V_nextVnodeUnique(vp));
*/
    WriteVolumeHeader(ec, vp);
    if (*ec) {
	LogMsg(0, VolDebugLevel, stdout, 	  "VUpdateVolume: error updating volume header, volume %x (%s)",
	    V_id(vp), V_name(vp));
        VForceOffline(vp);
    }
}

/*
  BEGIN_HTML
  <a name="PrintVolumesInHashTable">
  <strong>Print summary of all volumes in the hash table
  </strong>
  </a>
  END_HTML 
 */
void PrintVolumesInHashTable()
{
    if (VolDebugLevel < 50)
	return;
    for (int i = 0; i < VOLUME_HASH_TABLE_SIZE; i++){
	printf("PrintVolumesInHashTable: Lookint at index %d\n", i);
	Volume *vp = VolumeHashTable[i];
	while(vp){
	    printf("PVInHT: volume id %x hashid %u is in HT\n",
		   V_id(vp), vp->hashid);
	    vp = vp->hashNext;
	}
    }
}

void FreeVolume(Volume *vp)
{
    int i;
    ProgramType *pt;

    LogMsg(9, VolDebugLevel, stdout, "Entering FreeVolume for volume %x", V_id(vp));
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
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
    struct VnodeDiskObject *vnode;
    register Unique_t unique = 0;
    int bitNumber = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering GetBitmap() for volume %x, vclass = %d",
						    V_id(vp), vclass);
    *ec = 0;
    vindex vol_index(V_id(vp), vclass, vp->device, vcp->diskSize);
    vindex_iterator vnext(vol_index);
    int slots = vol_index.elts();

#ifdef notdef
    vip->bitmapSize = ((slots/8)+10)/4*4; /* The 10 is a little extra so
    				a few files can be created in this volume,
				the whole thing is rounded up to nearest 4
				bytes, because the bit map allocator likes
				it that way */
#endif notdef

    /* The vnode array size (slots / 8) must be a multiple */
    /* of 4 bytes! */
    vip->bitmapSize = slots / 8;    /* in bytes */

    vnode = (VnodeDiskObject *)bigbuf;

    vip->bitmap = (byte *) malloc(vip->bitmapSize);
    LogMsg(9, VolDebugLevel, stdout, "GetBitmap: allocating bitmap of %d bytes; array size %d",
					    vip->bitmapSize, slots);
    assert(vip->bitmap != NULL);
    bzero(vip->bitmap, vip->bitmapSize);
    vip->bitmapOffset = 0;
    if ((vip->bitmapSize << 3) > slots) {
	LogMsg(1, VolDebugLevel, stdout, "GetBitmap: bitmapSize = %d bits, only %d array slots",
		(vip->bitmapSize << 3), slots);
	GrowVnodes(V_id(vp), vclass, vip->bitmapSize);
    }

    /* iterate through all vnodes in specified class index */
    bitmap_flag = 1;
    while ((bitNumber = vnext(vnode)) != -1) {
        if (vnode->vnodeMagic != vcp->magic) {
	    LogMsg(0, VolDebugLevel, stdout, "GetBitmap: addled vnode index in volume %s; volume needs salvage", V_name(vp));
	    LogMsg(0, VolDebugLevel, stdout, "GetBitmap: vnodeMagic = 0x%x; vcp magic = 0x%x", 
		vnode->vnodeMagic, vcp->magic);
	    LogMsg(0, VolDebugLevel, stdout, "GetBitmap: Printing vnode(addr: 0x%x) at bitnumber = 0x%x",
		vnode, bitNumber);
	    print_VnodeDiskObject(vnode);
	    LogMsg(0, VolDebugLevel, stdout, "GetBitmap: Print Vnode returned");
	    *ec = VSALVAGE;
	    break;
	}
	LogMsg(29, VolDebugLevel, stdout, "GetBitmap: found vnode at index %d", bitNumber);
	*(vip->bitmap + (bitNumber>>3)) |= (1 << (bitNumber & 0x7));
	LogMsg(29, VolDebugLevel, stdout, "results of or: *(vip->bitmap + bitNumber >> 3) = %o",
				*(vip->bitmap + (bitNumber >> 3)));
	if (unique <= vnode->uniquifier)
	    unique = vnode->uniquifier + 1; 
    }

    if (vp->nextVnodeUnique < unique) {
	LogMsg(0, VolDebugLevel, stdout, "GetBitmap: bad volume uniquifier for volume %s; volume needs salvage", V_name(vp));
	*ec = VSALVAGE;
    }
}

int VolumeNumber(char *name)
{
    LogMsg(9, VolDebugLevel, stdout, "Entering VolumeNumber for volume %s", name);
    if (*name == '/')
        name++;
    return atoi(name+1);
}

char *VolumeExternalName(VolumeId volumeId)
{
    static char name[15];
    LogMsg(9, VolDebugLevel, stdout, "Entering VolumeExternalName for volume %x", volumeId);
    sprintf(name,VFORMAT,volumeId);
    return name;
}

#define OneDay	(24*60*60)		/* 24 hours */
#define Midnight(date) ((date-TimeZoneCorrection)/OneDay*OneDay+TimeZoneCorrection)

static void VAdjustVolumeStatistics(register Volume *vp)
{
    unsigned int now = FT_ApproxTime();

    LogMsg(9, VolDebugLevel, stdout, "Entering VAdjustVolumeStatistics for volume %x", V_id(vp));
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
    int status = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering VBumpVolumeUsage for volume %x", V_id(vp));
    if (now - V_dayUseDate(vp) > OneDay)
	VAdjustVolumeStatistics(vp);
    if ((V_dayUse(vp)++ & 127) == 0) {
	Error error;
	LogMsg(1, VolDebugLevel, stdout, "VBumpVolumeUsage: writing out VolDiskInfo (vol %x)",
			V_id(vp));
	RVMLIB_BEGIN_TRANSACTION(restore)
	VUpdateVolume(&error, vp);
	RVMLIB_END_TRANSACTION(flush, &(status));
    }
}

void VSetDiskUsage() {
    static FifteenMinuteCounter;

    LogMsg(9, VolDebugLevel, stdout, "Entering VSetDiskUsage()");
    VResetDiskUsage();
    if (++FifteenMinuteCounter == 3) {
	FifteenMinuteCounter = 0;
        VScanUpdateList();
    }
}

/* The number of minutes that a volume hasn't been updated before the "Dont salvage" flag in
   the volume header will be turned on */

#define SALVAGE_INTERVAL	(10*60)

static VolumeId *UpdateList;	/* Pointer to array of Volume ID's */
static int nUpdatedVolumes;	/* Updated with entry in UpdateList, salvage after crash flag on */
static int updateSize;		/* number of entries possible */
#define UPDATE_LIST_SIZE 100	/* size increment */

void VAddToVolumeUpdateList(Error *ec, register Volume *vp)
{
    LogMsg(9, VolDebugLevel, stdout, "Entering VAddToVolumeUpdateList for volume %x", V_id(vp));

    *ec = 0;
    vp->updateTime = FT_ApproxTime();
    if (V_dontSalvage(vp) == 0){
	LogMsg(29, VolDebugLevel, stdout, "VAddToVolumeUpdateList: salvage was 0 - so not updating volume");
	LogMsg(29, VolDebugLevel, stdout, "Leaving VAddToVolumeUpdateList()");
	return;
    }
    V_dontSalvage(vp) = 0;

    int camstatus = 0;
    VUpdateVolume(ec, vp);

    if (*ec || camstatus){
	LogMsg(29, VolDebugLevel, stdout, "VAddToVolumeUpdateList: Error during Update Volume; returning");
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
    LogMsg(29, VolDebugLevel, stdout, "VAddToVolumeUpdateList: Adding volume %x to update list", 
	 V_id(vp));
    UpdateList[nUpdatedVolumes++] = V_id(vp);
    LogMsg(29, VolDebugLevel, stdout, "Leaving VAddToVolumeUpdateList()");
}

static void VScanUpdateList() {
    register int i, gap;
    register Volume *vp;
    Error error;
    long now = FT_ApproxTime();

    LogMsg(9, VolDebugLevel, stdout, "Entering VScanUpdateList()");
    /* Be careful with this code, since it works with interleaved calls to AddToVolumeUpdateList */
    for (i = gap = 0; i<nUpdatedVolumes; i++) {
	LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Going to call VGetVolume ");
        vp = VGetVolume(&error, UpdateList[i-gap] = UpdateList[i]);
	LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Just returned from VGetVolume");
	if (error) {
	    LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Error %d in VGetVolume", error);
	    gap++;
	}
	else if (vp->nUsers == 1 && now - vp->updateTime > SALVAGE_INTERVAL) {
	    LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Going to set salvage flag for volume %x", V_id(vp));
	    int cstat = 0;
	    V_dontSalvage(vp) = DONT_SALVAGE;
	    RVMLIB_BEGIN_TRANSACTION(restore)
	    VUpdateVolume(&error, vp); /* No need to fsync--not critical */
	    RVMLIB_END_TRANSACTION(flush, &(cstat));
	      LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Finished UPdating Volume %x",
		  V_id(vp));
	    gap++;
	}
	if (vp){
	    LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: Going to Put volume %x", 
		V_id(vp));
	    VPutVolume(vp);
	}
	LWP_DispatchProcess();
    }
    nUpdatedVolumes -= gap;
    LogMsg(29, VolDebugLevel, stdout, "ScanUpdateList: nUpdatedVolumes = %d",
	 nUpdatedVolumes);
    LogMsg(29, VolDebugLevel, stdout, "Leaving ScanUpdateList()");
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

/* Get a volume header from the LRU list; update the old one if necessary */
/* Returns 1 if there was already a header, which is removed from the LRU list */
static int GetVolumeHeader(register Volume *vp)
{
    Error error;
    register struct volHeader *hd;
    int old;

    LogMsg(9, VolDebugLevel, stdout, "Entering GetVolumeHeader()");
    old = (vp->header != 0);

    if (old) {
	hd = vp->header;
	if (volumeLRU == hd)
	    volumeLRU = hd->next;
	assert(hd->back == vp);
    }
    else {
	hd = volumeLRU->prev; /* not currently in use and least recently used */
	if (hd->back) {
	    if (hd->diskstuff.inUse) {
		LogMsg(1, VolDebugLevel, stdout, "storing VolumeDiskInfo (%x) to reclaim cache slot",
			    hd->diskstuff.id);
		WriteVolumeHeader(&error, hd->back);
		/* Ignore errors; catch them later */
	    }
	    hd->back->header = 0;
	}
	hd->back = vp;
    	vp->header = hd;
    }
    LogMsg(29, VolDebugLevel, stdout, "GVH: setting hd->prev->next = ");
    hd->prev->next = hd->next;
    LogMsg(29, VolDebugLevel, stdout, "%x", hd->prev->next);
    LogMsg(29, VolDebugLevel, stdout, "GetVolumeHeader: hd->next = 0x%x, hd->prev = 0x%x",
		    hd->next, hd->prev);
    LogMsg(29, VolDebugLevel, stdout, "GetVolumeHeader: hd->next->prev = ");
    LogMsg(29, VolDebugLevel, stdout, "0x%x", hd->next->prev);
    hd->next->prev = hd->prev;
    hd->next = hd->prev = 0;

    return old;
}

static int AvailVolumeHeader(register Volume *vp)
{
    register struct volHeader *hd;

    LogMsg(9, VolDebugLevel, stdout, "Entering AvailVolumeHeader()");

    if (vp->header == 0) {
	hd = volumeLRU->prev;/* not currently in use and least recently used */
	if (hd->back) {
	    if (hd->diskstuff.inUse) {
		LogMsg(29, VolDebugLevel, stdout, "AvailVolumeHeader returns 0");
		return(0);
	    }
	}
    }

    LogMsg(29, VolDebugLevel, stdout, "AvailVolumeHeader returns 1");
    return (1);
}

/* Put it at the top of the LRU chain */
static void ReleaseVolumeHeader(register struct volHeader *hd)
{
    LogMsg(61, VolDebugLevel, stdout, "Entering ReleaseVolumeHeader");
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

    LogMsg(9, VolDebugLevel, stdout, "Entering FreeVolumeHeader for volume %x", V_id(vp));
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
  BEGIN_HTML
  <a name="AddVolumeToHashTable">
  <strong>Add the volume (*vp) to the hash table
  </strong>
  </a>
  END_HTML 
 */
/* As used, hashid is always the id of the volume.  */
static void AddVolumeToHashTable(register Volume *vp, int hashid)
{
    int hash = VOLUME_HASH(hashid);
    Volume *vptr;
    LogMsg(9, VolDebugLevel, stdout, "Entering AddVolumeToHashTable for volume %x, hashid %u",
					V_id(vp), hashid);

    /* Do some sanity checking before performing insert. */
    if (hashid != V_id(vp)) {
	LogMsg(0, VolDebugLevel, stdout, "VolHashTable: hashid %x != V_id(vp).", hashid, V_id(vp));
	LogMsg(0, VolDebugLevel, stderr, "VolHashTable: hashid %x != V_id(vp).", hashid, V_id(vp));
    }

    vptr = VolumeHashTable[hash];	/* Check the bucket for duplicates. */
    while (vptr) {
	if (vptr->hashid == vp->hashid) {
	    LogMsg(0, VolDebugLevel, stdout, "VolHashTable: Adding another volume with id %x.", hashid);
	    LogMsg(0, VolDebugLevel, stderr, "VolHashTable: Adding another volume with id %x.", hashid);
	}
	vptr = vptr->hashNext;
    }

    vp->hashid = hashid;
    vp->hashNext = VolumeHashTable[hash];
    VolumeHashTable[hash] = vp;
    vp->vnodeHashOffset = VolumeHashOffset();
}    

/*
  BEGIN_HTML
  <a name="DeleteVolumeFromHashTable">
  <strong>Delete the Volume *vp from the hash table</a></strong>
  END_HTML 
 */
void DeleteVolumeFromHashTable(register Volume *vp)
{
    unsigned long hash = VOLUME_HASH(vp->hashid);

    LogMsg(9, VolDebugLevel, stdout, "Entering DeleteVolumeFromHashTable");

    if (vp->header)			  /* Put LRU entry back on queue */
	ReleaseVolumeHeader(vp->header);

    if (VolumeHashTable[hash] == vp){
	LogMsg(29, VolDebugLevel, stdout, "DeleteVolumeFromHashTable: Deleting volume %x from hash table",
	    vp->hashid);
	VolumeHashTable[hash] = vp->hashNext;
    }
    else {
	Volume *tvp = VolumeHashTable[hash];
	if (tvp == NULL)
	    return;
	while (tvp->hashNext && tvp->hashNext != vp)
	    tvp = tvp->hashNext;
	if (tvp->hashNext == NULL)
	    return;
	tvp->hashNext = vp->hashNext;
	LogMsg(29, VolDebugLevel, stdout, "DeleteVolumeHashTable: Deleting volume %x from hashtable", vp->hashid);
    }
    vp->hashid = 0;
}

void VPrintCacheStats(FILE *fp) {
    register struct VnodeClassInfo *vcp;
    vcp = &VnodeClassInfo_Array[vLarge];
    LogMsg(0, 0, fp, 
	   "Large vnode cache, %d entries, %d allocs, %d gets (%d reads), %d writes",
	   vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    vcp = &VnodeClassInfo_Array[vSmall];
    LogMsg(0, 0, fp, 
	   "Small vnode cache,%d entries, %d allocs, %d gets (%d reads), %d writes",
	   vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    LogMsg(0, 0, fp, 
	   "Volume header cache, %d entries, %d gets, %d replacements",
	   VolumeCacheSize, VolumeGets, VolumeReplacements);
}

void SetVolDebugLevel(int level) {
    VolDebugLevel = level;
}

static int MountedAtRoot(char *path) {
    /* Returns 1 if path is a subdirectory of  "/"-directory, 0 otherwise */

    struct stat rootbuf, pathbuf;

    /* Check exactly one slash, and in first position */
    if (rindex(path, '/') != path) return(0);

    /* Then compare root and path device id's */
    if (stat("/", &rootbuf)) {
	perror("/");
	return(0);
    }
    if (stat(path, &pathbuf)) {
	perror(path);
	return(0);
    }

    if (rootbuf.st_dev == pathbuf.st_dev) return(0);
    else return(1);
}

/* migrated here from partition.cc which was retired */

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
