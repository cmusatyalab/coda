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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/venusvol.h,v 4.5 1998/07/08 22:42:10 jaharkes Exp $";
#endif /*_BLURB_*/





#ifndef _VENUS_VOLUME_H_
#define _VENUS_VOLUME_H_ 1



/*
 *
 * Specification of the Venus Volume abstraction.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdarg.h>
#include <stdio.h>
#include <errors.h>
#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>
#include <mond.h>

/* from util */
#include <dlist.h>
#include <rec_dlist.h>
#include <olist.h>
#include <rec_olist.h>
#include <ohash.h>
#include <rec_ohash.h>

/* from vol */
#include <voldefs.h>
#define	RWRVOL	(REPVOL	+ 1)		/* Should be in voldefs.h!  -JJK */

/* from venus */
#include "comm.h"
#include "simulate.h"
#include "venusrecov.h"
#include "venus.private.h"


/* Forward declarations. */
class ClientModifyLog;
class cmlent;
class cml_iterator;
class cmlstats;

class connent;		/* needed because of circular includes! */
class mgrpent;
class vdb;
class volent;
class vol_iterator;
class cop2ent;
class resent;
class vsr;
class vcbdent;
class rwsent;

/* volume pgid locking type */
enum VolLockType { EXCLUSIVE, SHARED };

/* XXX These should be in vice.h! */
#define	ViceTruncate_OP	    100
#define	ViceTruncate_PTR    ViceNewStore_PTR
#define	ViceUtimes_OP	    101
#define	ViceUtimes_PTR	    ViceNewStore_PTR
#define	ViceChown_OP	    102
#define	ViceChown_PTR	    ViceNewStore_PTR
#define	ViceChmod_OP	    103
#define	ViceChmod_PTR	    ViceNewStore_PTR


/*  *****  Constants  *****  */

#define	VDB	(rvg->recov_VDB)
const int VDB_MagicNumber = 6820348;
const int VDB_NBUCKETS = 512;
const int VOLENT_MagicNumber = 3614246;
const int VOLENTMaxFreeEntries = 8;
const int MLENT_MagicNumber = 5214113;
const int MLENTMaxFreeEntries = 32;

const int BLOCKS_PER_MLE = 6;			    /* rule of thumb */
const int DFLT_MLE = DFLT_CB / BLOCKS_PER_MLE;
const int UNSET_MLE = -1;
const int MIN_MLE = MIN_CB / BLOCKS_PER_MLE;

const int UNSET_TID = -1;

const int V_MAXVOLNAMELEN = 32;
const unsigned V_DEFAULTAGE = 600;		/* in SECONDS */
const unsigned V_UNSETAGE = (unsigned)-1;	/* huge */
const unsigned V_DEFAULTREINTLIMIT = 30000;	/* in MILLESECONDS */
const unsigned V_UNSETREINTLIMIT = (unsigned)-1;/* huge */

const int VSR_FLUSH_HARD = 1;
const int VSR_FLUSH_NOT_HARD = 0;

#define VCBDB	(rvg->recov_VCBDB)
const int VCBDB_MagicNumber = 3433089;
const int VCBDB_NBUCKETS = 32;
const int VCBDENT_MagicNumber = 8334738;


/* Volume-User modes. */
#define	VM_MUTATING	    0x1
#define	VM_OBSERVING	    0x2
#define	VM_RESOLVING	    0x4
#define	VM_NDELAY	    0x8		/* this is really a flag!  it is not exclusive with the others! */
                                        /* Indicates the caller doesn't want to be put to sleep if the */
                                        /* volume is already locked.  It's necessary to keep daemons */
                                        /* from getting ``stuck'' on volumes already in use. */
/*  *****  Types  ***** */

class cmlstats {
  public:
    int store_count;					/* number of store records */
    float store_size;				/* size (in bytes) of store records, excluding contents */
    float store_contents_size;			/* size (in bytes) of store record contents */
    int other_count;					/* number of records other than stores */
    float other_size;				/* size (in bytes) of non-stores */

    cmlstats() {
	store_count = 0;
	store_size = 0.0;
	store_contents_size = 0.0;
	other_count = 0;
	other_size = 0.0;
    }

    void operator+=(cmlstats& addend) {
	store_count += addend.store_count;
	store_size += addend.store_size;
	store_contents_size += addend.store_contents_size;
	other_count += addend.other_count;
	other_size += addend.other_size;
    }
};


/* Log containing records of partitioned operations performed at the client. */
/* This type is persistent! */
class ClientModifyLog {
  friend class cmlent;
  friend class cml_iterator;
  friend class volent;

    rec_dlist list;	 			/* link to cmlents */
    /*T*/vuid_t	owner;				/* writer of all entries in Log */
    /*T*/long entries;                          /* number of records in the CML */
    /*T*/long entriesHighWater;                 /* reset when count == zero */
    /*T*/long bytes;                            /* number of bytes used by CML */
    /*T*/long bytesHighWater;                   /* reset when size == 0 */
    cmlstats cancellations;

