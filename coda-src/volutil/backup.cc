#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University in all documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS KNOWN TO HAVE BUGS,
SOME OF WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON
DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE OR OF
ANY DERIVATIVE WORK.

Carnegie Mellon encourages users of this software to return any
improvements or extensions that they make, and to grant Carnegie
Mellon the rights to redistribute these changes without encumbrance.  */

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/backup.cc,v 4.17 1998/11/24 15:34:59 jaharkes Exp $";
#endif /*_BLURB_*/





/**************************************************/
/* 	backup.c				  */
/*	   - Coordinator for backup subsystem.	  */
/**************************************************/

/* To handle dead servers without waiting for such long timeouts to
 * occur, mark servers for which connections are broken as dead. Have
 * an asynchronous thread periodically poll the dead servers. When
 * making calls, only send the rpc if we think the server is up.  */

/* Would it be possible to increase availability by unlocking the
   volume when the clone succeeds? There's no reason to keep it 
   locked after the state has been captured...  */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <netinet/in.h>
#include <netdb.h>
#ifdef __MACH__
#include <sys/fs.h>
#include <fstab.h>
#endif
#include <errno.h>
#include <strings.h>

#include <lwp.h>
#include <lock.h>
#include <timer.h>
#include <rpc2.h>
#include <se.h>
#include <sftp.h>
#include <histo.h>
#include <util.h>
#include <partition.h>
#include <vice.h>
#include "volutil.h"
#include <voldump.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <inconsist.h>
#include <cvnode.h>
#include <volume.h>
#include <vrdb.h>
#include <vldb.h>
#include <vsg.h>
#include <vutil.h>


static char vkey[RPC2_KEYSIZE+1];    /* Encryption key for bind authentication */
static int Timeout = 30;	      /* Default Timeout value in seconds. */
static int Debug = 0;		      /* Global debugging flag */
static int Naptime = 30;	      /* Sleep period for PollLWP */

struct hostinfo {
    bit32      	address;  /* Assume host IP addresses are 32 bits */
    RPC2_Handle rpcid;	  /* should be -1 if connection is dead. */
    char	name[36];
}  Hosts[N_SERVERIDS];

#define BADCONNECTION	(RPC2_Handle) -1

bit32 HostAddress[N_SERVERIDS];		/* Need these for macros in vrdb.c */
char *ThisHost;			/* This machine's hostname */
int ThisServerId = -1;	       	/* this server id, as found in /vice/db/servers */

/* Rock info for communicating with the DumpLWP. */
#define ROCKTAG 12345
struct rockInfo {
    char dumpfile[MAXPATHLEN];
    VolumeId volid;	      /* Volume being dumped. */
    unsigned long numbytes; /* Number of bytes already written to the file. */
} Rock;

struct hgram DataRate;			/* Statistics on rate of data transfer */
struct hgram DataTransferred;   	/* and size of dumpfiles. */

/* Per replica info. */
#define LOCKED 0x1
#define CLONED 0x2
#define DUMPED 0x4
#define MARKED 0x8

#define ISLOCKED(flags)	((flags) & LOCKED)
#define ISCLONED(flags)	((flags) & CLONED)
#define ISDUMPED(flags)	((flags) & DUMPED)
#define ISMARKED(flags)	((flags) & MARKED)

typedef struct {
    VolumeId repvolId;		
    VolumeId backupId;		
    int serverNum;		/* Index of server in Hosts table. */
    int flags;			/* Stage of backup for this replica. */
    ViceVersionVector vvv;
} repinfo_t;

/* Per volume info. */
#define INCREMENTAL 0x10
#define REPLICATED  0x20
#define BADNESS	    0x40	/* Indicates an operation on a replica
				   failed.  * Used to signal a
				   retry. On the first pass, * dumps
				   should be attempted if any clone
				   succeeded * and marks on any
				   successful dump.  */
                            

typedef struct volinfo {
    struct volinfo *next;
    VolumeId volId;
    int flags;			/* type of volume and last successful op */
    unsigned char nReplicas;	/* Degree of replication. */
    unsigned char nCloned;	/* Number of replicas that were cloned */
    char comment[40];		/* Arbitrarily pick a size ... */
    repinfo_t *replicas;
} volinfo_t;


/* a dummy partition type "backup" is used by backup.cc 
 * leveraging on the partition package 
 */
struct DiskPartition *DiskPartitionList = NULL;
struct DiskPartition *Partitions = NULL;

/* Procedure definitions. */
static void V_InitRPC();
static void V_BindToServer(char *fileserver, RPC2_Handle *RPCid);
static void VolDumpLWP(struct rockInfo *rock);
static void PollLWP(int naptime);
static void PollServers();
static int lockReplicas(volinfo_t *vol);
static void unlockReplicas(volinfo_t *vol);
static int backup(volinfo_t *vol);
static void VUInitServerList();
extern long volDump_ExecuteRequest(RPC2_Handle, RPC2_PacketBuffer*,
				  SE_Descriptor*);


/* get_volId parses the VolumeList file to obtain the volId and
 * whether to do a full dump or an incremental dump of the volume. It
 * returns zero if no errors occured.  */

#define LINELENGTH 81
int getVolId(FILE *VolumeList, VolumeId *volId, int *flags, char *comment)
{
    char incstr[40];
    char string[LINELENGTH];
    int i;

    if (fgets(string, LINELENGTH, VolumeList) == NULL) 
	return -1;

    /* Check for a newline. */
    for (i = LINELENGTH - 1; i > 0 && string[i] != '\n'; i--);
    if (string[i] != '\n') {
	LogMsg(0, 0, stdout, "Input line to long: %s\n", string);
	return(-1);
    }
	
    if (sscanf(string, "%x %40s %40s", volId, incstr, comment) != 3) {
	if (!feof(VolumeList)) 
	    LogMsg(3, Debug, stdout, "Bad input line, -%s-\n", string);
	return(-1);
    }

    /* To determine the place in the dump cycle, first number today
     * modulo the length of the cycle. Cycles of size 7 are offset so
     * Sunday is day 0.  */
    struct timeval tp;
    gettimeofday(&tp, 0);
    long day = tp.tv_sec / 86400; /* Find count of days (int div by seconds/day)*/
    day -= 3;			 /* Make day % 7 == 0 if day == sunday. */
    day %= strlen(incstr);
    char I = incstr[day];

    /* Cycle should only have full and incremental dumps */
    if (I != 'I' && I != 'i' && I != 'F' && I != 'f') { /* Restrict options. */
	LogMsg(0, 0, stdout, "Bad dump option %c\n", I);
	return(-1); 
    }
    
    *flags |= ((I == 'I') || (I == 'i'))? INCREMENTAL : 0;
    return 0;
}


