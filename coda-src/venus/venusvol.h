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
#include <rpc2/errors.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>

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
/* Was: EXCLUSIVE, *SHARED*: name clash on Solaris */
enum VolLockType { EX_VOL_LK, SH_VOL_LK };

/* XXX These should be in vice.h! */
#define	OLDCML_Truncate_OP	100
#define	OLDCML_Truncate_PTR	OLDCML_NewStore_PTR
#define	OLDCML_Utimes_OP	101
#define	OLDCML_Utimes_PTR	OLDCML_NewStore_PTR
#define	OLDCML_Chown_OP	        102
#define	OLDCML_Chown_PTR	OLDCML_NewStore_PTR
#define	OLDCML_Chmod_OP	        103
#define	OLDCML_Chmod_PTR	OLDCML_NewStore_PTR


/*  *****  Constants  *****  */

#define	VDB	(rvg->recov_VDB)
const int VDB_MagicNumber = 6820348;
const int VDB_NBUCKETS = 512;
const int VOLENT_MagicNumber = 3614246;
const int VOLENTMaxFreeEntries = 8;
const int MLENT_MagicNumber = 5214113;
const int MLENTMaxFreeEntries = 32;

const int BLOCKS_PER_MLE = 6;			    /* rule of thumb */
const int MIN_MLE = MIN_CB / BLOCKS_PER_MLE;

const int UNSET_TID = -1;

const int V_MAXVOLNAMELEN = 32;
const unsigned V_DEFAULTAGE = 60;		/* in SECONDS */
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
#define	VM_NDELAY	    0x8		/* this is really a flag!  it is not
                                           exclusive with the others!
                                           Indicates the caller doesn't want
                                           to be put to sleep if the volume is
                                           already locked. It's necessary to
                                           keep daemons from getting ``stuck''
                                           on volumes already in use. */
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
    virtual ~ClientModifyLog() { CODA_ASSERT(count() == 0); } /* MUST be called within transaction! */
    void ResetHighWater() { entriesHighWater = entries; bytesHighWater = bytes; }
    void Clear();

    /* Log optimization routines. */
    cmlent *LengthWriter(ViceFid *);
    cmlent *UtimesWriter(ViceFid *);

    /* Reintegration routines. */
    void TranslateFid(ViceFid *, ViceFid *);
    int COP1(char *, int, ViceVersionVector *, int outoforder);
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
    int  OutOfOrder(int tid);				/*N*/
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
	    ViceReintHandle RHandle;
	    struct in_addr ReintPH;		/* chosen primaryhost & index */
	    int            ReintPHix;		/* for the partial reint. */
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
    unsigned long ReintTime(unsigned long bw);
    unsigned long ReintAmount();

    int Freeze();
    int IsReintegrating();
    int IsFrozen() { return flags.frozen; }
    void Thaw();
  
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
    vcbdent *Create(VolumeId, const char *);

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
    vcbdent(VolumeId, const char *);
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

void VolDaemon(void) /* used to be member of class vdb (Satya 3/31/95) */;
void TrickleReintegrate(); /* used to be in class vdb (Satya 5/20/95) */


/* Volume Database.  Dictionary for volume entries (volents). */
class vdb {
  friend void VolInit();
  friend void VOLD_Init(void);
  friend void VolDaemon(void);
  friend class cmlent;
  friend class volent;
  friend class vol_iterator;
  friend class repvol_iterator;
  friend class fsobj;
  friend void RecovInit();

    int MagicNumber;

    /* Size parameters. */
    int MaxMLEs;            /* Limit on number of MLE's over _all_ volumes */
    int AllocatedMLEs;

    /* The hash table. */
    rec_ohashtab htab;

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

    void DownEvent(struct in_addr *host);
    void UpEvent(struct in_addr *host);
    void WeakEvent(struct in_addr *host);
    void StrongEvent(struct in_addr *host);

