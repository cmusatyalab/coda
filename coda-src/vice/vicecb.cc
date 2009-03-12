/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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


/************************************************************************/
/*									*/
/*  vicecb.c	- Call back routines					*/
/*									*/
/*  Function  	- This routine contains the implementation of callback  */
/*		  structure						*/
/*  Warning - makes use of fact that AddCallBack and BreakCallBack are  */
/*		called with exclusive locks on the file, and thus can't */
/*		interfere with each other                               */
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"

#include <rpc2/rpc2.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <histo.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <callback.h>
#include <vrdb.h>
#include <srv.h>
#include <vice.private.h>

extern const int MaxVols;

/* *****  Exported variables  ***** */

int CBEs = 0;		/* active call back entries */
int CBEBlocks =	0;	/* number of blocks of CBEs */
int FEs	= 0;		/* active file entries */
int FEBlocks = 0;	/* number of blocks of FEs */
int VEs = 0;            /* number of FEs that are for volumes */
int VCBEs = 0;          /* number of CBEs that are for volumes */

/* *****  Private constants  ***** */

const int VHASH = 256;

/* *****  Private types  ***** */

/* One per file. */
struct FileEntry {
	struct FileEntry *next;
	struct Lock cblock;
	ViceFid theFid;
	int users;
	struct CallBackEntry *callBacks;
};

/* Block of file entries. */
#define FESPERBLOCK 219
struct FEBlock {
	struct FileEntry entry[FESPERBLOCK];
};

/* A callback entry represents one fid being referenced by one venus. */
struct CallBackEntry {
	struct CallBackEntry *next;	    /* The next dude same file id. */
	HostTable *conn;		    /* The bogon to notify. */
};

/* Block of callback entries. */
#define CBESPERBLOCK 1024
struct CBEBlock {
	struct CallBackEntry entry[CBESPERBLOCK];
};

/* Callback statistics, per volume */
struct CBStat {
	VolumeId volid;
	unsigned FEs;
	unsigned VEs;
	unsigned CBEs;
	unsigned VCBEs;
};

/* *****  Private variables  ***** */

static	struct FileEntry *hashTable[VHASH]; /* File entry hash table */
static struct CallBackEntry *CBEFree =	0;  /* first free CBE */
static struct FileEntry *FEFree = 0;	    /* first free file entry */

/* *****  Private routines  ***** */

static long VHash(ViceFid *);
static void GetFEBlock();
static struct FileEntry *GetFE();
static void FreeFE(struct FileEntry *);
static struct FileEntry *FindEntry(ViceFid *);
static void DeleteFileStruct(struct FileEntry *);

static void GetCBEBlock();
static struct CallBackEntry *GetCBE();
static void FreeCBE(struct CallBackEntry *);
static void SDeleteCallBack(HostTable *, struct FileEntry *);


/* *****  File Entries  ***** */

static long VHash(ViceFid *afid) {
    long i = ((afid->Volume + afid->Vnode) % VHASH);
    return((i < 0) ? i + VHASH : i);
}


/* Get a new block of FEs and chain it on FEFree. */
static void GetFEBlock() 
{
	struct FEBlock *block = 
		(struct FEBlock *)malloc(sizeof(struct FEBlock));
	CODA_ASSERT(block);

	for(int i = 0; i < (FESPERBLOCK - 1); i++) {
		Lock_Init(&block->entry[i].cblock);
		block->entry[i].next = &(block->entry[i + 1]);
	}
	block->entry[FESPERBLOCK - 1].next = 0;
	Lock_Init(&block->entry[FESPERBLOCK - 1].cblock);
	
	FEFree = (struct FileEntry *)block;
	FEBlocks++;
}


/* Get the next available FE. */
static struct FileEntry *GetFE() 
{
	if (FEFree == 0) 
		GetFEBlock();

	struct FileEntry *entry = FEFree;
	FEFree = entry->next;
	FEs++;

	return(entry);
}


/* Return an entry to the free list. */
static void FreeFE(struct FileEntry *entry) 
{
	if (entry->theFid.Vnode == 0 && entry->theFid.Unique == 0) 
		VEs--;
	entry->theFid.Volume = 0;
	entry->theFid.Vnode = 0;
	entry->theFid.Unique = 0;
	entry->next = FEFree;
	FEFree = entry;
	FEs--;
}