/* The rpcid is either bound already (we have a connection) or we
 * should attempt to bind to the server. I am assuming the vldb entry
 * has the server storage information for r/w volumes in the first
 * slot (0).  */

RPC2_Handle getrpcId(struct vldb *vldbp) {
    /* Maybe we already have it? */
    if (Hosts[vldbp->serverNumber[0]].rpcid > 0) { 
	return Hosts[vldbp->serverNumber[0]].rpcid;
    }

    /* or we already failed? -- let PollLWP have it. */
    if (Hosts[vldbp->serverNumber[0]].rpcid == BADCONNECTION) { 
	return 0;	/* Not an error as such. */
    }
    
    if (Hosts[vldbp->serverNumber[0]].address == 0) { /* No such Host */
	LogMsg(0, 0, stdout, "Host %d doesn't exist!\n", vldbp->serverNumber[0]);
	return -1;
    }

    V_BindToServer(Hosts[vldbp->serverNumber[0]].name, 
		   &Hosts[vldbp->serverNumber[0]].rpcid);
   

    if (Hosts[vldbp->serverNumber[0]].rpcid > BADCONNECTION) {
	/* Set up a directory for this server. */
	if (mkdir(Hosts[vldbp->serverNumber[0]].name, 0755) != 0) {
	    LogMsg(0, 0, stderr, "Couldn't make a directory for %s, exiting.\n",
		   Hosts[vldbp->serverNumber[0]].name);
	    exit(-1);
	}
    } else
	LogMsg(0, 0, stdout, "Could not establish a connection with %s",
	       Hosts[vldbp->serverNumber[0]].name);
    return 0;
}    

/* Given the id of a replica, look up the volume information in the
 * VLDB.  Fill in the repinfo_t structure, returning the connectionid
 * of the server.  */

getReplica(repinfo_t *rep) {
    char volIdstr[11];		    /* The ascii (decimal) version of the VolId */
    struct vldb *vldbp;			/* Pointer to a vldb entry */
    VolumeId volId = rep->repvolId;
    rep->serverNum = 0;
    
    sprintf(volIdstr, "%u", volId);
    vldbp = VLDBLookup(volIdstr);
    if (vldbp == NULL) {
	LogMsg(0, 0, stdout, "Volume replica %x doesn't exist!\n", volId);
	return -1;
    }

    if (vldbp->volumeType != readwriteVolume) {
	LogMsg(0, 0, stderr, "Volume replica %x has wrong type %d!\n", volId,vldbp->volumeType);
	return -1;
    }

    rep->serverNum = vldbp->serverNumber[0];
    return getrpcId(vldbp);
}


/* change the partition name entry to have the todayName appended */
int PreparePartitionEntries(struct DiskPartition *part)
{
    long t = time(0);
    char todayName[11];
    char today[10];
    struct DiskPartition *dp = 0;
    time_t now = time(0);
    char *name = dp->name;
    
    strftime(today, sizeof(today), "%d%b%Y", localtime(&now));
    sprintf(todayName, "/");
    strcat(todayName, today);

    CODA_ASSERT(part);

    for (dp = part; dp ; dp = dp->next) {
	name = dp->name;
	if ((strlen(todayName) + strlen(name) < sizeof(dp->name))) {
	    strcat(dp->name, todayName);
	    if (mkdir(dp->name, 0755) != 0) {
		LogMsg(0, 0, stdout, 
		       "Error '%s' creating directory %s.", 
		       strerror(errno), dp->name);
		return -1;
	    }
	} else {
	    LogMsg(0, 0, stdout, "Name too long! %s", dp->name);
	    return -1;
	}
    }

    return 0;
}

/* Parse the VolumeList file to get the list volumes to dump */
int ParseDumpList(char *VolumeListFile, volinfo_t **vols)
{
    *vols = NULL;
    
    FILE *VolumeList = fopen(VolumeListFile, "r");
    if (VolumeList == NULL) {
	LogMsg(0, 0, stderr, "Error opening VolumeList file %s.", VolumeListFile);
        return -1;
    }
    
    /* Read in the list of volumes to be backed up from the input
       file. */
    /* Note: this list will be in the reverse order from the list in
       the file. */

    volinfo_t *vol, *Vols = 0x0;
    
    VolumeId volId;	/* To hold the volumeId of the volume to be backed up */
    int flags = 0;
    char comment[40];
    while (getVolId(VolumeList, &volId, &flags, comment) == 0) {
	vol = (volinfo_t *) malloc(sizeof(volinfo_t));

	bzero((char *)vol, sizeof(volinfo_t));		/* Initialize the structure */
	vol->volId = volId;
	vol->flags |= flags;
	strncpy(vol->comment, comment, 40);
	
	vrent *vre = VRDB.find(volId);	/* Locate the VRDB entry */
	
	if (vre == NULL) { 		/* It's not a replicated volume */
	    vol->flags &= ~REPLICATED;	/* Mark volume as non-replicated */
	    vol->nReplicas = 1;		/* Use replica list to hold server info */
	    vol->replicas = (repinfo_t *) malloc(sizeof(repinfo_t));
	    
	    bzero((char *)vol->replicas, sizeof(repinfo_t));

	    vol->replicas[0].repvolId = volId;
	    if (getReplica(vol->replicas) == -1) {
		LogMsg(0, 0, stdout, "Skipping backup for volume %x\n", vol->volId);
		free(vol);
		flags = 0;		/* Reset the flags */
		continue;
	    }
	    
	} else {
	    vol->flags |= REPLICATED;	/* Mark volume as replicated */
	    vol->nReplicas = vre->nServers;
	    vol->replicas=(repinfo_t *)malloc(vol->nReplicas * sizeof(repinfo_t));

	    bzero((char *)vol->replicas, vol->nReplicas * sizeof(repinfo_t));

	    for (char i = 0; i < vol->nReplicas; i++) {
		vol->replicas[i].repvolId = vre->ServerVolnum[i];
		if (getReplica(&(vol->replicas[i])) == -1) {
		    LogMsg(0, 0, stdout, "Skipping backup for volume %x\n", vol->volId);
		    free(vol);
		    vol = 0;
		    break;	/* We're skipping this volume completely. */
		}
	    }
	}

	if (vol) {
	    vol->next = Vols;		/* Add volume to the list */
	    Vols = vol;
	}
	flags = 0;			/* Reset the value. */
    }

    *vols = Vols;
    return 0;
}



