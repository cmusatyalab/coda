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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/vicecb.cc,v 4.2 1997/01/28 11:54:45 satya Exp $";
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
#endif __cplusplus

#include <stdio.h>

#if !defined(__GLIBC__)
#include <libc.h>
#include <sysent.h>
#endif

#include <rpc2.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <histo.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <callback.h>
#include <vrdb.h>
#include <srv.h>

extern const MaxVols;

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

PRIVATE	struct FileEntry *hashTable[VHASH]; /* File entry hash table */
PRIVATE struct CallBackEntry *CBEFree =	0;  /* first free CBE */
PRIVATE struct FileEntry *FEFree = 0;	    /* first free file entry */

/* *****  Private routines  ***** */

PRIVATE long VHash(ViceFid *);
PRIVATE void GetFEBlock();
PRIVATE struct FileEntry *GetFE();
PRIVATE void FreeFE(struct FileEntry *);
PRIVATE struct FileEntry *FindEntry(ViceFid *);
PRIVATE void DeleteFileStruct(struct FileEntry *);

PRIVATE void GetCBEBlock();
PRIVATE struct CallBackEntry *GetCBE();
PRIVATE void FreeCBE(struct CallBackEntry *);
PRIVATE void SDeleteCallBack(HostTable *, struct FileEntry *);


/* *****  File Entries  ***** */

PRIVATE long VHash(ViceFid *afid) {
    long i = ((afid->Volume + afid->Vnode) % VHASH);
    return((i < 0) ? i + VHASH : i);
}