static struct FileEntry *FindEntry(ViceFid *afid) 
{
    for (struct FileEntry *tf = hashTable[VHash(afid)]; tf; tf = tf->next)
	    if (FID_EQ(&tf->theFid, afid)) return(tf);

    return(0);
}


static void DeleteFileStruct(struct FileEntry *af) 
{
	long bucket = VHash(&af->theFid);
	struct FileEntry **lf = &hashTable[bucket];
	for (struct FileEntry *tf = hashTable[bucket]; tf; tf = tf->next) {
		if (tf == af) {
			*lf = af->next;
			FreeFE(af);
			return;
		}
		lf = &tf->next;
	}
}


/* ***** Callback Entries ***** */

/* Get a new block of CBEs and chain it on CBEFree. */
static void GetCBEBlock() {
    struct CBEBlock *block = (struct CBEBlock *)malloc(sizeof(struct CBEBlock));
    CODA_ASSERT(block);

    for(int i = 0; i < (CBESPERBLOCK - 1); i++) 
	block->entry[i].next = &(block->entry[i+1]);
    block->entry[CBESPERBLOCK - 1].next = 0;

    CBEFree = (struct CallBackEntry *)block;
    CBEBlocks++;
}


/* Get the next available CBE. */
static struct CallBackEntry *GetCBE() {
    if (CBEFree == 0) GetCBEBlock();

    struct CallBackEntry *entry = CBEFree;
    CBEFree = entry->next;
    CBEs++;

    return(entry);
}


/* Return an entry to the free list. */
static void FreeCBE(struct CallBackEntry *entry) {
    entry->next = CBEFree;
    CBEFree = entry;
    CBEs--;
}
    

int InitCallBack() {
    for (int i = 0; i < VHASH; i++)
	hashTable[i] = 0;

    return(0);
}

/*
AddCallBack: Establish a callback for afid with
  client connected via */

CallBackStatus AddCallBack(HostTable *client, ViceFid *afid) 
{
    SLog(3, "AddCallBack for Fid %s, Venus %s.%d",
	 FID_(afid), inet_ntoa(client->host), ntohs(client->port));

    char aVCB = (afid->Vnode == 0 && afid->Unique == 0);

    struct FileEntry *tf = FindEntry(afid);
    if (!tf) {
	/* Create a new file entry. */
	tf = GetFE();
	tf->theFid = *afid;
	tf->users = 0;
	tf->callBacks = 0;

	/* Insert it into the hash table. */
	long bucket = VHash(afid);
	tf->next = hashTable[bucket];
	hashTable[bucket] = tf;

	if (aVCB) VEs++;   /* this is actually a volume entry */
    }

    /* Shouldn't ever call AddCallBack() while doing a BreakCallBack()! */
    if (CheckLock(&tf->cblock)) {
	LogMsg(0, SrvDebugLevel, stdout, "AddCallBack: breaking callback, %s",	
	       FID_(afid));
	return(NoCallBack);
    }

    /* Don't add it if it is already in the list. */
    struct CallBackEntry *tc;
    for (tc = tf->callBacks; tc; tc = tc->next)
	if (tc->conn == client) return(CallBackSet);

    /* Otherwise, set it up and add it to the head of the linked list */
    tf->users++;
    if (aVCB) VCBEs++;   /* this is a volume callback */
    tc = GetCBE();
    tc->next = tf->callBacks;
    tf->callBacks = tc;
    tc->conn = client;

    return(CallBackSet);
}

/* this function is used when sorting HostTable entries to obtain correct
 * lock ordering */
static int order_helist(const void *x, const void *y)
{
    HostTable *he1 = (HostTable *)x;
    HostTable *he2 = (HostTable *)y;
    
    if (he1->id < he2->id) return -1;
    if (he1->id > he2->id) return 1;
    return 0;
}