    /* Size of the Log -- private, because it is only called
     * to reset the tranients bytes and bytesHighWater
     */
    long _bytes();

  public:
    ClientModifyLog() { ResetTransient(); }  /* MUST be called within transaction! */
    void ResetTransient();
    virtual ~ClientModifyLog() { ASSERT(count() == 0); } /* MUST be called within transaction! */
    void ResetHighWater() { entriesHighWater = entries; bytesHighWater = bytes; }
    void Clear();

    /* Log optimization routines. */
    cmlent *LengthWriter(ViceFid *);
    cmlent *UtimesWriter(ViceFid *);

    /* Reintegration routines. */
    void TranslateFid(ViceFid *, ViceFid *);
    int COP1(char *, int, ViceVersionVector *);
    void LockObjs(int);
    void UnLockObjs(int);
    void MarkFailedMLE(int);
    void HandleFailedMLE();
    void MarkCommittedMLE(RPC2_Unsigned);
    void CancelPending();
    void ClearPending();
    void CancelStores();

    void GetReintegrateable(int, int *);
    cmlent *GetFatHead(int);

    /* Routines for handling inconsistencies and safeguarding against catastrophe! */
    void MakeUsrSpoolDir(char *);
    int	CheckPoint(char *);

    void AttachFidBindings();

    long logBytes() {return bytes;}
    long logBytesHighWater() {return bytesHighWater;}
    long size();
    int count() { return list.count(); }
    long countHighWater() {return entriesHighWater;}
    vuid_t Owner() { return owner; }

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);

    /* local-repair methods */
    void IncThread(int);                                /*N*/
    void IncPack(char **, int *, int);                  /*N*/
    void IncCommit(ViceVersionVector *, int);           /*U*/
    void IncAbort(int =UNSET_TID);                      /*U*/
    void IncGetStats(cmlstats&, cmlstats&, int =UNSET_TID); /*N*/
    int IncReallocFids(int);                            /*U*/
    int HaveElements(int);                              /*N*/
};

/* local-repair addition */
struct CmlFlags {
    unsigned to_be_repaired : 1;
    unsigned repair_mutation : 1;
    unsigned frozen : 1;			/* do not cancel */
    unsigned cancellation_pending : 1;		/* once unfrozen */
    /*T*/unsigned failed : 1;			/* offending record */
    /*T*/unsigned committed : 1;		/* committed at server */
    unsigned reserved : 26;
};


/*
  BEGIN_HTML
  <a name="cmlent"><strong> class definition for logged mutation operation entry </strong></a>
  END_HTML
*/
/* local-repair modification */
/* Entries representing partitioned operations performed at the client. */
/* This type is persistent! */
class cmlent {
  friend class ClientModifyLog;
  friend class cml_iterator;
  friend class volent;
  friend class fsobj;
  friend class simulator;
  friend int PathAltered(ViceFid *, char *, ClientModifyLog *, cmlent *);

    ClientModifyLog *log;
    rec_dlink handle;

    ViceStoreId	sid;		/* transaction identifier */
    Date_t time;		/* mtime of operation */
    UserId uid;			/* author of operation */
    int tid;			/* local-repair addition */
    CmlFlags flags;		/* local-repair addition */

    /* Discriminant and sub-type specific members. */
    int opcode;
    union {
	struct {				
	    ViceFid Fid;
	    RPC2_Unsigned Length;
	    /* T */ViceVersionVector VV;
	    RPC2_Integer Offset;		/* for partial reintegration */
	    ViceReintHandle RHandle[VSG_MEMBERS];
	} u_store;
	struct {				
	    ViceFid Fid;
	    RPC2_Unsigned Length;
	    /* T */ViceVersionVector VV;
	} u_truncate;
	struct {				
	    ViceFid Fid;
	    Date_t Date;
	    /* T */ViceVersionVector VV;
	} u_utimes;
	struct {				
	    ViceFid Fid;
	    UserId Owner;
	    /* T */ViceVersionVector VV;
	} u_chown;
	struct {				
	    ViceFid Fid;
	    RPC2_Unsigned Mode;
	    /* T */ViceVersionVector VV;
	} u_chmod;
	struct {
	    ViceFid PFid;
	    RPC2_String Name;
	    ViceFid CFid;
	    RPC2_Unsigned Mode;
	    /* T */ViceVersionVector PVV;
	} u_create;
	struct {
	    ViceFid PFid;
	    RPC2_String Name;
	    ViceFid CFid;
	    int LinkCount;
	    /* T */ViceVersionVector PVV;
	    /* T */ViceVersionVector CVV;
	} u_remove;
	struct {
	    ViceFid PFid;
	    RPC2_String Name;
	    ViceFid CFid;
	    /* T */ViceVersionVector PVV;
	    /* T */ViceVersionVector CVV;
	} u_link;
	struct {
	    ViceFid SPFid;
	    RPC2_String OldName;
	    ViceFid TPFid;
	    RPC2_String NewName;
	    ViceFid SFid;
	    /* T */ViceVersionVector SPVV;
	    /* T */ViceVersionVector TPVV;
	    /* T */ViceVersionVector SVV;
	} u_rename;
	struct {
	    ViceFid PFid;
	    RPC2_String Name;
	    ViceFid CFid;
	    RPC2_Unsigned Mode;
	    /* T */ViceVersionVector PVV;
	} u_mkdir;
	struct {
	    ViceFid PFid;
	    RPC2_String Name;
	    ViceFid CFid;
	    /* T */ViceVersionVector PVV;
	    /* T */ViceVersionVector CVV;
	} u_rmdir;
	struct {
	    ViceFid PFid;
	    RPC2_String OldName;
	    RPC2_String NewName;
	    ViceFid CFid;
	    RPC2_Unsigned Mode;
	    /* T */ViceVersionVector PVV;
	} u_symlink;
	struct {
	    ViceFid Fid;
	    RPC2_Unsigned Length;
	    Date_t Date;
	    UserId Owner;
	    RPC2_Unsigned Mode;
	    ViceVersionVector OVV;
	} u_repair;
    } u;