/* Routines to do the RPC2 calls to the server. */
/*
 * Lock all the replicas of a volume.
 */
static int lockReplicas(volinfo_t *vol)
{
    long rc = 0;
    repinfo_t *reps = vol->replicas;
    
    for (char i = 0; i < vol->nReplicas; i++) {
	if (Hosts[reps[i].serverNum].rpcid != BADCONNECTION) {
	    rc = VolLock(Hosts[reps[i].serverNum].rpcid, reps[i].repvolId, &reps[i].vvv);
	    if (rc != RPC2_SUCCESS) {
		LogMsg(0, 0, stdout, "VolLock (%x) failed on volume %x replica %x with %s\n",
		       Hosts[reps[i].serverNum].rpcid, /* For debugging. */
		       vol->volId, reps[i].repvolId, RPC2_ErrorMsg((int)rc));

		if (rc < RPC2_FLIMIT) /* Connection has been nixed by RPC2 */
		    Hosts[reps[i].serverNum].rpcid = BADCONNECTION; 

		/* If a call fails, we should clear the flags, since LOCKING
		 * is the first stage. However, we can continue with the backup
		 * since optimistically the quorum for backup is 1.
		 */
		reps[i].flags = 0;     
	    } else
		/* Set this flag to force retry of all other operations */
		reps[i].flags = LOCKED;	
	}	
    }
    return 0;
}

/*
 * Unlock all the replicas of a volume. We dont care if one fails, just report it.
 */
static void unlockReplicas(volinfo_t *vol)
{
    long rc;
    repinfo_t *reps = vol->replicas;
    
    for (char i = 0; i < vol->nReplicas; i++) {
	/* Try to unlock all replicas, whether we think they're locked or not. */
	if (Hosts[reps[i].serverNum].rpcid != BADCONNECTION) {
	    rc= VolUnlock(Hosts[reps[i].serverNum].rpcid, reps[i].repvolId);
	    if ((rc != RPC2_SUCCESS) && (rc != EINVAL)) {
		LogMsg(0, 0, stdout, "VolUnlock failed on volume %x replica %x with %s\n",
		       vol->volId, reps[i].repvolId, RPC2_ErrorMsg((int)rc));
		LogMsg(0, 0, stdout, "VolUnlock failed, connection %d",
		       Hosts[reps[i].serverNum].rpcid);
		       
		if (rc < RPC2_FLIMIT)	/* Connection has been nixed by RPC2 */
		    Hosts[reps[i].serverNum].rpcid = BADCONNECTION;
	    } else
		reps[i].flags &= ~LOCKED;
	    
	}
    }
}

/* Clone the replicas. First lock as many as you can. Only clone those
 * that were locked. Although this may not capture latest state, all
 * the assumptions that make optomistic replication work also apply
 * here. Unlock all the replicas after cloning. Store the # of cloned
 * replicas in the volinfo_t.  */
static int backup(volinfo_t *vol) {
    VolumeId volId = vol->volId;
    int Incremental = (vol->flags & INCREMENTAL);
    repinfo_t *reps = vol->replicas;
    long rc;
    int count = 0;
    
    LogMsg(0, 0, stdout, "%x: cloning ", volId);

    if (lockReplicas(vol) != 0) 		/* Lock all the replicas */
	return -1;

    for (char i = 0; i < vol->nReplicas; i++) {

	/* Only clone a replica if it was locked and its server is up. */
	if ((Hosts[reps[i].serverNum].rpcid != BADCONNECTION) && ISLOCKED(reps[i].flags)) {
	    rc = VolMakeBackups(Hosts[reps[i].serverNum].rpcid,
				reps[i].repvolId, &reps[i].backupId);
	    if (rc != RPC2_SUCCESS) {
		LogMsg(0,0, stdout, "VolBackup (%x) failed on replica %x with %s\n",
		       Hosts[reps[i].serverNum].rpcid, /* For debugging. */
		       reps[i].repvolId, RPC2_ErrorMsg((int)rc));

		if (rc < RPC2_FLIMIT) /* Connection has been nixed by RPC2 */
		    Hosts[reps[i].serverNum].rpcid = BADCONNECTION; 

		/* I think we can safely continue if some replica(s) fails.
		 * The worst that happens is that some extra clones are done.
		 */
	    }
	    else {
		LogMsg(0, 0, stdout, "\t%08x->%08x", reps[i].repvolId, reps[i].backupId);
		count++;
		reps[i].flags |= CLONED;
	    }
	} 
     }

    /* Don't need to unlock anymore, S_VolBackup does it as a side-effect.
     *  unlockReplicas(vol);
     */
    
    vol->nCloned = count;			/* Store # of successful clones */
    CODA_ASSERT(count <= vol->nReplicas);
    return 0;
}

struct DiskPartition *findBestPartition(struct DiskPartition *Parts) 
{
    unsigned long space;
    struct DiskPartition *best, *part = 0;

    CODA_ASSERT(Parts);