/*
  BreakCallBack: Break a callback for afid at all clients except those
  connected via the "client" parameter
*/
void BreakCallBack(HostTable *client, ViceFid *afid) {
    struct CallBackEntry *tc;

    LogMsg(3, SrvDebugLevel, stdout, "BreakCallBack for Fid %s", FID_(afid));
    if (client) LogMsg(3, SrvDebugLevel, stdout, "Venus %s.%d",
		       inet_ntoa(client->host), ntohs(client->port));
    else LogMsg(3, SrvDebugLevel, stdout, "No connection");

    struct FileEntry *tf = FindEntry(afid),
		     *nf;
    if (!tf) return;	/* No callbacks on this file */
    
    char aVCB = (afid->Vnode == 0 && afid->Unique == 0);
    ViceFid vFid = NullFid; vFid.Volume = afid->Volume;

    /* 
     * Break all callbacks for this file.  Clean up failed veni. 
     * If the object is a file, remove any volume callbacks that
     * that host may have.
     */
    ObtainWriteLock(&tf->cblock);

    /* If two threads ar trying to break the same callback on the same
     * volume (or file) the first thread breaks all the callbacks, it
     * then deletes the file struct.  The second thread already has a
     * pointer to the file struct, which is now on the free list and
     * is hosed.  So it tries to break callbacks on the entire free
     * list!
     *
     * The fix is to re-obtain the file entry just to make sure its
     * still around.  If not, we release the write lock (because other
     * threads may also be waiting for this entry) and return.  If its
     * a different entry, then someone broke all the callbacks and
     * more were added while we were away.
     * NOTE:  There is a slim chance that this will unnecessarily
     *        break callbacks.  If tf got released by another thread,
     *	      then got re-created by an add-callbacks before we get
     *	      our lock, chances are the same entry will be taken off
     *	      the free list and we will break the new callbacks.
     */
    if (!(nf = FindEntry(afid)) || (nf != tf)) {
	ReleaseWriteLock(&tf->cblock);
	return;
    }
    
    /* allocate space for all hostentries we will lock */
    HostTable **helist = (HostTable **) malloc(sizeof(HostTable *) * tf->users);

    /* allocate space for multirpc lists.  tf->users is an upper bound. */
    RPC2_Handle *cidlist = (RPC2_Handle *) malloc(sizeof(RPC2_Handle) * tf->users);
    RPC2_Integer *rclist = (RPC2_Integer *) malloc(sizeof(RPC2_Integer) * tf->users);
    memset((char *) rclist, 0, (int) sizeof(RPC2_Integer) * tf->users);
    
    /* how many client entries, other than us?  fill conn id list, and obtain
     * locks on the hostentry structures */
    int nhosts = 0, nhents = 0;
    /* get a list of all hosts we need to break callbacks with */
    for (tc = tf->callBacks; tc; tc = tc->next)
	if (tc->conn && tc->conn != client)
	    helist[nhents++] = tc->conn;

    /* Sort the list of hosts, to get correct lock ordering */
    qsort(helist, nhents, sizeof(HostTable *), order_helist);

    for (int i = 0; i < nhents; i++) {
	ObtainWriteLock(&helist[i]->lock);

	/* Recheck! The callback connection might have been destroyed */
	if (!helist[i]->id) {
	    ReleaseWriteLock(&helist[i]->lock);
	    helist[i] = NULL;
	}
	else
	    cidlist[nhosts++] = helist[i]->id;
    }

    LogMsg(3, SrvDebugLevel, stdout, "BreakCallBack: %d conns, %d users", 
	   nhosts, tf->users); 

    /* assuming no multicast in call below (MCast parameter NULL) */
    if (nhosts > 0) {
	/* make sure we abort long running callback break attempts */
	struct timeval tout = { 60, 0 };
	MRPC_MakeMulti(CallBack_OP, CallBack_PTR, nhosts, cidlist, rclist,
		       NULL, NULL, &tout, afid);

	for (int i = 0, nhosts = 0; i < nhents; i++) {
	    if (!helist[i]) continue;

	    /* we cannot have lost any of the hostentries, because they
	     * were locked */

	    /* if a file callback, delete any volume callbacks */
	    if (!aVCB)  DeleteCallBack(helist[i], &vFid);

	    /* recursively calls DeleteVenus */
	    if (rclist[nhosts] < RPC2_ELIMIT)  
		CLIENT_CleanUpHost(helist[i]);

	    /* let go of the lock */
	    ReleaseWriteLock(&helist[i]->lock);

	    nhosts++;
	}
    }

    free(helist);
    free(cidlist);
    free(rclist);

    ReleaseWriteLock(&tf->cblock);

    /* Nuke all bad callback entries. */
    struct CallBackEntry *goodc = 0;
    struct CallBackEntry *nextc = 0;
    for (tc = tf->callBacks; tc; tc = nextc) {
	nextc = tc->next;
	if (tc->conn == 0 || tc->conn != client) {
	    FreeCBE(tc);
	    if (aVCB) VCBEs--;
        }
	else goodc = tc;	/* Must be client */
    }

    /* Now see if we have any good callbacks left on this file.  If
       not, nuke it */
    if (goodc) {
	/* We have one callback entry */
	goodc->next = 0;
	tf->callBacks = goodc;
	tf->users = 1;
    }
    else {
	/* We have no callback entries left */
	DeleteFileStruct(tf);
    }
}