    /*T*/dlist *fid_bindings;	/* list of (bindings to) fids referenced by this record */

    /*T*/dlist *pred;		/* list of (bindings to) predecessor cmlents */
    /*T*/dlist *succ;		/* list of (bindings to) successor cmlents */

  public:
    void *operator new(size_t);
    cmlent(ClientModifyLog *, time_t, vuid_t, int, int ...);	/* local-repair modification */
    void ResetTransient();
    ~cmlent();
    void operator delete(void *, size_t);

    /* Size of an entry */
    long bytes();

    /* Log optimization routines. */
    int cancel();

    /* Reintegration routines. */
    int realloc();
    void translatefid(ViceFid *, ViceFid *);
    void thread();
    int size();
    void pack(PARM **);
    void commit(ViceVersionVector *);
    int cancelstore();
    int Aged();
    unsigned long ReintTime();
    unsigned long ReintAmount();
    int IsReintegrating();
    int IsFrozen() { return flags.frozen; }
  
    /* for partial reintegration */
    int HaveReintegrationHandle();
    void ClearReintegrationHandle();
    int DoneSending();
    int GetReintegrationHandle();
    int ValidateReintegrationHandle();
    int WriteReintegrationHandle();
    int CloseReintegrationHandle(char *, int, ViceVersionVector *);

    /* Routines for handling inconsistencies and safeguarding against catastrophe! */
    void abort();
    int checkpoint(FILE *);
    void writeops(FILE *);

    void AttachFidBindings();
    void DetachFidBindings();
    void LockObj();
    void UnLockObj();

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);

    /* local-repair addition */
    int GetTid() { return tid; }                                /*N*/
    void SetTid(int);                                           /*U*/
    int ReintReady();                                           /*U*/
    int ContainLocalFid();                                      /*N*/
    void TranslateFid(ViceFid *, ViceFid *);                    /*T*/
    int LocalFakeify();                                         /*U*/
    void CheckRepair(char *, int *, int *);                     /*N*/
    int DoRepair(char *, int);                                  /*U*/
    void GetLocalOpMsg(char *);                                 /*N*/
    void SetRepairFlag();                                       /*U*/
    void SetRepairMutationFlag();                               /*U*/
    int IsToBeRepaired() { return flags.to_be_repaired; }       /*N*/
    int IsRepairMutation() { return flags.repair_mutation; }    /*N*/
    int InLocalRepairSubtree(ViceFid *);                        /*N*/
    int InGlobalRepairSubtree(ViceFid *);                       /*N*/
    void GetVVandFids(ViceVersionVector *[3], ViceFid *[3]);    /*N*/
    void GetAllFids(ViceFid *[3]);    				/*N*/
};

#define	CmlIterOrder DlIterOrder
#define	CommitOrder DlAscending
#define	AbortOrder  DlDescending

class cml_iterator {
    ClientModifyLog *log;
    CmlIterOrder order;
    ViceFid *fidp;
    ViceFid fid;
    cmlent *prelude;	/* start iteration after this element */
    dlist_iterator *next;

  public:
    cml_iterator(ClientModifyLog&, CmlIterOrder =CommitOrder, ViceFid * =0, cmlent * =0);
    ~cml_iterator();
    cmlent *operator()();
};


/* *****  Declaractions for VCB data collection ***** */

typedef enum { Acquire,		/* new once/volent instantiation */
	       Validate, 	/* successful volume validation */
	       FailedValidate, 
	       Break,       	/* broken volume callback */
	       Clear, 		/* cleared callback due to connectivity change */
	       NoStamp,		/* missed opportunity for volume validation */
} VCBEventType;