    best = Parts;
    space = Parts->free;
    for (part = Parts; part ; part = part->next) {
	if (part->free > space) {
	    best = part;
	    space = part->free;
	}
    }

    return best;
}

/* Turns out the VVV isn't maintained across repairs or resolves. So
 * after either of these operations, the VVVs won't agree. In
 * addition, it is unclear how to deal when for one incremental the
 * VVVs agree and for another in the same series they don't. For the
 * second incremental, a vvlist file won't exist, so it'll look like a
 * full and the first incremental won't be in the ordering, although
 * it'll appear to the user like it should be since it succeded. A fix
 * to this would be to force the vvlist file over as part of
 * MarkAsAncient, or to produce the vvlist file during cloning. For
 * now though, I'm turning off this optimization.
 *
 * The Incremental flag I is passed by reference since the server can
 * force an incremental to be a full if the *ancient* file cannot be
 * found. If this is the case, we are notified so that today's tape
 * will be saved.
 *
 * Here's the deal: for each replica, create a dump file on the
 * emptiest partition in Parts (or in the current directory if no
 * Parts were specified).  If the replica wasn't cloned, or was
 * already dumped, skip it.  */

int dumpVolume(volinfo_t *vol, struct DiskPartition  *Parts)
{
    repinfo_t *reps = vol->replicas;
    VolumeId volId = vol->volId;
    long rc;
    RPC2_Unsigned I = (vol->flags & INCREMENTAL);
    int ndumped = 0;
    
    for (int i = 0; i < vol->nReplicas; i++) {
	if ((Hosts[reps[i].serverNum].rpcid == BADCONNECTION) ||
	    !ISCLONED(reps[i].flags))
	    continue;	
	
	if (ISDUMPED(reps[i].flags)) {
	    ndumped++; /* Count it, but don't need to redo it. */
	    continue;
	}
	
	CODA_ASSERT(reps[i].backupId > 0);
	
	/* get the name of the dumpfile. */
	struct DiskPartition *part = NULL;
	char buf[MAXPATHLEN];

	if (Parts == 0) {	/* No partitions specified. */
	    if (vol->flags & REPLICATED)
		sprintf(buf,"%s/%x.%x",Hosts[reps[i].serverNum].name, volId, reps[i].repvolId);
	    else
		sprintf(buf, "%s/%x", Hosts[reps[i].serverNum].name, volId);
	} else {
	    /* No concurrency problems since LWP isn't pre-emptive. */
	    part = findBestPartition(Parts);
	    
	    if (vol->flags & REPLICATED) 
		sprintf(buf, "%s/%s-%x.%x", part->name,
			Hosts[reps[i].serverNum].name, volId, reps[i].repvolId);
	    else 
		sprintf(buf, "%s/%s-%x", part->name, Hosts[reps[i].serverNum].name, volId);
	}

	/* Remove the file if it already exists. Since we made the
	 * dump dir it can only exist if we are retrying the
	 * replicated dump even though it succeeded for this replica
	 * last time around. Don't care if it fails.  */
	unlink(buf);
	
	/* Setup the write thread to handle this operation. */
	CODA_ASSERT(strlen(buf) < sizeof(Rock.dumpfile));
	strcpy(Rock.dumpfile, buf);
	Rock.volid = reps[i].backupId;
	Rock.numbytes = 0;
	
	LogMsg(0, 0, stdout, "Dumping %x.%x to %s ...", volId, reps[i].repvolId, buf);

	rc = VolNewDump(Hosts[reps[i].serverNum].rpcid, reps[i].backupId, &I);
	if (rc != RPC2_SUCCESS) {
	    LogMsg(0,0,stdout, "VolDump (%x) failed on %x with %s\n",
		   Hosts[reps[i].serverNum].rpcid, /* For debugging. */
		   reps[i].backupId, RPC2_ErrorMsg((int)rc));
	    if (rc < RPC2_FLIMIT) /* Connection has been nixed by RPC2 */
		Hosts[reps[i].serverNum].rpcid = BADCONNECTION;
	    unlink(buf);	/* Delete the dump file. */
	    continue;
	}

	/* Incremental can be forced to be full. */	
	if (I == 0) vol->flags &= ~INCREMENTAL;
	
	if (Parts) {
	    /* Setup a pointer from the dump subtree to the actual dumpfile
	     * if a different partition was used for storage.
	     */
	    
	    char link[66];
	    if (vol->flags & REPLICATED)
		sprintf(link,"%s/%x.%x",Hosts[reps[i].serverNum].name,
			volId, reps[i].repvolId);
	    else
		sprintf(link, "%s/%x", Hosts[reps[i].serverNum].name, volId);
	    
	    /* Remove the link if it exists. See comment by previous
               unlink. */
	    unlink(link);
	    
	    if (symlink(buf, link) == -1) {
		if (errno == EEXIST) {	/* Retrying dump. */
		    if (unlink(link) != -1)
			if (symlink(buf, link) != -1)
			    break;
		}
		perror("symlink");
		unlink(buf);	/* Delete the dump file. */		
		return -1;
	    }

	    /* Reset diskusage for the partition that was used */
	    DP_SetUsage(part);
	}

	/* At this point, we know everything worked for this replica. */
	LogMsg(0, 0, stdout, "\t\tTransferred %d bytes\n", Rock.numbytes);
	double tmp = Rock.numbytes * 1.0;
	UpdateHisto(&DataTransferred, tmp);

	reps[i].flags |= DUMPED;
	ndumped++;
    }
    return ndumped;
}

/* At this point we're convinced that the Volume has been successfully
   * backed up.  Tell the server the backup was successful if the *
   backup * was a full.  */