/*
  
  DeleteCallBack : Delete a callback for afid with client connected
  via client
 
*/
void DeleteCallBack(HostTable *client, ViceFid *afid) 
{
    SDeleteCallBack(client, FindEntry(afid));
}

static void SDeleteCallBack(HostTable *client, struct FileEntry *af) 
{
    if (!af) return;

    struct FileEntry *tf = af;
    int	busy = CheckLock(&tf->cblock);	    /* Is it already busy? */
    char aVCB = (af->theFid.Vnode == 0 && af->theFid.Unique == 0);

    struct CallBackEntry **lc = &tf->callBacks;
    struct CallBackEntry *nc = 0;
    for (struct CallBackEntry *tc = tf->callBacks; tc; tc = nc) {
	nc = tc->next;
	if (tc->conn == client) {
	    if (busy) {
		tc->conn = 0;
	    }
	    else {
		FreeCBE(tc);
		if (aVCB) VCBEs--;
		(*lc) = nc;
		tf->users--;
	    }
	    SLog(3, "SDeleteCallBack for Fid %s, Venus %s.%d",
		 FID_(&af->theFid),
		 inet_ntoa(client->host), ntohs(client->port));
	    break;
	}
	lc = &tc->next;
    }

    /* Now see if there are any left. */
    if (!busy && tf->users <= 0)
	DeleteFileStruct(tf);
}


/*
  DeleteVenus: Delete all callbacks for a client 
*/
void DeleteVenus(HostTable *client) 
{
    SLog(1, "DeleteVenus for venus %s.%d",
	 inet_ntoa(client->host), ntohs(client->port));

    for (int i = 0; i < VHASH; i++) {
	    struct FileEntry *nf = 0;
	    for (struct FileEntry *tf = hashTable[i]; tf; tf = nf) {
		    /* Pull this out before it gets zapped */
		    nf = tf->next;
		    /* May or may not delete the block. */
		    SDeleteCallBack(client, tf);
	    }
    }
}


/* Delete all the status on a file--used when a file is removed. */
void DeleteFile(ViceFid *afid) 
{
    LogMsg(3, SrvDebugLevel, stdout, "DeleteFile for Fid %s", FID_(afid));

    char aVCB = (afid->Vnode == 0 && afid->Unique == 0);
    struct FileEntry *tf = FindEntry(afid);
    if(tf) {
	if (CheckLock(&tf->cblock)) {
	    /* Mark them all as dead if a delete callback is occurring. */
	    for(struct CallBackEntry *tc = tf->callBacks; tc; tc = tc->next)
		tc->conn = 0;
	}
	else {
	    /* Do it all ourselves. */
	    struct CallBackEntry *nc = 0;
	    for(struct CallBackEntry *tc = tf->callBacks; tc; tc = nc) {
		nc = tc->next;
		FreeCBE(tc);
		if (aVCB) VCBEs--;
	    }
	    DeleteFileStruct(tf);
	}
    }
}

/* ***** Coda Callbacks ***** */
/*
  CodaAddCallBack: Establish a callback with a client
  for the non-replicated and replicated fid of an object
*/
CallBackStatus CodaAddCallBack(HostTable *VenusId, ViceFid *Fid, 
			       VolumeId VSGVolnum) 
{
	if (Fid->Volume == VSGVolnum)
		return(AddCallBack(VenusId, Fid));

	ViceFid VSGFid;
	VSGFid.Volume = VSGVolnum;
	VSGFid.Vnode = (Fid)->Vnode;
	VSGFid.Unique = (Fid)->Unique;
	return(AddCallBack(VenusId, &VSGFid));
}

/*
  
  CodaBreakCallBack: Break the callback with a client, for the
  non-replicated and replicated fid of an object.

  */