    void AttachFidBindings();
    int WriteDisconnect(unsigned =V_UNSETAGE, unsigned =V_UNSETREINTLIMIT);
    int WriteReconnect();
    void GetCmlStats(cmlstats&, cmlstats&);
    void AutoRequestWBPermit();

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
    unsigned asr_running : 1;                   /* only 1 ASR allowed per volume at a time */
    unsigned has_local_subtree : 1;		/* indicating whehter this volume contains local subtrees */
    /*T*/unsigned reintegratepending : 1;	/* are we waiting for tokens? */
    /*T*/unsigned reintegrating : 1;		/* are we reintegrating now? */
    /*T*/unsigned repair_mode : 1;		/* 0 --> normal, 1 --> repair */
    /*T*/unsigned resolve_me: 1;		/* resolve reintegrated objects */
    /*T*/unsigned weaklyconnected : 1;		/* are we weakly connected? */ 
    /*T*/unsigned writebacking : 1;             /* writeback mode */
    /*T*/unsigned writebackreint :1;            /* is reint due to permit revoke? */
    /*T*/unsigned sync_reintegrate :1;          /* perform reintegration synchronously*/
    /*T*/unsigned sync_reintegrate_done :1;     /* synchronous reintegration done */
    /*T*/unsigned autowriteback :1;             /* auto try to get wb permit */
    /*T*/unsigned staylogging : 1;              /* keep logging after |cml| == 0*/
    unsigned reserved : 13;
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

typedef enum {
    ReplicatedVolume,
    VolumeReplica,
} VenusVolType;

class repvol;

/* local-repair modification */
/* A volume entry. */
class volent {
  friend class ClientModifyLog;
  friend class cmlent;
  friend class vdb;
  friend class vol_iterator;
  friend class repvol_iterator;
  friend class fsdb;
  friend class fsobj;
  friend class fso_vol_iterator;
  friend class userent;
  friend class vproc;
  friend void VolInit();
  friend void Reintegrate(volent *);
  friend long CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
  friend void InitVCBData(VolumeId);

    int MagicNumber;

    rec_olink handle;				/* link for htab */

    /* State information. */
    /*T*/short observer_count;
    /*T*/short mutator_count;
    /*T*/short waiter_count;
    /*T*/short resolver_count;
    /*T*/short shrd_count;		/* for volume pgid locking */
    /*T*/short excl_count;		/* for volume pgid locking */
    /*T*/int excl_pgid;			/* pgid for the exclusive lock holder */
    int reint_id_gen;                   /* reintegration id generator */
    int cur_reint_tid;			/* tid of reintegration in progress, if any */
    int lc_asr;                         /* last/current ASR run for this volume */
    /* Reintegration stuff. */
    struct Lock CML_lock;		/* for checkpoint/mutator synchronization */
    unsigned AgeLimit;			/* min age of log records in SECONDS */
    unsigned ReintLimit;		/* work limit, in MILLESECONDS */
    Unique_t FidUnique;
    RPC2_Unsigned SidUnique;
    /*T*/int OpenAndDirtyCount;
    // The next four are now transient - bnoble
    /*T*/int RecordsCancelled;
    /*T*/int RecordsCommitted;
    /*T*/int RecordsAborted;
    /*T*/int FidsRealloced;
    /*T*/long BytesBackFetched;

    /*T*/PermitStatus VPStatus;   /* do we have a volume permit? */

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
    /*T*/int saved_misses;
    
    /* Local synchronization state. */
    /*T*/char vol_sync;
    /*T*/int refcnt;			/* count of fsobj's plus active threads */
   
    /* Read/Write Sharing Stats */
    long current_disc_time;		/* disconnection time for the current period */
    long current_reco_time;		/* reconnection time for the current period */
    short current_rws_cnt;		/* read/write sharing count */
    short current_disc_read_cnt;	/* disconnected read count */
    rec_dlist rwsq;			/* a queue of read/write sharing stats to be reported */

    /* WriteBack Caching */
    cmlent * reintegrate_until;

    /* Constructors, destructors, and private utility routines. */
    volent(volent&) { abort(); }    		/* not supported! */
    void *operator new(size_t);
    int operator=(volent&) { abort(); return(0); }/* not supported! */
    virtual void Recover() { return; }
    void InitStatsVSR(vsr *);
    void UpdateStatsVSR(vsr *);
    ViceVolumeType VolStatType(void);
    virtual void ResetTransient() { abort(); }

  protected:
    char name[V_MAXVOLNAMELEN];
    VolumeId vid;
    VolFlags flags;
    /*T*/VolumeStateType state;

    /* Fso's. */
    /*T*/olist *fso_list;

    ClientModifyLog CML;

    void operator delete(void *, size_t);
    volent(VolumeId vid, char *name);
    ~volent();
    void ResetVolTransients();