MarkAsAncient(volinfo_t *vol) {
    long rc;
    repinfo_t *reps = vol->replicas;
    VolumeId volId;
    int nmarked = 0;  /* Count is how many have been done, not how many we did.*/
    
    volId = (vol->flags & REPLICATED)? vol->volId : 0;
    for (int i = 0; i < vol->nReplicas; i++) {

	/* If we've already done it, bump the count (for check) and skip it. */
	if (ISMARKED(reps[i].flags)) {
	    nmarked++;

	/* If it is dumped, unmarked, and a full -- mark it */
	} else if (ISDUMPED(reps[i].flags)) {

	    /* If the dump was incremental, it doesn't need to be
	     * marked. We now only mark fulls, so all incrementals are
	     * wrt the last full, not the last dump. But people think
	     * "M" indicates success, so set the flag to make the
	     * reporting look consistent.  */
	    if (vol->flags & INCREMENTAL) {
		nmarked++;
		reps[i].flags |= MARKED;
		continue;
	    }
		
	    if (Hosts[reps[i].serverNum].rpcid == BADCONNECTION)
		continue;

	    rc= VolMarkAsAncient(Hosts[reps[i].serverNum].rpcid, volId, reps[i].repvolId);
	    if (rc != RPC2_SUCCESS) {
		LogMsg(0,0,stdout, "VolMarkAsAncient (%x) failed on %x with %s\n", 
		       Hosts[reps[i].serverNum].rpcid, /* For debugging. */
		       reps[i].repvolId, RPC2_ErrorMsg((int)rc));
		if (rc < RPC2_FLIMIT) /* Connection has been nixed by RPC2 */
		    Hosts[reps[i].serverNum].rpcid = BADCONNECTION; 
		vol->flags |= BADNESS;
	        continue;
	    } else {
		reps[i].flags |= MARKED;
		nmarked ++;
	    }
	}
    }

    return nmarked;
}