/* Get a new block of FEs and chain it on FEFree. */
PRIVATE void GetFEBlock() {
    struct FEBlock *block = (struct FEBlock *)malloc(sizeof(struct FEBlock));
    assert(block);

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
PRIVATE struct FileEntry *GetFE() {
    if (FEFree == 0) GetFEBlock();

    struct FileEntry *entry = FEFree;
    FEFree = entry->next;
    FEs++;

    return(entry);
}


/* Return an entry to the free list. */
PRIVATE void FreeFE(struct FileEntry *entry) {
    if (entry->theFid.Vnode == 0 && entry->theFid.Unique == 0) VEs--;
    entry->theFid.Volume = 0;
    entry->theFid.Vnode = 0;
    entry->theFid.Unique = 0;
    entry->next = FEFree;
    FEFree = entry;
    FEs--;
}


PRIVATE struct FileEntry *FindEntry(ViceFid *afid) {
    for (struct FileEntry *tf = hashTable[VHash(afid)]; tf; tf = tf->next)
	if (FID_EQ(tf->theFid, *afid)) return(tf);

    return(0);
}


PRIVATE void DeleteFileStruct(struct FileEntry *af) {
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
PRIVATE void GetCBEBlock() {
    struct CBEBlock *block = (struct CBEBlock *)malloc(sizeof(struct CBEBlock));
    assert(block);

    for(int i = 0; i < (CBESPERBLOCK - 1); i++) 
	block->entry[i].next = &(block->entry[i+1]);
    block->entry[CBESPERBLOCK - 1].next = 0;

    CBEFree = (struct CallBackEntry *)block;
    CBEBlocks++;
}


/* Get the next available CBE. */
PRIVATE struct CallBackEntry *GetCBE() {
    if (CBEFree == 0) GetCBEBlock();

    struct CallBackEntry *entry = CBEFree;
    CBEFree = entry->next;
    CBEs++;

    return(entry);
}


/* Return an entry to the free list. */
PRIVATE void FreeCBE(struct CallBackEntry *entry) {
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
  BEGIN_HTML
  <a name="AddCallBack"><strong>Establish a callback for <tt>afid</tt> with
  client connected via <tt>aconnid</tt> </strong> </a>
  END_HTML 
*/
CallBackStatus AddCallBack(HostTable *aconnid, ViceFid *afid) {
    LogMsg(3, SrvDebugLevel, stdout, "AddCallBack for Fid 0x%x.%x.%x, Venus %s.%d", afid->Volume,
	     afid->Vnode, afid->Unique, aconnid->HostName, aconnid->port);

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
	LogMsg(0, SrvDebugLevel, stdout, "AddCallBack: breaking callback, 0x%x.%x.%x",	
	       afid->Volume, afid->Vnode, afid->Unique);
	return(NoCallBack);
    }

    /* Don't add it if it is already in the list. */
    struct CallBackEntry *tc;
    for (tc = tf->callBacks; tc; tc = tc->next)
	if (tc->conn == aconnid) return(CallBackSet);

    /* Otherwise, set it up and add it to the head of the linked list */
    tf->users++;
    if (aVCB) VCBEs++;   /* this is a volume callback */
    tc = GetCBE();
    tc->next = tf->callBacks;
    tf->callBacks = tc;
    tc->conn = aconnid;

    return(CallBackSet);
}


/*
  BEGIN_HTML
  <a name="BreakCallBack"><strong>Break a callback for <tt>afid</tt> with
  client connected via <tt>aconnid</tt> </strong> </a>
  END_HTML 
*/
void BreakCallBack(HostTable *aconnid, ViceFid *afid) {
    struct CallBackEntry *tc;

    LogMsg(3, SrvDebugLevel, stdout, "BreakCallBack for Fid 0x%x.%x.%x",
	     afid->Volume, afid->Vnode, afid->Unique);
    if (aconnid) LogMsg(3, SrvDebugLevel, stdout, "Venus %s.%d",
			aconnid->HostName, aconnid->port);
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
    
    /* allocate space for multirpc lists.  tf->users is an upper bound. */
    RPC2_Handle *cidlist = (RPC2_Handle *) malloc(sizeof(RPC2_Handle) * tf->users);
    RPC2_Integer *rclist = (RPC2_Integer *) malloc(sizeof(RPC2_Integer) * tf->users);
    bzero((char *) rclist, (int) sizeof(RPC2_Integer) * tf->users);
    
    /* how many client entries, other than us?  fill conn id list */
    int nhosts = 0;
    for (tc = tf->callBacks; tc; tc = tc->next) 
	if (tc->conn && tc->conn != aconnid && tc->conn->id)
	    cidlist[nhosts++] = tc->conn->id;

    LogMsg(3, SrvDebugLevel, stdout, "BreakCallBack: %d conns, %d users", 
	   nhosts, tf->users); 

    /* assuming no multicast in call below (MCast parameter NULL) */
    if (nhosts > 0) {
	MRPC_MakeMulti(CallBack_OP, CallBack_PTR, nhosts, cidlist, rclist,
		       NULL, NULL, NULL, afid);

	for (int i = 0; i < nhosts; i++) {
	    /* host entry may disappear during yield in CallBack() */
	    HostTable *he = FindHostEntry(cidlist[i]);
	    if (he) {
		/* recursively calls DeleteVenus */
		if (rclist[i] < RPC2_ELIMIT)  CleanUpHost(he);

		/* if a file callback, delete any volume callbacks */
		if (!aVCB)  DeleteCallBack(he, &vFid);
	    }
	}
    }

    free(cidlist);
    free(rclist);

    ReleaseWriteLock(&tf->cblock);

    /* Nuke all bad callback entries. */
    struct CallBackEntry *goodc = 0;
    struct CallBackEntry *nextc = 0;
    for (tc = tf->callBacks; tc; tc = nextc) {
	nextc = tc->next;
	if (tc->conn == 0 || tc->conn != aconnid) {
	    FreeCBE(tc);
	    if (aVCB) VCBEs--;
        }
	else goodc = tc;	/* Must be aconnid */
    }

    /* Now see if we have any good callbacks left on this file.  If not, nuke it */
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
  BEGIN_HTML
  <a name="DeleteCallBack"><strong>Delete a callback for <tt>afid</tt> with
  client connected via <tt>aconnid</tt>.  </strong> </a>
  END_HTML 
*/
void DeleteCallBack(HostTable *aconnid, ViceFid *afid) {
    SDeleteCallBack(aconnid, FindEntry(afid));
}

PRIVATE void SDeleteCallBack(HostTable *aconnid, struct FileEntry *af) {
    if (!af) return;

    struct FileEntry *tf = af;
    int	busy = CheckLock(&tf->cblock);	    /* Is it already busy? */
    char aVCB = (af->theFid.Vnode == 0 && af->theFid.Unique == 0);

    struct CallBackEntry **lc = &tf->callBacks;
    struct CallBackEntry *nc = 0;
    for (struct CallBackEntry *tc = tf->callBacks; tc; tc = nc) {
	nc = tc->next;
	if (tc->conn == aconnid) {
	    if (busy) {
		tc->conn = 0;
	    }
	    else {
		FreeCBE(tc);
		if (aVCB) VCBEs--;
		(*lc) = nc;
		tf->users--;
	    }
	    LogMsg(3, SrvDebugLevel, stdout, "SDeleteCallBack for Fid 0x%x.%x.%x, Venus %s.%d",
		    af->theFid.Volume, af->theFid.Vnode, af->theFid.Unique,
		    aconnid->HostName, aconnid->port);
	    break;
	}
	lc = &tc->next;
    }

    /* Now see if there are any left. */
    if (!busy && tf->users <= 0)
	DeleteFileStruct(tf);
}


/*
  BEGIN_HTML
  <a name="DeleteVenus"><strong>Delete all callbacks for a client 
  connected via <tt>aconnid</tt> </strong> </a>
  END_HTML 
*/
void DeleteVenus(HostTable *aconnid) {
    LogMsg(1, SrvDebugLevel, stdout, "DeleteVenus for venus %s.%d",
	     aconnid->HostName, aconnid->port);

    for (int i = 0; i < VHASH; i++) {
	struct FileEntry *nf = 0;
	for (struct FileEntry *tf = hashTable[i]; tf; tf = nf) {
	    /* Pull this out before it gets zapped */
	    nf = tf->next;
	    SDeleteCallBack(aconnid, tf);   /* May or may not delete the block. */
	}
    }
}


/* Delete all the status on a file--used when a file is removed. */
void DeleteFile(ViceFid *afid) {
    LogMsg(3, SrvDebugLevel, stdout, "DeleteFile for Fid 0x%x.%x.%x",
	     afid->Volume, afid->Vnode, afid->Unique);

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
  BEGIN_HTML
  <a name="CodaAddCallBack"><strong>Establish a callback with a client
  for the non-replicated and replicated fid of an object. </strong> </a>
  END_HTML 
*/
CallBackStatus CodaAddCallBack(HostTable *VenusId, ViceFid *Fid, VolumeId VSGVolnum) {
    if (Fid->Volume == VSGVolnum)
	return(AddCallBack(VenusId, Fid));

    ViceFid VSGFid;
    VSGFid.Volume = VSGVolnum;
    VSGFid.Vnode = (Fid)->Vnode;
    VSGFid.Unique = (Fid)->Unique;
    return(AddCallBack(VenusId, &VSGFid));
}

/*
  BEGIN_HTML
  <a name="CodaBreakCallBack"><strong>Break the callback with a client, for the
  non-replicated and replicated fid of an object. </strong> </a>
  END_HTML 
*/
void CodaBreakCallBack(HostTable *VenusId, ViceFid *Fid, VolumeId VSGVolnum) {
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

void CodaDeleteCallBack(HostTable *VenusId, ViceFid *Fid, VolumeId VSGVolnum) {
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

PRIVATE int CompareCBSEnts(struct CBStat *a, struct CBStat *b) {
    if (a->volid < b->volid) return -1;
    if (a->volid > b->volid) return 1;
    return(0);
}

void PrintCallBackState(FILE *fp) {
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
	bzero((char *)CBStats, (int)sizeof(struct CBStat) * MaxVols);

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
			fprintf(fp, "For Fid %x.%x.%x users = %d when counted = %d\n",
				tfe->theFid.Volume, tfe->theFid.Vnode, 
				tfe->theFid.Unique, tfe->users, countcbe);
		    
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
	        fprintf(fp, "\t0x%x  %d FEs (%d VEs), %d CBEs (%d VCBEs)\n",
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


PRIVATE void PrintCBE(struct CallBackEntry *tcbe, FILE *fp) {
    assert (tcbe);
    if (tcbe->conn) {
	unsigned long host = htonl(tcbe->conn->host);
	fprintf(fp, "\tHost %d.%d.%d.%d ",
		(host & 0xff000000)>>24, (host & 0x00ff0000)>>16, 
		(host & 0x0000ff00)>>8, host & 0x000000ff);
	// try to get the host name 
	long cbhostaddr;
	if (sscanf(tcbe->conn->HostName, "%x", &cbhostaddr) == 1) {
	    unsigned long nhost = htonl(cbhostaddr);
	    struct hostent *h = gethostbyaddr((char *)&nhost, 
					      (int)sizeof(unsigned long),
					      AF_INET); 
	    if (h) 
		fprintf(fp, "portal 0x%x name %s\n", 
			tcbe->conn->port, h->h_name);
	    else 
		fprintf(fp, "portal 0x%x name %s\n", 
			tcbe->conn->port, tcbe->conn->HostName);
	}
	else 
	    fprintf(fp, "portal 0x%x name %s\n", 
		    tcbe->conn->port, tcbe->conn->HostName);
    }  
}


PRIVATE void GetCallBacks(ViceFid *fid, FILE *fp) {
    fprintf(fp, "Printing callbacks for 0x%x.%x.%x\n", 
	    fid->Volume, fid->Vnode, fid->Unique);
    struct FileEntry *tfe = FindEntry(fid);
    if (tfe) 
	for (struct CallBackEntry *tcbe = tfe->callBacks; tcbe; tcbe = tcbe->next) 
	    PrintCBE(tcbe, fp);

    fprintf(fp, "End of callbacks for %x.%x.%x\n",
	    fid->Volume, fid->Vnode, fid->Unique); 
}

PRIVATE void GetCallBacks(VolumeId vid, FILE *fp) {
    fprintf(fp, "Printing callbacks for 0x%x\n", vid);

    /* print volume callbacks first */
    ViceFid fid = NullFid; fid.Volume = vid;
    struct FileEntry *tfe = FindEntry(&fid);
    if (tfe) {
	fprintf(fp, "VID 0x%x  Volume Callback\n", vid);
	for (struct CallBackEntry *tcbe = tfe->callBacks; tcbe; tcbe = tcbe->next) 
	    PrintCBE(tcbe, fp);
    }

    /* print file callbacks for all fids in the volume */
    for (int j = 0; j < VHASH; j++) {
	struct FileEntry *nf = 0;
	for (struct FileEntry *tf = hashTable[j]; tf; tf = nf) {
	    nf = tf->next;
	    if (tf->theFid.Volume == vid && tf->theFid.Vnode && tf->theFid.Unique) {
		fprintf(fp, "FID 0x%x.%x.%x\n", tf->theFid.Volume, tf->theFid.Vnode,
			tf->theFid.Unique);
		for (struct CallBackEntry *tcbe = tf->callBacks; tcbe; tcbe = tcbe->next)
		    PrintCBE(tcbe, fp);
	    }
        }
    }
    fprintf(fp, "End of callbacks for %x\n", vid);
}

// print all the callbacks for given fid.
// if the fid is volid.0.0, print callback status
// for the entire volume.
void PrintCallBacks(ViceFid *fid, FILE *fp) {
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