  public:
    /* Volume synchronization. */
    void hold();
    void release();
    int Enter(int, vuid_t);
    void Exit(int, vuid_t);
    void TakeTransition();
    virtual int GetConn(connent **, vuid_t);
    virtual int GetMgrp(mgrpent **, vuid_t, RPC2_CountedBS * =0) { abort(); return(0); }
    virtual void KillMgrpMember(struct in_addr *);
    virtual void GetBandwidth(unsigned long *bw);
    void DownMember(long);
    void UpMember(long);
    void WeakMember();
    void StrongMember();
    void ResetStats() { CML.ResetHighWater(); }
    void Wait();
    void Signal();
    void Lock(VolLockType, int = 0);		
    void UnLock(VolLockType);		  
    int Collate(connent *, int code, int TranslateEINCOMP = 1);
    int Collate_NonMutating(mgrpent *, int);
    int Collate_COP1(mgrpent *, int, ViceVersionVector *);
    int Collate_Reintegrate(mgrpent *, int, ViceVersionVector *);
    int Collate_COP2(mgrpent *, int);

    /* Allocation routines. */
    ViceFid GenerateLocalFid(ViceDataType);
    ViceFid GenerateFakeFid();
    ViceStoreId GenerateStoreId();

    /* User-visible volume status. */
    int GetVolStat(VolumeStatus *, RPC2_BoundedBS *,
		   VolumeStateType *, int *, int *,
		    RPC2_BoundedBS *, RPC2_BoundedBS *, vuid_t);
    int SetVolStat(VolumeStatus *, RPC2_BoundedBS *,
		    RPC2_BoundedBS *, RPC2_BoundedBS *, vuid_t);

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

    /* Repair routines */
    int IsUnderRepair(vuid_t);

    /* ASR routines */
    int EnableASR(vuid_t);
    int DisableASR(vuid_t);
    int IsASRAllowed();
    void lock_asr();
    void unlock_asr();
    int asr_running();
    void asr_id(int);
    int asr_id();

    /* Callback routines */
    virtual int CallBackBreak() { abort(); return(0); }
    virtual void ClearCallBack() { return; }
    virtual void CollateVCB(mgrpent *, RPC2_Integer *, CallBackStatus *) { abort(); }
    virtual int GetVolAttr(vuid_t) { abort(); return(0); }
    virtual int HaveCallBack() { return(0); }
    virtual int HaveStamp() { abort(); return(0); }
    virtual void PackVS(int, RPC2_CountedBS *) { abort(); }
    virtual void SetCallBack() { abort(); }
    virtual int WantCallBack() { abort(); }
    void UseCallBack(int);
    int ValidateFSOs();

    /* write-back routines */
    int EnterWriteback(vuid_t vuid);
    int LeaveWriteback(vuid_t vuid);
    int StopWriteback(ViceFid *fid);
    int SyncCache(ViceFid * fid);
    int IsWritebacking() { return flags.writebacking; }
    void SetPermit();
    int GetPermit(vuid_t vuid);
    int ReturnPermit(vuid_t vuid);
    int HavePermit() { return (VPStatus == PermitSet); }
    void ClearPermit();
    int PermitBreak();

    /* VSR routines. */
    vsr *GetVSR(vuid_t);
    void PutVSR(vsr *);
    void FlushVSRs(int);

    /* Resolution routines */
    virtual void Resolve() { abort(); }
    virtual int ResListCount() { return(0); }
    virtual void ResSubmit(char **, ViceFid *) { abort(); }

    /* COP2 routines. */
    virtual void AddCOP2(ViceStoreId *, ViceVersionVector *) { abort(); }
    virtual void ClearCOP2(RPC2_CountedBS *) { abort(); }
    virtual int COP2(mgrpent *, RPC2_CountedBS *) { abort(); }
    virtual int COP2(mgrpent *, ViceStoreId *, ViceVersionVector *) { abort(); }
    virtual int FlushCOP2(time_t =0) { abort(); }

    /* Allocation routines. */
    virtual int AllocFid(ViceDataType, ViceFid *, RPC2_Unsigned *, vuid_t, int =0) { abort(); return(0); }

    /* Utility routines. */
    virtual void GetHosts(struct in_addr hosts[VSG_MEMBERS]) { abort(); }
    virtual unsigned long GetAVSG(struct in_addr hosts[VSG_MEMBERS])
        { abort(); return 0; }
    int AvsgSize();
    virtual int WeakVSGSize() { abort(); return 0; }
    virtual int IsBackup() { abort(); return 0; }
    virtual int IsReplicated() { abort(); return 0; }
    virtual int IsReadWriteReplica() { abort(); return 0; }
    virtual int IsHostedBy(const struct in_addr *addr) { abort(); return 0; }
    int IsHoarding() { return (state == Hoarding); }
    int IsDisconnected() { return (state == Emulating); }
    int IsWriteDisconnected() { return (state == Logging); }
    int IsWeaklyConnected() { return flags.weaklyconnected; }
    int IsFake() { return FID_VolIsFake(vid); }
    void GetMountPath(char *, int =1);