int main(int argc, char **argv) {
    /* Process arguments */
    char *myname = argv[0];
    char *filename = NULL;
    char dumpdir[MAXPATHLEN];
    time_t now = time(0);
    char today[12];
    volinfo_t *vol; /* for-loop index variable */
    
    if (getuid() != 0) {
	LogMsg(0, 0, stdout, "Volume utilities must be run as root; sorry\n");
	exit(1);
    }



    dumpdir[0]='\0';
    while ( argc > 1) {			/* While args left to parse. */
	if (!strcmp(argv[1], "-t")) {
	    Timeout = atoi(argv[2]);
	    CODA_ASSERT(Timeout > 0);
	    argc -= 2; argv += 2;
	} else if (!strcmp(argv[1], "-p")) {
	    Naptime = atoi(argv[2]);
	    argc -= 2; argv += 2;
	} else if (!strcmp(argv[1], "-d")) {
	    Debug = atoi(argv[2]);
	    argc -= 2; argv += 2;
	} else if (filename == NULL) {/* Assume 1st unrecognized arg is filename*/
	    filename = argv[1];
	    argc--; argv++;
	} else if(dumpdir[0] == '\0') { /* Assume 2nd unrecognized arg is dumpdir */
	    strncpy(dumpdir, argv[1], MAXPATHLEN);
	    argc--; argv++;
	} else {	/* Must have had unrecognized input. */
	    printf("Usage: %s [-p pollPeriod] [-t timeout] [-d debuglevel] <dumpfile> [<backupdir>]\n", myname);
	    exit(1);
	}
    }
    
    if (filename == NULL) {	/* Definitely need a file for the VolumeList. */
	printf("Usage: %s [-p pollPeriod] [-t timeout] [-d debuglevel] <filename> [<backup dir>]\n", myname);
	exit(1);
    }



    strftime(today, sizeof(today), "%d%b%Y", localtime(&now));
    if (dumpdir) {	/* User specified a dump directory! */
	if (chdir(dumpdir) != 0) {
	    perror("Set dump directory.");
	    exit(-1);
	} else {
	    strcat(dumpdir, "/");
	    strcat(dumpdir, today);
	    if ( mkdir(dumpdir, 0700) == -1 ) {
		fprintf(stderr, "Cannot create %s:", dumpdir);
		perror("");
		exit(1);
	    } else
		chdir(dumpdir);
	}
    }

    /* Initialize */
    V_InitRPC();
    CheckVRDB();
    VUInitServerList();

    /* initialize the partitions */
    DP_Init("/vice/db/vicetab");
    Partitions = DiskPartitionList;
    /* change the name */
    if ( PreparePartitionEntries(Partitions) != 0 ) {
	eprint("Malformed partitions! Cannot prepare for dumping.");
	exit(1);
    }

    InitHisto(&DataRate, 0, 1048576, 20, LINEAR); /* 1 == LINEAR */
    InitHisto(&DataTransferred, 1, 104857600, 10, LOG10); /* 3 == Log 10 */

    /* Parse BackupList file. */
    volinfo_t *Volumes;
    if (ParseDumpList(filename, &Volumes) != 0) {
	eprint("Error parsing %s for volumes!", filename);
	exit(-1);
    }

    /* Start up thread to handle WriteDump requests. */
    PROCESS dumpPid;
    bzero((char *)&Rock, sizeof(struct rockInfo));
    LWP_CreateProcess((PFIC)VolDumpLWP, 5 * 1024, LWP_NORMAL_PRIORITY,
		      (char *)&Rock, "VolDumpLWP", &dumpPid);

    /* Start up thread to periodically poll down servers */
    PROCESS pollPid;
    LWP_CreateProcess((PFIC)PollLWP, 8 * 1024, LWP_NORMAL_PRIORITY - 1,
		      (char *)Naptime, "PollLWP", &pollPid);

    /* First try to backup (clone, dump, and mark) all volumes. Do all
     * volumes for a phase before starting the next phase to localize
     * disruption.  */
    /* Backup (clone) Phase */

    for (vol = Volumes; vol; vol = vol->next) {
	if (backup(vol) != 0) {
	    vol->flags |= BADNESS;
	    LogMsg(0, 0, stdout, "Backup of replicated volume %x failed!\n",vol->volId);
	}
    }

    /* Dump phase: dump all the volumes that were successfully backed up. */
    for (vol = Volumes; vol; vol = vol->next) {
	if (dumpVolume(vol, Partitions) < vol->nCloned) {
	    LogMsg(0, 0, stdout,"Dump of volume %x failed!\n",vol->volId);
	    vol->flags |= BADNESS;
	}
    }


    /* Tell servers to save pointers to successfully and fully dumped volumes. */
    for (vol = Volumes; vol; vol = vol->next) {
	/* Tell the server the backup was successful */
	if (MarkAsAncient(vol) < vol->nCloned) {
	    LogMsg(0, 0, stdout, "MarkAncient of %x failed!\n",vol->volId);
	    vol->flags |= BADNESS;
	}	
    }


    /* 2nd Pass. Backup again any volumes which failed the first time around. */
    LogMsg(0, 0, stdout, "");
    LogMsg(0, 0, stdout, "Attempting to retry any failed operations.");
    LogMsg(0, 0, stdout, "");

    /* force an attempt to connect to any down servers. If we paused
     * for a long period of time, servers may have gone down and back
     * up. We should get a chance to rebind.  */
    PollServers();

    for (vol = Volumes; vol; vol = vol->next) {
	/* If a volume failed and its server appears down, try it again. */
	repinfo_t *reps = vol->replicas;
	
	if ((vol->flags & BADNESS) || (vol->nCloned < vol->nReplicas)) {
	    unsigned char up = 0;

	    /* Count how many servers are up now. */
	    for (int i = 0; i < vol->nReplicas; i++) {
		if (Hosts[reps[i].serverNum].rpcid > 0)  /*  up. */
		    up++;
	    }

	    /* Only attempt it again if we think we can do better than we did. */
	    if ((vol->flags & BADNESS) || (up > vol->nCloned)) {

		/* Clear the appropriate flags. If not all replicas
		 * were included, try the whole thing again.  */
		vol->flags &= ~BADNESS;

		/* Only retry it if this stage failed last time. If it
		 * was never attempted, nCloned == 0.  */
		if (vol->nCloned < vol->nReplicas) {
		    if (backup(vol) != 0) {
			LogMsg(0, 0, stdout, "Backup of replicated volume %x failed again\n",vol->volId);
		    }
		}

		/* dumpVolume will only try those reps that haven't
		 * been dumped since the last clone.  */
		int ndumped;
		if ((ndumped = dumpVolume(vol, Partitions)) < vol->nCloned) {
		    vol->flags |= BADNESS;
		    LogMsg(0, 0, stdout,"Dump of volume %x failed again!\n",vol->volId);
		}

		/* MarkAsAncient will only try those reps that haven't
		 * been dumped since the last clone.  */
		if (MarkAsAncient(vol) < ndumped) {
		    vol->flags |= BADNESS;
		    LogMsg(0, 0, stdout, "MarkAsAncient of volume %x failed again!\n", vol->volId);
		}
	    }	
	}
    }

    /* While postprocessing the volumes, create a file called FULLDUMP
     * to notify the backup script that a full dump occured. The
     * script then can change it's behaviour in regards to the
     * treatment of the tape it will write.  */
    int fullDump = 0;
    
    
    /* Output the results. With each unsuccessfully backed up volume
     * report the last successsful stage of the replicas, since a
     * 3-way replicated volume on which 2 sites completely succeed is
     * still a failure.  */
    LogMsg(0, 0, stdout, "Successfully backed-up Volumes:");
    for (vol = Volumes; vol; vol = vol->next) {

	if ((vol->flags & BADNESS) || (vol->nCloned < vol->nReplicas))
	    continue;

	if (vol->flags & INCREMENTAL) 
	    LogMsg(0,0,stdout,"0x%8x (incremental)\t%s", vol->volId,vol->comment);
	else {
	    fullDump++;
	    LogMsg(0, 0, stdout, "0x%8x\t\t\t%s", vol->volId, vol->comment);
	}
    }

    LogMsg(0, 0, stdout, "");
    LogMsg(0, 0, stdout, "Only partially successfully backed-up Volumes:");
    for (vol = Volumes; vol; vol = vol->next) {
	if ((vol->flags & BADNESS) || (vol->nCloned < vol->nReplicas)) {
	    char buf[120], *ptr = buf;
	    repinfo_t *reps = vol->replicas;

	    int okay = 0;
	    for (int i = 0; i < vol->nReplicas; i++) 
		if (ISMARKED(reps[i].flags) ||	/* Successful backup. */
		    (ISDUMPED(reps[i].flags) && (vol->flags & INCREMENTAL))) {	
		    okay++;
		    break;	/* from inner for loop */
		}

	    if (okay) {

		if (vol->flags & INCREMENTAL)
		    sprintf(buf, "0x%8x (incremental)\t(", vol->volId);
		else  {
		    sprintf(buf, "0x%8x \t(", vol->volId);
		    fullDump++;
		}
		
		ptr += strlen(buf);
		
		/* Use the letter for the last stage each replica passed. */
		for (int i = 0; i < vol->nReplicas; i++) {
		    char c = ' ';
		    if (ISLOCKED(reps[i].flags)) c = 'L';
		    if (ISCLONED(reps[i].flags)) c = 'C';
		    if (ISDUMPED(reps[i].flags)) c = 'D';
		    if (ISMARKED(reps[i].flags)) c = 'M';
		    sprintf(ptr, "%c", c);
		    ptr++;
		}
		
		*ptr = 0;
		LogMsg(0, 0, stdout, "%s)\t%s", buf, vol->comment);
	    }
	}
    }

    LogMsg(0, 0, stdout, "");
    LogMsg(0, 0, stdout, "Volumes that FAILED backup:");
    for (vol = Volumes; vol; vol = vol->next) {
	char buf[120], *ptr = buf;
	if ((vol->flags & BADNESS) || (vol->nCloned < vol->nReplicas)) {
	    repinfo_t *reps = vol->replicas;

	    int okay = 0;
	    for (int i = 0; i < vol->nReplicas; i++) 
		if (ISMARKED(reps[i].flags)) {	/* Successful backup. */
		    okay++;
		    break;	/* from inner for loop */
		}

	    if (!okay) {   /* Only print out those that failed. */
	    
		sprintf(buf, "%#08x\t(", vol->volId);
		ptr += strlen(buf);
		
		/* Use the letter for the last stage each replica passed. */
		for (int i = 0; i < vol->nReplicas; i++) {
		    char c = ' ';
		    if (ISLOCKED(reps[i].flags)) c = 'L';
		    if (ISCLONED(reps[i].flags)) c = 'C';
		    if (ISDUMPED(reps[i].flags)) c = 'D';
		    if (ISMARKED(reps[i].flags)) c = 'M';
		    sprintf(ptr, "%c", c);
		    ptr++;
		}
		
		*ptr = 0;
		LogMsg(0, 0, stdout, "%s)\t%s", buf, vol->comment);
	    }
	}
    }

    LogMsg(0, 0, stdout, "");
    LogMsg(0, 0, stdout, "Volumes that were NOT backed-up:");
    ohashtab_iterator vnext(VRDB, (void *)-1); 
    vrent *vre;
    
    while (vre = (vrent *)vnext()) {
	for (vol = Volumes; vol && (vol->volId != vre->volnum); vol = vol->next) ;

	if (vol == 0) 
	    LogMsg(0, Debug, stdout, "%#08x\t\t\t%s", vre->volnum, vre->key);
    }

    if (fullDump) {
	FILE *tmp = fopen("FULLDUMP", "a");
	if (tmp) fclose(tmp);
	else perror("Opening FULLDUMP");
    }

    LogMsg(0, 0, stdout, "Histogram of sizes of dump files\n");
    PrintHisto(stdout, &DataTransferred);

    LogMsg(0, 0, stdout, "Histogram of DataRates for transfer of dump files\n");
    PrintHisto(stdout, &DataRate);
    LogMsg(0, 0, stdout, "\n");

    return 0;
}