/* 
 * Data block for a vcb event.  Used for acquire and break events,
 * for which data is scattered throughout the code path.  We tag 
 * the thread (vproc) with this structure to gather the data.
 */
struct vcbevent {
  unsigned	nobjs;		/* # objects cached in volume */
  unsigned	nchecked;	/* # objects that were checked (no callback) */
  unsigned	nfailed;	/* # checks that failed */

  vcbevent(unsigned n = 0) {
    nobjs = n;
    nchecked = 0;
    nfailed = 0;
  }
};

#define volonly nchecked	/* for break event, steal a field to */
                                /* distinguish between obj and vol breaks */

#define	PRINT_VCBEVENT(event)	((event) == Acquire ? "Acquire" :\
				 (event) == Validate ? "Validate" :\
				 (event) == FailedValidate ? "FailedValidate":\
				 (event) == Break ? "Break":\
				 (event) == Clear ? "Clear":\
				 (event) == NoStamp ? "NoStamp":\
				 "???")

/* 
 * Volume callback database.  We need this in addition to the vdb
 * because volents can get flushed.
 */
class vcbdb {
  friend void VolInit();
  friend class vcbd_iterator;
  friend class vcbdent;
  friend void ReportVCBEvent(VCBEventType, VolumeId, vcbevent *);

    int MagicNumber;

    /* The hash table. */
    rec_ohashtab htab;

    /* Constructors, destructors. */
    void *operator new(size_t);
    vcbdb();
    ~vcbdb() { abort(); } 	/* it never goes away... */
    void operator delete(void *, size_t) { abort(); } /* can never be invoked */
    void ResetTransient();

    /* Allocation/Deallocation routines. */
    vcbdent *Create(VolumeId, char *);

  public:
    vcbdent *Find(VolumeId);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};


class vcbdent {
  friend class vcbdb;
  friend class vcbd_iterator;
  friend void ReportVCBEvent(VCBEventType, VolumeId, vcbevent *);
  friend void CheckVCB();

    int MagicNumber;

    VolumeId vid;			/* key */
    char name[V_MAXVOLNAMELEN];

    rec_olink handle;			/* link for htab */

    void *operator new(size_t);
    vcbdent(VolumeId, char *);
    ~vcbdent() { abort(); }	/* it never goes away... */
    void operator delete(void *, size_t) { abort(); } /* can never be invoked */

    struct VCBStatistics data;	/* in mond-convenient format */

  public:
    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

class vcbd_iterator : public rec_ohashtab_iterator {

  public:
    vcbd_iterator(void * =(void *)-1);
    vcbdent *operator()();
};

void VolDaemon() /* used to be member of class vdb (Satya 3/31/95) */;
void TrickleReintegrate(); /* used to be in class vdb (Satya 5/20/95) */


/* Volume Database.  Dictionary for volume entries (volents). */
class vdb {
  friend void VolInit();
  friend void VOLD_Init();
  friend void VolDaemon();
  friend class cmlent;
  friend class volent;
  friend class vol_iterator;
  friend class fsobj;
  friend void RecovInit();

    int MagicNumber;

    /* Size parameters. */
    int MaxMLEs;            /* Limit on number of MLE's over _all_ volumes */
    int AllocatedMLEs;

    /* The hash table. */
    rec_ohashtab htab;

    /* The free list. */
    rec_olist freelist;

    /* The mle free list. */
    rec_dlist mlefreelist;

    /* Constructors, destructors. */
    void *operator new(size_t);
    vdb();
    void ResetTransient();
    ~vdb() { abort(); }
    void operator delete(void *, size_t);

    /* Allocation/Deallocation routines. */
    volent *Create(VolumeInfo *, char *);

    /* Daemon functions. */
    void Validate();
    void GetDown();
    void FlushCOP2();
    void WriteBack();
    void FlushVSR();
    void CheckPoint(unsigned long);
    void CheckReintegratePending();
    void CheckLocalSubtree();

  public:
    volent *Find(VolumeId);
    volent *Find(char *);
    int Get(volent **, VolumeId);
    int Get(volent **, char *);
    void Put(volent **);
    void FlushVolume();

    void AttachFidBindings();
    int WriteDisconnect(unsigned =V_UNSETAGE, unsigned =V_UNSETREINTLIMIT);
    int WriteReconnect();
    void GetCmlStats(cmlstats&, cmlstats&);

    int CallBackBreak(VolumeId);
    void TakeTransition();	/* also a daemon function */

    void SaveCacheInfo(VolumeId, int, int, int);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int, int =0);

    void ListCache(FILE *, int long_format=1, unsigned int valid=3);
};


/* A volume is in exactly one of these states. */
typedef enum {	Hoarding,
		Emulating,
		Logging,
		Resolving,
} VolumeStateType;