    /* local-repair addition */
    void IncAbort(int);                         /*U*/
    void CheckTransition();                     /*N*/
    void TranslateCMLFid(ViceFid *, ViceFid *); /*T*/
    void ClearRepairCML();                      /*U*/
    int GetReintId();                           /*U*/
    VolumeId GetVid() { return vid; }           /*N*/
    const char *GetName() { return name; }      /*N*/
    ClientModifyLog *GetCML() { return &CML; }  /*N*/
    int ContainUnrepairedCML();			/*N*/
    int HasLocalSubtree() { return flags.has_local_subtree; }
    void CheckLocalSubtree();			/*U*/
    virtual VolumeId ReplicatedVol() { abort(); return(0); }

    /* advice addition */
    /* void DisconnectedCacheMiss(vproc *, vuid_t, ViceFid *, char *);
     * void TriggerReconnectionQuestionnaire();
     * void NotifyStateChange(); */
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
    virtual void print2(int) { abort(); }

    void ListCache(FILE *, int long_format=1, unsigned int valid=3);

    /* Read/Write Sharing Stats */
    void RwStatUp();
    void RwStatDown();
    rec_dlist *GetRwQueue();
};

/* A volume replica entry. */
class volrep : public volent {
    friend class vdb;

    VolumeId replicated;        /* replicated `parent' volume */
    struct in_addr host;        /* server that has this volume */
    unsigned int rovol : 1;     /* readwrite or readonly */
    
    /* not yet used */
/*T*/struct dllist_head vollist; /* list of volumes on a srvent */

    volrep(VolumeId vid, char *name, struct in_addr *addr, int readonly,
           VolumeId parent=0);
    ~volrep();
    void ResetTransient();
    void Recover() { return; }

  public:
    VolumeId ReplicatedVol() { return(replicated); }

    int GetConn(connent **, vuid_t);
    void KillMgrpMember(struct in_addr *);
    void GetBandwidth(unsigned long *bw);

    /* Utility routines. */
    void Host(struct in_addr *addr) { *addr = host; }
    void GetHosts(struct in_addr addrs[VSG_MEMBERS]);
    unsigned long GetAVSG(struct in_addr addrs[VSG_MEMBERS]);
    int WeakVSGSize() { return 0; }
    int IsBackup() { return rovol; }
    int IsReplicated() { return 0; }
    int IsReadWriteReplica() { return (replicated != 0); }
    int IsHostedBy(const struct in_addr *addr)
        { return (addr->s_addr == host.s_addr); }

    void print2(int);
};

/* A replicated volume entry. */
class repvol : public volent {
    friend class vdb;
    friend class fsobj;
    friend void Resolve(volent *);
    friend int GetMgrp(mgrpent **mpp, repvol *vp, vuid_t vuid);
    friend void ReportVCBEvent(VCBEventType, VolumeId, vcbevent *);

    volrep *vsg[VSG_MEMBERS];      /* underlying volume replicas */
/*T*/struct dllist_head mgrpents;  /* list of mgrpents for this volume */

    /* Preallocated Fids. */
    FidRange FileFids;
    FidRange DirFids;
    FidRange SymlinkFids;

    /* Resolution stuff. */
    /*T*/olist *res_list;

    /* COP2 stuff. */
    /*T*/dlist *cop2_list;

    /* Callback stuff */
    /*T*/CallBackStatus VCBStatus;      /* do we have a volume callback? */
    /*T*/int VCBHits;			/* # references hitting this callback */
    ViceVersionVector VVV;              /* (maximal) volume version vector */

    repvol(VolumeId vid, char *name, volrep *vsg[VSG_MEMBERS]);
    ~repvol();
    void ResetTransient();
    void Recover();

  public:
    int GetMgrp(mgrpent **, vuid_t, RPC2_CountedBS * =0);
    void KillMgrpMember(struct in_addr *);
    void KillUserMgrps(vuid_t);
    void GetBandwidth(unsigned long *bw);

    /* Allocation routines. */
    int AllocFid(ViceDataType, ViceFid *, RPC2_Unsigned *, vuid_t, int);

    /* Utility routines. */
    void GetHosts(struct in_addr hosts[VSG_MEMBERS]);
    unsigned long GetAVSG(struct in_addr hosts[VSG_MEMBERS]);
    int WeakVSGSize();
    int IsBackup() { return 0; }
    int IsReplicated() { return 1; }
    int IsReadWriteReplica() { return 0; }
    int IsHostedBy(const struct in_addr *addr);