void CodaBreakCallBack(HostTable *VenusId, ViceFid *Fid, VolumeId VSGVolnum) 
{
	ViceFid VolFid;

	VolFid.Volume = Fid->Volume;
	VolFid.Vnode = VolFid.Unique = 0;
	BreakCallBack(VenusId, Fid);
	BreakCallBack(VenusId, &VolFid);

	if (Fid->Volume != VSGVolnum) {
		ViceFid VSGFid;
		VSGFid.Volume = VSGVolnum;
		VSGFid.Vnode = Fid->Vnode;
		VSGFid.Unique = Fid->Unique;
		VolFid.Volume = VSGVolnum;
		BreakCallBack(VenusId, &VSGFid);
		BreakCallBack(VenusId, &VolFid);
	}
	return;
}

void CodaDeleteCallBack(HostTable *VenusId, ViceFid *Fid, VolumeId VSGVolnum) 
{
	if (Fid->Volume == VSGVolnum)
		DeleteCallBack(VenusId, Fid);
	else {
		ViceFid VSGFid;
		VSGFid.Volume = VSGVolnum;
		VSGFid.Vnode = Fid->Vnode;
		VSGFid.Unique = Fid->Unique;
		DeleteCallBack(VenusId, &VSGFid);
	}
	return;
}



/* ***** Debugging ***** */

static int CompareCBSEnts(struct CBStat *a, struct CBStat *b) 
{
	if (a->volid < b->volid) 
		return -1;
	if (a->volid > b->volid) 
		return 1;
	return(0);
}

void PrintCallBackState(FILE *fp) 
{
    fprintf(fp, "Callback entries:\n");
    fprintf(fp, "\tCBEs allocated %d (%d blocks) \n",
	    CBEBlocks * CBESPERBLOCK, CBEBlocks);
    fprintf(fp, "\tActive CBEntries %d (%d volume CBEs)\n", CBEs, VCBEs);
    // count number of free CBE
    {
	struct CallBackEntry *nextcb;
	int count;
	for (nextcb = CBEFree,count = 0; nextcb != 0; nextcb = nextcb->next) 
	    count++;
	fprintf(fp, "\tFree CBEntries found from free list = %d\n", count);
    }
    
    fprintf(fp, "File entries:\n");
    fprintf(fp, "\tFEs allocated %d (%d blocks)\n",
	    FEBlocks * FESPERBLOCK, FEBlocks);
    fprintf(fp, "\tActive FEntries %d (%d volume entries)\n", FEs, VEs);
    // count number of free FEs
    {
	struct FileEntry  *nextfe;
	int count;
	for (nextfe = FEFree,count = 0; nextfe != 0; nextfe = nextfe->next) 
	    count++;
	fprintf(fp, "\tFree FEntries found from free list = %d\n", count);
    }

    // count number of allocated FEs and CBEs, break down by volume
    {
	int allocatedFEs = 0;
	int allocatedCBEs = 0;
	int numVolumes = 0;
	struct CBStat *CBStats, *CBSEnt;   // will sort at end 
	struct hgram CBGram, VCBGram;
	
	// shouldn't need to do this -- maxvols is really a constant. 
	CBStats = (struct CBStat *) malloc(MaxVols * sizeof(struct CBStat));
	memset((char *)CBStats, 0, (int)sizeof(struct CBStat) * MaxVols);

	for (int i = 0; i < VHASH; i++) {
	    struct FileEntry *tfe = hashTable[i];
	    while (tfe) {
		char aVE = (tfe->theFid.Vnode == 0 && tfe->theFid.Unique == 0);
		allocatedFEs++;

		// find the volume statistics entry - not very fast, but simple
		int j;   // can't declare j below because of other initialization
		for (j = 0, CBSEnt = CBStats; j < numVolumes; j++, CBSEnt++)
		   if (CBSEnt->volid == tfe->theFid.Volume) break;

		if (CBSEnt->volid == 0) { // new entry
		   CBSEnt->volid = tfe->theFid.Volume;
		   numVolumes++;
		}		     
		CBSEnt->FEs++;
		if (aVE) CBSEnt->VEs++;
			
		// count allocated cbes 
		{
		    int countcbe = 0;
		    struct CallBackEntry *tcbe = tfe->callBacks;
		    while (tcbe) {
			countcbe++;
			tcbe = tcbe->next;
		    }
		    if (countcbe != tfe->users) 
			fprintf(fp, "For Fid %s users = %d when counted = %d\n",
				FID_(&tfe->theFid), tfe->users, countcbe);
		    
		    allocatedCBEs += countcbe;
		    CBSEnt->CBEs += countcbe;
		    if (aVE) CBSEnt->VCBEs += countcbe;
		}
		tfe = tfe->next;
	    }
	}
	fprintf(fp, "\tFrom lists: %d CBEs allocated and %d FEs allocated\n",
		allocatedCBEs, allocatedFEs);

	// summary statstics. number of callbacks per volume depends on 
	// number of clients and number of objects in the volume. number
	// of volume callbacks is only 100 because of the small number of
	// clients currently using the system.

	InitHisto(&CBGram, 1, 100000, 20, LOG10);   // upper limit is a guess
	InitHisto(&VCBGram, 0, 100, 20, LINEAR);    // can have vols with no VCBs

	fprintf(fp, "Callback statistics by volume: %d volumes\n", numVolumes);
	qsort((void *)CBStats, numVolumes, sizeof(struct CBStat), 
	      (int (*)(const void *, const void *))CompareCBSEnts);
	{
	int i;
            for (i = 0;  i < numVolumes; i++) {
	        fprintf(fp, "\t%08x  %d FEs (%d VEs), %d CBEs (%d VCBEs)\n",
		        CBStats[i].volid, CBStats[i].FEs, CBStats[i].VEs,
		        CBStats[i].CBEs, CBStats[i].VCBEs);
	    
	        UpdateHisto(&CBGram, (double) CBStats[i].CBEs);
	        UpdateHisto(&VCBGram, (double) CBStats[i].VCBEs);
            }
        }
	fprintf(fp, "\nHistogram of number of callbacks per volume\n");
	PrintHisto(fp, &CBGram);
	fprintf(fp, "\nHistogram of number of volume callbacks per volume\n");
	PrintHisto(fp, &VCBGram);
	free(CBStats);
    }
}