/* copied from VInitServerList() in vol/volume.c and modified to suit
   my needs. */
static void VUInitServerList() {
    /* Find the server id */
    char hostname[100];
    char line[200];
    char *serverList = SERVERLISTPATH;
    FILE *file;

    LogMsg(9, Debug, stdout, "Entering VUInitServerList");
    bzero((char *)Hosts, sizeof(Hosts));

    file = fopen(serverList, "r");
    if (file == NULL) {
	LogMsg(0, Debug, stdout, 
	       "VUInitServerList: unable to read file %s; aborted", serverList);
	exit(1);
    }
    gethostname(hostname, sizeof(hostname)-1);
    ThisHost = (char *) malloc((int)strlen(hostname)+1);
    strcpy(ThisHost, hostname);
    while (fgets(line, sizeof(line), file) != NULL) {
        char sname[50];
	struct hostent *hostent;
	long sid;
	if (sscanf(line, "%s%d", sname, &sid) == 2) {
	    if (sid > N_SERVERIDS) {
		LogMsg(0, Debug, stdout, "Warning: host %s is assigned a bogus server number (%x) in %s; host ignored",
		  sname, sid, serverList);
		continue;
	    }
	    if (strcmp(hostname, sname) == 0)
		ThisServerId = (int)sid;
	    hostent = gethostbyname(sname);
	    if (hostent == NULL) {
		LogMsg(0, Debug, stdout, "Warning: host %s (listed in %s) is not in /etc/hosts", sname, serverList);
	    } else {
		long netaddress;
		CODA_ASSERT(hostent->h_length == 4);
		bcopy((char *)hostent->h_addr, (char *)&netaddress, 4);
		HostAddress[sid] = Hosts[sid].address = ntohl(netaddress);
		strncpy(Hosts[sid].name, hostent->h_name, 100);
	    }
	}
    }
    if (ThisServerId == -1) {
	LogMsg(0, Debug, stdout, "Warning: the hostname of this server (%s) is not listed in %s", ThisHost, serverList);
    }
    fclose(file);
}

/* The following three routines were copied directly from volclient.c */