/* We save some space by packing booleans into a bit-vector. */
struct VolFlags {
    /*T*/unsigned transition_pending : 1;
    /*T*/unsigned demotion_pending : 1;
    /*T*/unsigned valid : 1;			/* not used yet */
    /*T*/unsigned online : 1;			/* not used yet */
    /*T*/unsigned usecallback : 1;		/* should be deprecated? */
    unsigned logv : 1;				/* log mutations, allow fetches */
    unsigned allow_asrinvocation : 1;		/* asr's allowed in this volume */
    unsigned has_local_subtree : 1;		/* indicating whehter this volume contains local subtrees */
    /*T*/unsigned reintegratepending : 1;	/* are we waiting for tokens? */
    /*T*/unsigned reintegrating : 1;		/* are we reintegrating now? */
    /*T*/unsigned repair_mode : 1;		/* 0 --> normal, 1 --> repair */
    /*T*/unsigned resolve_me: 1;		/* resolve reintegrated objects */
    /*T*/unsigned weaklyconnected : 1;		/* are we weakly connected? */ 
    unsigned reserved : 19;
};


/* Descriptor for a range of pre-allocated fids. */
struct FidRange : public ViceFidRange {
    unsigned long AllocHost;			/* shouldn't be needed! -JJK */

    FidRange() {
	Vnode = 0;
	this->Unique = 0;
	Stride = 0;
	Count = 0;
	AllocHost = 0;
    }
};

/* local-repair modification */
/* A volume entry. */
class volent {
  friend void VolInit();
  friend int GetRootVolume();
  friend void TrickleReintegrate();
  friend void Reintegrate(volent *);
  friend void Resolve(volent *);
  friend class ClientModifyLog;
  friend class cmlent;
  friend class cml_iterator;
  friend class vdb;
  friend class vol_iterator;
  friend class fsdb;
  friend class fsobj;
  friend class fso_vol_iterator;
  friend class vsgdb;
  friend class vsgent;
  friend class userent;
  friend long CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
  friend class vproc;
  friend class simulator;
  friend void InitVCBData(VolumeId);
  friend void ReportVCBEvent(VCBEventType, VolumeId, vcbevent *);

    int MagicNumber;

    /* Keys. */
    char name[V_MAXVOLNAMELEN];
    VolumeId vid;
    /*T*/vsgent *vsg;				/* pointer to object's vsg; reduces vsgdb::Get() calls */
    rec_olink handle;				/* link for {htab, freelist} */

    /* Assoc(key). */
    int type;
    unsigned long host;
    union {
	struct {
	    int	Epoch;				/* not used yet! */
	    VolumeId RWVols[MAXHOSTS];
	} rep;
	struct {
	    VolumeId REPVol;
	} rwr;
    } u;

    /* State information. */
    /*T*/VolumeStateType state;
    /*T*/short observer_count;
    /*T*/short mutator_count;
    /*T*/short waiter_count;
    /*T*/short resolver_count;
    /*T*/short shrd_count;		/* for volume pgid locking */
    /*T*/short excl_count;		/* for volume pgid locking */
    /*T*/int excl_pgid;			/* pgid for the exclusive lock holder */
    int reint_id_gen;                   /* reintegration id generator */
    int cur_reint_tid;			/* tid of reintegration in progress, if any */
    VolFlags flags;

    /* Fso's. */
    /*T*/olist *fso_list;

    /* Preallocated Fids. */
    FidRange FileFids;
    FidRange DirFids;
    FidRange SymlinkFids;

    /* COP2 stuff. */
    /*T*/dlist *cop2_list;

    /* Reintegration stuff. */
    ClientModifyLog CML;
    struct Lock CML_lock;		/* for checkpoint/mutator synchronization */
    unsigned AgeLimit;			/* min age of log records in SECONDS */
    unsigned ReintLimit;		/* work limit, in MILLESECONDS */
    /*T*/Unique_t FidUnique;
    /*T*/RPC2_Unsigned SidUnique;
    /*T*/int OpenAndDirtyCount;
    // The next four are now transient - bnoble
    /*T*/int RecordsCancelled;
    /*T*/int RecordsCommitted;
    /*T*/int RecordsAborted;
    /*T*/int FidsRealloced;
    /*T*/long BytesBackFetched;

    /* Resolution stuff. */
    /*T*/olist *res_list;

    /* Callback stuff. */
    /*T*/CallBackStatus VCBStatus;      /* do we have a volume callback? */
    ViceVersionVector VVV;              /* (maximal) volume version vector */
    /*T*/int VCBHits;			/* number of references hitting this callback */

    /* VSR (Vmon Session Record) stuff. */
    /*T*/olist *vsr_list;
    /*T*/VmonSessionId VsrUnique;
    /*T*/VmonAVSG AVSG;