static void PrintCBE(struct CallBackEntry *tcbe, FILE *fp) 
{
    CODA_ASSERT (tcbe);
    if (tcbe->conn) {
	fprintf(fp, "\tHost %s portal 0x%x\n", 
		inet_ntoa(tcbe->conn->host), ntohs(tcbe->conn->port));
    }  
}


static void GetCallBacks(ViceFid *fid, FILE *fp) 
{
    fprintf(fp, "Printing callbacks for %s\n", FID_(fid));
    struct FileEntry *tfe = FindEntry(fid);
    if (tfe) 
	for (struct CallBackEntry *tcbe = tfe->callBacks; tcbe; tcbe = tcbe->next) 
	    PrintCBE(tcbe, fp);

    fprintf(fp, "End of callbacks for %s\n", FID_(fid));
}

static void GetCallBacks(VolumeId vid, FILE *fp) 
{
    fprintf(fp, "Printing callbacks for %08x\n", vid);

    /* print volume callbacks first */
    ViceFid fid = NullFid; fid.Volume = vid;
    struct FileEntry *tfe = FindEntry(&fid);
    if (tfe) {
	fprintf(fp, "VID %08x  Volume Callback\n", vid);
	for (struct CallBackEntry *tcbe = tfe->callBacks; tcbe; tcbe = tcbe->next) 
	    PrintCBE(tcbe, fp);
    }

    /* print file callbacks for all fids in the volume */
    for (int j = 0; j < VHASH; j++) {
	struct FileEntry *nf = 0;
	for (struct FileEntry *tf = hashTable[j]; tf; tf = nf) {
	    nf = tf->next;
	    if (tf->theFid.Volume == vid && tf->theFid.Vnode && tf->theFid.Unique) {
		fprintf(fp, "FID %s\n", FID_(&tf->theFid));
		for (struct CallBackEntry *tcbe = tf->callBacks; tcbe; tcbe = tcbe->next)
		    PrintCBE(tcbe, fp);
	    }
        }
    }
    fprintf(fp, "End of callbacks for %08x\n", vid);
}

// print all the callbacks for given fid.
// if the fid is volid.0.0, print callback status
// for the entire volume.
void PrintCallBacks(ViceFid *fid, FILE *fp) 
{
    // check for the replicated id also 

    ViceFid ofid = *fid;
    int useofid = 0;
    if (!XlateVid(&ofid.Volume)) {
	ofid.Volume = fid->Volume;
	if (ReverseXlateVid(&ofid.Volume)) 
	    useofid = 1;
    }
    else 
	useofid = 1;

    if (fid->Vnode && fid->Unique) {
	GetCallBacks(fid, fp);
	if (useofid) GetCallBacks(&ofid, fp);
    } else {
	GetCallBacks(fid->Volume, fp);
	if (useofid) GetCallBacks(ofid.Volume, fp);
    }
}