static void V_InitRPC()
{
    PROCESS mylpid;
    FILE *tokfile;
    SFTP_Initializer sftpi;
    struct timeval tout;
    long rc;

    /* store authentication key */
    tokfile = fopen(TKFile, "r");
    if (!tokfile) {
	char estring[80];
	sprintf(estring, "Tokenfile %s", TKFile);
	perror(estring);
	exit(-1);
    }
    fscanf(tokfile, "%s", vkey);
    fclose(tokfile);

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
/*    sftpi.PacketSize = 1024;
    sftpi.WindowSize = 8;
    sftpi.SendAhead = 4;
    sftpi.AckPoint = 4; */
    SFTP_Activate(&sftpi);
    tout.tv_sec = Timeout;
    tout.tv_usec = 0;
    rc = RPC2_Init(RPC2_VERSION, 0, NULL, 3, &tout);
    if (rc != RPC2_SUCCESS) {
	LogMsg(0, 0, stdout, "RPC2_Init failed with %s\n",RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
}

static void V_BindToServer(char *fileserver, RPC2_Handle *RPCid)
{
 /* Binds to File Server on volume utility port on behalf of uName.
    Sets RPCid to the value of the connection id.    */

    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_BindParms bparms;
    RPC2_Handle rpcid;
    long     rc;

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, fileserver);
    pident.Tag = RPC2_PORTBYNAME;
    strcpy(pident.Value.Name, "codasrv");
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = UTIL_SUBSYSID;

    bzero((char *)&bparms, sizeof(bparms));
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;

    LogMsg(10, Debug, stdout, "V_BindToServer: binding to host %s\n", fileserver);
    rc = RPC2_NewBinding(&hident, &pident, &sident, &bparms, &rpcid);
    if (rc < 0 && rc > RPC2_ELIMIT)
	rc = 0;

    if (rc == 0) {
	LogMsg(5, Debug, stdout, "RPC2_Bind to server %s succeeded: rpcid %d\n",
	       fileserver, rpcid);
	*RPCid = rpcid;
    }

    /* Shouldn't ever get NOTAUTHENTICATED, but what if we do? -- DCS */
    if (rc != 0 && rc != RPC2_NOTAUTHENTICATED) {
	LogMsg(5, Debug, stdout, "RPC2_Bind to server %s failed with %s\n",
				fileserver, RPC2_ErrorMsg((int)rc));
	*RPCid = (RPC2_Handle)-1;
    }
}


/*
 * PollLWP will cycle: sleep for a time, then trying to rebind to any servers
 * which are down. Should it be forced to exit if the main program exits?
 */
static void PollLWP(int naptime)
{
    struct timeval time;

    time.tv_sec = naptime;
    time.tv_usec = 0;

    while (1) {
	if (IOMGR_Select(0, 0, 0, 0, &time) != 0) 
	    LogMsg(0, 0, stderr, "IOMGR_Select returns a failure!");

	PollServers();

	/* Recheck the disk usage for the partitions we are using. */
	for (struct DiskPartition *part = Partitions; part ; part = part->next) {
	    DP_SetUsage(part);
	}

	(void)IOMGR_Poll();
	LWP_DispatchProcess();			/* Yield everytime through loop */
	
    }
}

static void PollServers()
{
    for (int i = 0; i < N_SERVERIDS; i++) {
	if (Hosts[i].rpcid == BADCONNECTION) {
	    LogMsg(3, Debug, stdout, "Polling to host %s %x.", 
			   Hosts[i].name, Hosts[i].address);
	    
	    CODA_ASSERT(Hosts[i].address != 0);  /* Shouldn't find new servers! */
	    
	    /* Will set rpcid to -1 if it fails. */
	    V_BindToServer(Hosts[i].name, &Hosts[i].rpcid);
	    
	    if (Hosts[i].rpcid == BADCONNECTION)
		LogMsg(3, Debug, stdout, "Poll to host %s %x failed.",
		       Hosts[i].name, Hosts[i].address);
	    else {
		LogMsg(0, 0, stdout, "Restablished contact with %s.",
		       Hosts[i].name);
		if (mkdir(Hosts[i].name, 0755) != 0) {
		    if (errno != EEXIST) {
			LogMsg(0, 0, stderr, "Couldn't make a directory for %s, exiting.\n", Hosts[i].name);
			exit(-1);
		    }
		}
	    }
	}
	if (Hosts[i].rpcid > 0)
	    LogMsg(3, Debug, stdout, "Poll: rpcid == %x\n",
		   Hosts[i].rpcid);
    }
}



static void VolDumpLWP(struct rockInfo *rock)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *myrequest;
    RPC2_Handle	mycid;
    long rc;
    
    RPC2_SubsysIdent subsysid;
    FILE *tokfile;

    /* Hide the dumpfile name under a rock for later retrieval. */
    CODA_ASSERT(LWP_NewRock(ROCKTAG, (char *)rock) == LWP_SUCCESS);
    
    /* get encryption key for authentication */
    tokfile = fopen(TKFile, "r");
    fscanf(tokfile, "%s", vkey);
    fclose(tokfile);

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    
    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = VOLDUMP_SUBSYSTEMID;

    while (1) {
	rc=RPC2_GetRequest(&myfilter, &mycid, &myrequest, NULL, NULL, 0, NULL);
	if (rc == RPC2_SUCCESS) {
	    rc = volDump_ExecuteRequest(mycid, myrequest, NULL);
	    if (rc) {
		LogMsg(0, 0, stdout, "VolDumpLWP: request %d failed with %s\n",
			myrequest->Header.Opcode, RPC2_ErrorMsg((int)rc));
	    }
	}
	else LogMsg(0, 0, stdout, "VolDumpLWP: Get_Request failed with %s\n",RPC2_ErrorMsg((int)rc));
    }

}

long WriteDump(RPC2_Handle rpcid, unsigned long offset, unsigned long *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    int status = 0;
    long rc = 0;
    struct rockInfo *rock;
    SE_Descriptor sed;
    
    CODA_ASSERT(LWP_GetRock(ROCKTAG, (char **)&rock) == LWP_SUCCESS);

    if (volid != rock->volid) {
	LogMsg(0, 0, stdout, "Got a WriteDump for %x, I'm dumping %x!\n", volid, rock->volid);
	return -1;
    }

    if (rock->numbytes != offset) {
	LogMsg(0, 0, stdout, "Offest %d != rock->numbytes %d\n", offset, rock->numbytes);
    }
    
    /* fetch the file with volume data */
    bzero((char *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sed.Value.SmartFTPD.ByteQuota = -1;
    sed.Value.SmartFTPD.SeekOffset = offset;
    sed.Value.SmartFTPD.hashmark = 0;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;
    CODA_ASSERT(strlen(rock->dumpfile) <
	   sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, rock->dumpfile);

    struct timeval before, after;
    gettimeofday(&before, 0);
    
    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT){
	LogMsg(0, 0, stdout, "WriteDump: Error %s in InitSideEffect\n", RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) 
	       <= RPC2_ELIMIT) {
	LogMsg(0, 0, stdout, "WriteDump: Error %s in CheckSideEffect\n", RPC2_ErrorMsg((int)rc));
    }

    gettimeofday(&after, 0);
    
    if (sed.Value.SmartFTPD.BytesTransferred != *nbytes) {
	LogMsg(0, 0, stdout, "Transmitted bytes %d != requested bytes %d!\n",
	    sed.Value.SmartFTPD.BytesTransferred, *nbytes);
	*nbytes = sed.Value.SmartFTPD.BytesTransferred;
    }
    rock->numbytes += sed.Value.SmartFTPD.BytesTransferred;

    /* Update the Histogram */
    double tmp;
    tmp = (after.tv_sec-before.tv_sec) + (after.tv_usec-before.tv_usec) * .000001;
    tmp = sed.Value.SmartFTPD.BytesTransferred / tmp;
    UpdateHisto(&DataRate, tmp);
    return rc;
}

long ReadDump(RPC2_Handle rpcid, RPC2_Unsigned offset, RPC2_Integer *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    LogMsg(0, 0, stdout, "GOT A READDUMP CALL!!!!\n");
    CODA_ASSERT(0);
}