    /* Cache statistics fields have been removed.  The statistics are stored in the VSR. */
    /* If maintaining these statistics in the VSR is too slow, we'll have to go back to  */
    /* storing transient values here.  However, this makes the statistics less accurate  */
    /* since all user accesses are counted here and then reported once for each user.    */

    /* Advice stuff. */
    /* save stuff from the previous session for reporting to the advice monitor -- gross! */
    long DisconnectionTime;
    long DiscoRefCounter;     /* value of FSDB->RefCounter at disconnection */

    /*T*/int saved_uid;
    /*T*/int saved_hits;
    /*T*/ int saved_misses;

    /* Local synchronization state. */
    /*T*/char sync;
    /*T*/int refcnt;			/* count of fsobj's plus active threads */

    /* Read/Write Sharing Stats */
    long current_disc_time;		/* disconnection time for the current period */
    long current_reco_time;		/* reconnection time for the current period */
    short current_rws_cnt;		/* read/write sharing count */
    short current_disc_read_cnt;	/* disconnected read count */
    rec_dlist rwsq;			/* a queue of read/write sharing stats to be reported */

    /* Constructors, destructors, and private utility routines. */
    void *operator new(size_t);
    volent(VolumeInfo *, char *);
    void ResetTransient();
    volent(volent&) { abort(); }    		/* not supported! */
    operator=(volent&) { abort(); return(0); }	/* not supported! */
    ~volent();
    void operator delete(void *, size_t);
    void Recover();
    void hold();
    void release();
    void InitStatsVSR(vsr *);
    void UpdateStatsVSR(vsr *);
  public:
    /* Volume synchronization. */
    int Enter(int, vuid_t);
    void Exit(int, vuid_t);
    void TakeTransition();
    void DownMember(long);
    void UpMember(long);
    void WeakMember();
    void StrongMember();
    void ResetStats() { CML.ResetHighWater(); }
    void Wait();
    void Signal();
    void Lock(VolLockType, int = 0);		
    void UnLock(VolLockType);		  
    int GetConn(connent **, vuid_t);
    int Collate(connent *, int);
    int GetMgrp(mgrpent **, vuid_t, RPC2_CountedBS * =0);
    int Collate_NonMutating(mgrpent *, int);
    int Collate_COP1(mgrpent *, int, ViceVersionVector *);
    int Collate_Reintegrate(mgrpent *, int, ViceVersionVector *);
    int Collate_COP2(mgrpent *, int);

    /* Allocation routines. */
    int AllocFid(ViceDataType, ViceFid *, RPC2_Unsigned *, vuid_t, int =0);
    ViceFid GenerateLocalFid(ViceDataType);
    ViceFid GenerateFakeFid();
    ViceStoreId GenerateStoreId();

    /* User-visible volume status. */
    int GetVolStat(VolumeStatus *, RPC2_BoundedBS *,
		    RPC2_BoundedBS *, RPC2_BoundedBS *, vuid_t);
    int SetVolStat(VolumeStatus *, RPC2_BoundedBS *,
		    RPC2_BoundedBS *, RPC2_BoundedBS *, vuid_t);

    /* COP2 routines. */
    int COP2(mgrpent *, RPC2_CountedBS *);
    int COP2(mgrpent *, ViceStoreId *, ViceVersionVector *);
    int FlushCOP2(time_t =0);
    int FlushCOP2(mgrpent *, RPC2_CountedBS *);
    void GetCOP2(RPC2_CountedBS *);
    cop2ent *FindCOP2(ViceStoreId *);
    void AddCOP2(ViceStoreId *, ViceVersionVector *);
    void ClearCOP2(RPC2_CountedBS *);