    /* Repair routines. */
    int EnableRepair(vuid_t, VolumeId *, vuid_t *, unsigned long *);
    int DisableRepair(vuid_t);
    int Repair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int ConnectedRepair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int DisconnectedRepair(ViceFid *, char *, vuid_t, VolumeId *, int *);
    int LocalRepair(fsobj *, ViceStatus *, char *fname, ViceFid *);

    /* Resolution routines */
    void Resolve();
    void ResSubmit(char **, ViceFid *);
    int ResAwait(char *);
    int RecResolve(connent *, ViceFid *);
    int ResListCount() { return(res_list->count()); }

    /* COP2 routines. */
    int COP2(mgrpent *, RPC2_CountedBS *);
    int COP2(mgrpent *, ViceStoreId *, ViceVersionVector *);
    int FlushCOP2(time_t =0);
    int FlushCOP2(mgrpent *, RPC2_CountedBS *);
    void GetCOP2(RPC2_CountedBS *);
    cop2ent *FindCOP2(ViceStoreId *);
    void AddCOP2(ViceStoreId *, ViceVersionVector *);
    void ClearCOP2(RPC2_CountedBS *);

    /* Callback routines */
    int GetVolAttr(vuid_t);
    void CollateVCB(mgrpent *, RPC2_Integer *, CallBackStatus *);
    void PackVS(int, RPC2_CountedBS *);
    int HaveStamp() { return(VV_Cmp(&VVV, &NullVV) != VV_EQ); }
    int HaveCallBack() { return(VCBStatus == CallBackSet); }
    int CallBackBreak();
    void ClearCallBack();
    void SetCallBack();
    int WantCallBack();

    void print2(int);
};

class vol_iterator : public rec_ohashtab_iterator {
  public:
    vol_iterator(void * =(void *)-1);
    volent *operator()();
};

class repvol_iterator : public rec_ohashtab_iterator {
  public:
    repvol_iterator(void * =(void *)-1);
    repvol *operator()();
};
/* Entries representing pending COP2 events. */
class cop2ent : public dlink {
  friend class repvol;

    ViceStoreId sid;
    ViceVersionVector updateset;
    time_t time;

    void *operator new(size_t);
    cop2ent(ViceStoreId *, ViceVersionVector *);
    cop2ent(cop2ent&);		/* not supported! */
    int operator=(cop2ent&);	/* not supported! */
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
  friend void repvol::Resolve();
  friend void repvol::ResSubmit(char **, ViceFid *);
  friend int repvol::ResAwait(char *);

    ViceFid fid;
    int result;
    int refcnt;

    resent(ViceFid *);
    resent(resent&);		/* not supported! */
    int operator=(resent&);	/* not supported! */
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
    int operator=(vsr&);	/* not supported! */
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
extern char voldaemon_sync;
extern char VCBEnabled;

/*  *****  Functions/Procedures  *****  */

/* venusvol.c */
extern void VolInit();
extern int VOL_HashFN(void *);
extern int GetRootVolume();

/* vol_COP2.c */
const int COP2SIZE = 1024;

/* vol_daemon.c */
extern void VOLD_Init(void);

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
	CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
    }\
}

#define	PRINT_VOLSTATE(state)	((state) == Hoarding ? "Hoarding" :\
				 (state) == Resolving ? "Resolving" :\
				 (state) == Emulating ? "Emulating" :\
				 (state) == Logging ? "Logging":\
				 "???")
#define	PRINT_VOLMODE(mode)	((mode) & VM_OBSERVING ? "Observing" :\
				 (mode) & VM_MUTATING ? "Mutating" :\
				 (mode) & VM_RESOLVING ? "Resolving" :\
				 "???")
#define	PRINT_MLETYPE(op) ((op) == OLDCML_NewStore_OP ? "Store" :\
			    (op) == OLDCML_Truncate_OP ? "Truncate" :\
			    (op) == OLDCML_Utimes_OP ? "Utimes" :\
			    (op) == OLDCML_Chown_OP ? "Chown" :\
			    (op) == OLDCML_Chmod_OP ? "Chmod" :\
			    (op) == OLDCML_Create_OP ? "Create" :\
			    (op) == OLDCML_Remove_OP ? "Remove" :\
			    (op) == OLDCML_Link_OP ? "Link" :\
			    (op) == OLDCML_Rename_OP ? "Rename" :\
			    (op) == OLDCML_MakeDir_OP ? "Mkdir" :\
			    (op) == OLDCML_RemoveDir_OP ? "Rmdir" :\
			    (op) == OLDCML_SymLink_OP ? "Symlink" :\
			    (op) == OLDCML_Repair_OP ? "Repair" :\
			    "???")


#endif	not _VENUS_VOLUME_H_