    /* local-repair modifications to the following methods */
    /* Modlog routines. */
    int LogStore(time_t, vuid_t, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogSetAttr(time_t, vuid_t, ViceFid *,
		    RPC2_Unsigned, Date_t, UserId, RPC2_Unsigned, int = UNSET_TID);
    int LogTruncate(time_t, vuid_t, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogUtimes(time_t, vuid_t, ViceFid *, Date_t, int = UNSET_TID);
    int LogChown(time_t, vuid_t, ViceFid *, UserId, int = UNSET_TID);
    int LogChmod(time_t, vuid_t, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogCreate(time_t, vuid_t, ViceFid *, char *, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogRemove(time_t, vuid_t, ViceFid *, char *, ViceFid *, int, int = UNSET_TID);
    int LogLink(time_t, vuid_t, ViceFid *, char *, ViceFid *, int = UNSET_TID);
    int LogRename(time_t, vuid_t, ViceFid *, char *,
		   ViceFid *, char *, ViceFid *, ViceFid *, int, int = UNSET_TID);
    int LogMkdir(time_t, vuid_t, ViceFid *, char *, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogRmdir(time_t, vuid_t, ViceFid *, char *, ViceFid *, int = UNSET_TID);
    int LogSymlink(time_t, vuid_t, ViceFid *, char *,
		    char *, ViceFid *, RPC2_Unsigned, int = UNSET_TID);
    int LogRepair(time_t, vuid_t, ViceFid *, RPC2_Unsigned,
		  Date_t, UserId, RPC2_Unsigned, int = UNSET_TID);
    /* local-repair modifications to the above methods */

    void CancelStores(ViceFid *);
    void RestoreObj(ViceFid *);
    int	CheckPointMLEs(vuid_t, char *);
    int LastMLETime(unsigned long *);
    int PurgeMLEs(vuid_t);
    int WriteDisconnect(unsigned =V_UNSETAGE, unsigned =V_UNSETREINTLIMIT);
    int WriteReconnect();

    /* Reintegration routines. */
    void Reintegrate();
    int IncReintegrate(int);
    int PartialReintegrate(int);
    void SetReintegratePending();
    void CheckReintegratePending();
    void ClearReintegratePending();
    int IsReintegrating() { return flags.reintegrating; }
    int ReadyToReintegrate();

    /* Resolution routines. */
    void Resolve();
    void ResSubmit(char **, ViceFid *);
    int ResAwait(char *);
    int RecResolve(connent *, ViceFid *);

    /* Repair routines. */
    int EnableRepair(vuid_t, VolumeId *, vuid_t *, unsigned long *);
    int DisableRepair(vuid_t);
    int Repair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int ConnectedRepair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int DisconnectedRepair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int LocalRepair(fsobj *, ViceStatus *, char *fname, ViceFid *);
    int IsUnderRepair(vuid_t);

    /* ASR routines */
    int EnableASR(vuid_t);
    int DisableASR(vuid_t);
    int IsASRAllowed();

    /* Callback routines */
    void UseCallBack(int);
    int GetVolAttr(vuid_t);
    int ValidateFSOs();
    int CallBackBreak();
    void ClearCallBack();
    void SetCallBack();
    int WantCallBack();
    int HaveCallBack() { return(VCBStatus == CallBackSet); }
    int HaveStamp() { return(VV_Cmp(&VVV, &NullVV) != VV_EQ); }
    void PackVS(int, RPC2_CountedBS *);
    void CollateVCB(mgrpent *, RPC2_Integer *, CallBackStatus *);

    /* VSR routines. */
    vsr *GetVSR(vuid_t);
    void PutVSR(vsr *);
    void FlushVSRs(int);

    /* Utility routines. */
    void GetHosts(unsigned long *);
    unsigned long GetAVSG(unsigned long * =0);
    int AvsgSize();
    int WeakVSGSize();
    int IsReadWrite() { return (type == RWVOL); }
    int IsReadOnly() { return (type == ROVOL); }
    int IsBackup() { return (type == BACKVOL); }
    int IsReplicated() { return (type == REPVOL); }
    int IsReadWriteReplica() { return (type == RWRVOL); }
    int IsDisconnected() { return (state == Emulating); }
    int IsWriteDisconnected() { return (state == Logging); }
    int IsWeaklyConnected() { return flags.weaklyconnected; }
    int IsHostedBy(unsigned long);
    void GetMountPath(char *, int =1);

    /* local-repair addition */
    void IncAbort(int);                         /*U*/
    void CheckTransition();                     /*N*/
    void TranslateCMLFid(ViceFid *, ViceFid *); /*T*/
    void ClearRepairCML();                      /*U*/
    int GetReintId();                           /*U*/
    VolumeId GetVid() { return vid; }           /*N*/
    ClientModifyLog *GetCML() { return &CML; }  /*N*/
    int ContainUnrepairedCML();			/*N*/
    void CheckLocalSubtree();			/*U*/

    /* advice addition */
    void DisconnectedCacheMiss(vproc *, vuid_t, ViceFid *, char *);
    void TriggerReconnectionQuestionnaire();
    void NotifyStateChange();
    void GetVolInfoForAdvice(int *, int *);
    void SetDisconnectionTime();
    void UnsetDisconnectionTime();
    long GetDisconnectionTime() { return(DisconnectionTime); }
    void SetDiscoRefCounter();
    void UnsetDiscoRefCounter();
    long GetDiscoRefCounter() { return(DiscoRefCounter); }
    void SaveCacheInfo(int, int, int);
    void GetCacheInfo(int, int *, int *);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);

    void ListCache(FILE *, int long_format=1, unsigned int valid=3);

    /* Read/Write Sharing Stats */
    void RwStatUp();
    void RwStatDown();
    rec_dlist *GetRwQueue();
};

class vol_iterator : public rec_ohashtab_iterator {

  public:
    vol_iterator(void * =(void *)-1);
    volent *operator()();
};

/* Entries representing pending COP2 events. */
class cop2ent : public dlink {
  friend class volent;

    ViceStoreId sid;
    ViceVersionVector updateset;
    time_t time;

    void *operator new(size_t);
    cop2ent(ViceStoreId *, ViceVersionVector *);
    cop2ent(cop2ent&);		/* not supported! */
    operator=(cop2ent&);	/* not supported! */
    virtual ~cop2ent();
    void operator delete(void *, size_t);

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    void print();
    void print(FILE *);
    void print(int);
};


/* Entries representing fids that need to be resolved. */
class resent : public olink {
  friend void volent::Resolve();
  friend void volent::ResSubmit(char **, ViceFid *);
  friend int volent::ResAwait(char *);

    ViceFid fid;
    int result;
    int refcnt;

    resent(ViceFid *);
    resent(resent&);		/* not supported! */
    operator=(resent&);		/* not supported! */
    virtual ~resent();

    void HandleResult(int);

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    void print();
    void print(FILE *);
    void print(int);
};


/* Volume session records (statistics gathering). */
class vsr : public olink {
  friend class volent;
  friend void VmonUpdateSession(vproc *vp, ViceFid *key, fsobj *f, volent *vol, vuid_t vuid, enum CacheType datatype, enum CacheEvent event, unsigned long blocks);

    vuid_t uid;
    RPC2_Unsigned starttime;
    RPC2_Unsigned endtime;
    RPC2_Unsigned cetime;
    VmonSessionEventArray events;
    SessionStatistics stats;
    SessionStatistics initstats;
    CacheStatistics cachestats;

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    vsr(vuid_t);
    vsr(vsr&);			/* not supported! */
    operator=(vsr&);		/* not supported! */
    virtual ~vsr();

    void RecordEvent(int, int, RPC2_Unsigned);

    void print();
    void print(FILE *);
    void print(int);
};

class rwsent : public rec_dlink {
public:
    short sharing_count;
    short disc_read_count;
    int   disc_duration;
    void *operator new(size_t);
    rwsent(short, short, int);
    ~rwsent();
    void operator delete(void *, size_t);
};

/*  *****  Variables  *****  */
extern int MLEs;
extern int LogOpts;
extern int vcbbreaks;
extern char vol_sync;
extern char VCBEnabled;

/*  *****  Functions/Procedures  *****  */

/* venusvol.c */
extern void VolInit();
extern int VOL_HashFN(void *);
extern int GetRootVolume();

/* vol_COP2.c */
const int COP2SIZE = 1024;

/* vol_daemon.c */
extern void VOLD_Init();

/* vol_reintegrate.c */
extern void Reintegrate(volent *);

/* vol_resolve.c */
extern void Resolve(volent *);

/* vol_cml.c */
extern void RecoverPathName(char *, ViceFid *, ClientModifyLog *, cmlent *);
extern int PathAltered(ViceFid *, char *, ClientModifyLog *, cmlent *);

/* vol_vcb.c */
extern void InitVCBData(VolumeId);
extern void AddVCBData(unsigned, unsigned =0);
extern void DeleteVCBData();
extern void ReportVCBEvent(VCBEventType, VolumeId, vcbevent * =NULL);


#define	VOL_ASSERT(v, ex)\
{\
    if (!(ex)) {\
	(v)->print(logFile);\
	Choke("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
    }\
}

#define	PRINT_VOLTYPE(type)	((type) == RWVOL ? "RWVOL" :\
				 (type) == ROVOL ? "ROVOL" :\
				 (type) == BACKVOL ? "BACKVOL" :\
				 (type) == REPVOL ? "REPVOL" :\
				 (type) == RWRVOL ? "RWRVOL" :\
				 "???")
#define	PRINT_VOLSTATE(state)	((state) == Hoarding ? "Hoarding" :\
				 (state) == Resolving ? "Resolving" :\
				 (state) == Emulating ? "Emulating" :\
				 (state) == Logging ? "Logging":\
				 "???")
#define	PRINT_VOLMODE(mode)	((mode) & VM_OBSERVING ? "Observing" :\
				 (mode) & VM_MUTATING ? "Mutating" :\
				 (mode) & VM_RESOLVING ? "Resolving" :\
				 "???")
#define	PRINT_MLETYPE(op) ((op) == ViceNewStore_OP ? "Store" :\
			    (op) == ViceTruncate_OP ? "Truncate" :\
			    (op) == ViceUtimes_OP ? "Utimes" :\
			    (op) == ViceChown_OP ? "Chown" :\
			    (op) == ViceChmod_OP ? "Chmod" :\
			    (op) == ViceCreate_OP ? "Create" :\
			    (op) == ViceRemove_OP ? "Remove" :\
			    (op) == ViceLink_OP ? "Link" :\
			    (op) == ViceRename_OP ? "Rename" :\
			    (op) == ViceMakeDir_OP ? "Mkdir" :\
			    (op) == ViceRemoveDir_OP ? "Rmdir" :\
			    (op) == ViceSymLink_OP ? "Symlink" :\
			    (op) == ViceRepair_OP ? "Repair" :\
			    "???")


#endif	not _VENUS_VOLUME_H_
