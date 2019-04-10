/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
#endif

#include <stdarg.h>
#include <stdio.h>
#include <rpc2/errors.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>

/* For VENUS_CallBackFetch prototype */
#include <callback.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from util */
#include <dlist.h>
#include <rec_dlist.h>
#include <olist.h>
#include <rec_olist.h>
#include <ohash.h>
#include <rec_ohash.h>

/* from vol */
#include <voldefs.h>

/* from venus */
#include "comm.h"
#include "venusrecov.h"
#include "realmdb.h"
#include "venus.private.h"
#include "vsg.h"

/* Forward declarations. */
class ClientModifyLog;
class cmlent;
class cml_iterator;
class cmlstats;

class connent; /* needed because of circular includes! */
class mgrpent;
class vdb;
class volent;
class cop2ent;
class resent;

/* volume pgid locking type */
/* Was: EXCLUSIVE, *SHARED*: name clash on Solaris */
enum VolLockType
{
    EX_VOL_LK,
    SH_VOL_LK
};

/*  *****  Constants  *****  */

#define VDB (rvg->recov_VDB)
const int VDB_MagicNumber     = 6820348;
const int VDB_NBUCKETS        = 512;
const int VOLENT_MagicNumber  = 3614246;
const int MLENT_MagicNumber   = 5214113;
const int MLENTMaxFreeEntries = 32;

const int UNSET_TID = -1;

const unsigned V_UNSETAGE        = (unsigned)-1; /* huge */
const unsigned V_UNSETREINTLIMIT = (unsigned)-1; /* huge */

/* Volume-User modes. */
#define VM_MUTATING 0x1
#define VM_OBSERVING 0x2
#define VM_RESOLVING 0x4
#define VM_NDELAY \
    0x8 /* this is really a flag!  it is not
                                           exclusive with the others!
                                           Indicates the caller doesn't want
                                           to be put to sleep if the volume is
                                           already locked. It's necessary to
                                           keep daemons from getting ``stuck''
                                           on volumes already in use. */

/* Define for 'no ASR running'  */
#define NO_ASR 0

/*  *****  Types  ***** */

class cmlstats {
public:
    int store_count; /* number of store records */
    float store_size; /* size (in bytes) of store records, excluding contents */
    float store_contents_size; /* size (in bytes) of store record contents */
    int other_count; /* number of records other than stores */
    float other_size; /* size (in bytes) of non-stores */

    cmlstats() { clear(); }

    void clear()
    {
        store_count         = 0;
        store_size          = 0.0;
        store_contents_size = 0.0;
        other_count         = 0;
        other_size          = 0.0;
    }

    void operator+=(cmlstats &addend)
    {
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
    friend class repvol;
    friend class reintvol;
    friend class volent; /* ::Enter(int, uid_t); */
    /* ::Exit(int, uid_t) */

    rec_dlist list; /* link to cmlents */
    /*T*/ uid_t owner; /* writer of all entries in Log */
    /*T*/ long entries; /* number of records in the CML */
    /*T*/ long entriesHighWater; /* reset when count == zero */
    /*T*/ long bytes; /* number of bytes used by CML */
    /*T*/ long bytesHighWater; /* reset when size == 0 */
    /*T*/ char cancelFrozenEntries; /* flag indicating whether it's safe to
                                     * auto-thaw and cancel frozen entries */
    cmlstats cancellations;

    /* Size of the Log -- private, because it is only called
     * to reset the tranients bytes and bytesHighWater
     */
    long _bytes();

public:
    ClientModifyLog()
    {
        ResetTransient();
    } /* MUST be called within transaction! */
    ~ClientModifyLog()
    {
        CODA_ASSERT(count() == 0);
    } /* MUST be called within transaction! */
    void ResetTransient();
    void ResetHighWater()
    {
        entriesHighWater = entries;
        bytesHighWater   = bytes;
    }
    void Clear();

    /* Log optimization routines. */
    cmlent *LengthWriter(VenusFid *);
    cmlent *UtimesWriter(VenusFid *);

    /* Reintegration routines. */
    void TranslateFid(VenusFid *, VenusFid *);
    int COP1(char *, int, ViceVersionVector *, int outoforder);
    int COP1_NR(char *buf, int bufsize, ViceVersionVector *, int outoforder);
    void UnLockObjs(int);
    void MarkFailedMLE(int);
    void HandleFailedMLE(void);
    void MarkCommittedMLE(RPC2_Unsigned);
    void CancelPending();
    void ClearPending();
    void ClearToBeRepaired(); /* must not be called within transaction! */
    void CancelStores();

    int GetReintegrateable(int, unsigned long *, int *);
    cmlent *GetFatHead(int);

    /* Call to set/clear flags for whether it's safe to cancel frozen entries */
    void cancelFreezes(char flag) { cancelFrozenEntries = flag; }

    /* Routines for handling inconsistencies and safeguarding against catastrophe! */
    void MakeUsrSpoolDir(char *);
    int CheckPoint(char *);

    void AttachFidBindings();

    long logBytes() { return bytes; }
    long logBytesHighWater() { return bytesHighWater; }
    int count() { return list.count(); }
    long countHighWater() { return entriesHighWater; }
    uid_t Owner() { return owner; }

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int);

    /* local-repair methods */
    void IncThread(int); /*N*/
    void IncPack(char **, int *, int); /*N*/
    int OutOfOrder(int tid); /*N*/
    void IncCommit(ViceVersionVector *, int); /*U*/
    void IncAbort(int = UNSET_TID); /*U*/
    void IncGetStats(cmlstats &, cmlstats &, int = UNSET_TID); /*N*/
    int IncReallocFids(int); /*U*/
    int HaveElements(int); /*N*/
    int DiscardLocalMutation(char *); /*U*/
    void PreserveLocalMutation(char *); /*U*/
    void PreserveAllLocalMutation(char *); /*U*/
    void CheckCMLHead(char *msg); /*U*/
    int ListCML(FILE *); /*N*/
};

/* local-repair addition */
struct CmlFlags {
    unsigned to_be_repaired : 1;
    unsigned unused : 1;
    unsigned frozen : 1; /* do not cancel */
    unsigned cancellation_pending : 1; /* once unfrozen */
    /*T*/ unsigned failed : 1; /* offending record */
    /*T*/ unsigned committed : 1; /* committed at server */
    unsigned prepended : 1;
    unsigned reserved : 25;
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
    friend class repvol;
    friend class reintvol;
    friend class fsobj;
    friend int PathAltered(VenusFid *, char *, ClientModifyLog *, cmlent *);

    ClientModifyLog *log;
    rec_dlink handle;

    ViceStoreId sid; /* transaction identifier */
    Date_t time; /* mtime of operation */
    UserId uid; /* author of operation */
    int tid; /* local-repair addition */
    CmlFlags flags; /* local-repair addition */
    /*T*/ int expansions;

    /* Discriminant and sub-type specific members. */
    int opcode;
    RPC2_String Name, NewName;
    union {
        struct {
            VenusFid Fid;
            RPC2_Unsigned Length;
            /* T */ ViceVersionVector VV;
            RPC2_Unsigned Offset; /* for partial reintegration */
            ViceReintHandle RHandle;
            struct in_addr ReintPH; /* chosen primaryhost & index */
            int ReintPHix; /* for the partial reint. */
        } u_store;
        struct {
            VenusFid Fid;
            RPC2_Unsigned Length;
            /* T */ ViceVersionVector VV;
        } u_truncate;
        struct {
            VenusFid Fid;
            Date_t Date;
            /* T */ ViceVersionVector VV;
        } u_utimes;
        struct {
            VenusFid Fid;
            UserId Owner;
            /* T */ ViceVersionVector VV;
        } u_chown;
        struct {
            VenusFid Fid;
            RPC2_Unsigned Mode;
            /* T */ ViceVersionVector VV;
        } u_chmod;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            RPC2_Unsigned Mode;
            /* T */ ViceVersionVector PVV;
        } u_create;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            int LinkCount;
            /* T */ ViceVersionVector PVV;
            /* T */ ViceVersionVector CVV;
        } u_remove;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            /* T */ ViceVersionVector PVV;
            /* T */ ViceVersionVector CVV;
        } u_link;
        struct {
            VenusFid SPFid;
            VenusFid TPFid;
            VenusFid SFid;
            /* T */ ViceVersionVector SPVV;
            /* T */ ViceVersionVector TPVV;
            /* T */ ViceVersionVector SVV;
        } u_rename;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            RPC2_Unsigned Mode;
            /* T */ ViceVersionVector PVV;
        } u_mkdir;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            /* T */ ViceVersionVector PVV;
            /* T */ ViceVersionVector CVV;
        } u_rmdir;
        struct {
            VenusFid PFid;
            VenusFid CFid;
            RPC2_Unsigned Mode;
            /* T */ ViceVersionVector PVV;
        } u_symlink;
        struct {
            VenusFid Fid;
            RPC2_Unsigned Length;
            Date_t Date;
            UserId Owner;
            RPC2_Unsigned Mode;
            ViceVersionVector OVV;
        } u_repair;
    } u;

    /*T*/ dlist *
        fid_bindings; /* list of (bindings to) fids referenced by this record */

    /*T*/ dlist *pred; /* list of (bindings to) predecessor cmlents */
    /*T*/ dlist *succ; /* list of (bindings to) successor cmlents */

public:
    void *operator new(size_t);
    cmlent(ClientModifyLog *, time_t, uid_t, int,
           int...); /* local-repair modification */
    void ResetTransient();
    ~cmlent();
    void operator delete(void *);

    /* Size of an entry */
    long bytes();

    /* Log optimization routines. */
    int cancel();

    /* Reintegration routines. */
    int realloc();
    void translatefid(VenusFid *, VenusFid *);
    void thread();
    int size();
    void pack(BUFFER *);
    void commit(ViceVersionVector *);
    int cancelstore();
    int Aged();
    unsigned long ReintTime(unsigned long bw);
    unsigned long ReintAmount(unsigned long *reint_time);

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
    int WriteReintegrationHandle(unsigned long *reint_time);
    int CloseReintegrationHandle(char *, int, ViceVersionVector *);

    /* Routines for handling inconsistencies and safeguarding against catastrophe! */
    void abort();
    int checkpoint(FILE *);
    void writeops(FILE *);

    void getfids(VenusFid fid[3]);

    void AttachFidBindings();
    void DetachFidBindings();

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int);

    /* local-repair addition */
    int GetTid() { return tid; } /*N*/
    void SetTid(int); /*U*/
    int ReintReady(); /*U*/
    int ContainLocalFid(); /*N*/
    void TranslateFid(VenusFid *, VenusFid *); /*T*/
    void CheckRepair(char *, int *, int *); /*N*/
    int DoRepair(char *, int); /*U*/
    void GetLocalOpMsg(char *); /*N*/
    void SetRepairFlag(); /*U*/
    void SetRepairMutationFlag(); /*U*/
    int IsToBeRepaired() { return flags.to_be_repaired; } /*N*/
    int IsExpanded() { return expansions; } /*T*/
    void GetVVandFids(ViceVersionVector * [3], VenusFid * [3]); /*N*/
    void GetAllFids(VenusFid * [3]); /*N*/
};

#define CmlIterOrder DlIterOrder
#define CommitOrder DlAscending
#define AbortOrder DlDescending

class cml_iterator {
    ClientModifyLog *log;
    CmlIterOrder order;
    const VenusFid *fidp;
    VenusFid fid;
    cmlent *prelude; /* start iteration after this element */
    dlist_iterator *next;
    rec_dlist_iterator *rec_next;

public:
    cml_iterator(ClientModifyLog &, CmlIterOrder = CommitOrder,
                 const VenusFid * = NULL, cmlent * = 0);
    ~cml_iterator();
    cmlent *operator()();
};

void VolDaemon(void) /* used to be member of class vdb (Satya 3/31/95) */;
void TrickleReintegrate(); /* used to be in class vdb (Satya 5/20/95) */

class fsobj;

/* Volume Database.  Dictionary for volume entries (volents). */
class vdb {
    friend void VolInit(void);
    friend void VOLD_Init(void);
    friend void VolDaemon(void);
    friend class cmlent;
    friend class repvol; /* for hashtab insert/remove */
    friend class volrep; /* for hashtab insert/remove */
    friend class nonrepvol_iterator;
    friend class repvol_iterator;
    friend class volrep_iterator;
    friend class fsobj;
    friend void RecovInit();

    friend class Realm;

    int MagicNumber;

    /* Size parameters. */
    unsigned int MaxMLEs; /* Limit on number of MLE's over _all_ volumes */
    int AllocatedMLEs;

    /* The hash tables for replicated volumes and volume replicas. */
    rec_ohashtab volrep_hash;
    rec_ohashtab repvol_hash;

    /* The mle free list. */
    rec_dlist mlefreelist;

    static const char *ASRPolicyFile;
    static const char *ASRLauncherFile;

    /* Constructors, destructors. */
    void *operator new(size_t);
    vdb();
    void ResetTransient();
    ~vdb() { abort(); }
    void operator delete(void *);

    /* Allocation/Deallocation routines. */
    volent *Create(Realm *realm, VolumeInfo *, const char *);

    /* Daemon functions. */
    void GetDown();
    void FlushCOP2();
    void CheckPoint(unsigned long);
    void CheckLocalSubtree();

public:
    volent *Find(Volid *);
    volent *Find(Realm *, const char *);
    int Get(volent **, Volid *);
    int Get(volent **, Realm *, const char *, fsobj *f);
    void Put(volent **);

    void DownEvent(struct in_addr *host);
    void UpEvent(struct in_addr *host);

    void AttachFidBindings(void);
    int WriteDisconnect(unsigned int age  = V_UNSETAGE,
                        unsigned int time = V_UNSETREINTLIMIT);
    int SyncCache(void);
    void GetCmlStats(cmlstats &, cmlstats &);

    int CallBackBreak(Volid *);
    void TakeTransition(); /* also a daemon function */

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int, int = 0);

    void ListCache(FILE *, int long_format = 1, unsigned int valid = 3);
    int FreeMLECount(void) { return MaxMLEs - AllocatedMLEs; }
    int GetMaxMLEs(void) { return MaxMLEs; }
    const char *GetASRPolicyFile() { return vdb::ASRPolicyFile; }
    const char *GetASRLauncherFile() { return vdb::ASRLauncherFile; }
};

/* A volume is in exactly one of these states. */
typedef enum
{
    Unreachable = 1,
    Reachable,
    Resolving,
} VolumeStateType;

/* We save some space by packing booleans into a bit-vector. */
/* R - replicated volumes */
/* V - volume replicas */
/* T - transients */
struct VolFlags {
    unsigned replicated : 1; /* is this a replicated vol or a vol replica */
    /* T*/ unsigned transition_pending : 1;
    /* T*/ unsigned demotion_pending : 1;
    /*R */ unsigned unused1 : 1;
    /*RT*/ unsigned allow_asrinvocation : 1; /* asr's allowed in this volume */
    /*RT*/ unsigned asr_running : 1; /* only 1 ASR allowed per volume at a time */
    /*R */ unsigned
        has_local_subtree : 1; /* indicating whether this volume contains local subtrees */
    /*RT*/ unsigned
        unused2 : 1; /* used to be 'reintegration waiting for tokens */
    /*RT*/ unsigned reintegrating : 1; /* are we reintegrating now? */
    /*RT*/ unsigned repair_mode : 1; /* 0 --> normal, 1 --> repair */
    /*RT*/ unsigned resolve_me : 1; /* resolve reintegrated objects */
    /*  */ unsigned unused3 : 3;
    /*RT*/ unsigned sync_reintegrate : 1; /* perform reintegration synchronously*/
    /*  */ unsigned unused4 : 2;
    /*V */ unsigned readonly : 1; /* is this a readonly (backup) volume replica */
    /*RT*/ unsigned enable_asrinvocation : 1; /* asr's enabled in this volume */
    /*VT*/ unsigned available : 1; /* is the server for this volume online? */
    unsigned unused5 : 10;
    unsigned reint_conflict : 1; /* set when the head of the CML is marked
				       as in conflict, should not be used as
				       authorative information */
    unsigned unauthenticated : 1; /* set when reintegration fails due to lack
				       of tokens, should not be used as
				       authorative information */
};

/* Descriptor for a range of pre-allocated fids. */
struct FidRange : public ViceFidRange {
    unsigned long Unused; /* was unused AllocHost */

    FidRange()
    {
        Vnode        = 0;
        this->Unique = 0;
        Stride       = 0;
        Count        = 0;
    }
};

typedef enum
{
    ReplicatedVolume,
    VolumeReplica,
} VenusVolType;

class repvol;

/* local-repair modification */
/* A volume entry. */
class volent {
    friend class fsdb;
    friend class fsobj;
    friend class userent;
    friend class vdb;
    friend class volent_iterator;
    friend class vproc; /* End_VFS(int *); wants vol->realm->GetUser() */

    int MagicNumber;

    /* State information. */
    /*T*/ short waiter_count;
    /*T*/ short excl_count; /* for volume pgid locking */
    /*T*/ int excl_pgid; /* pgid for the exclusive lock holder */

    /* Local synchronization state. */
    /*T*/ char vol_sync;
    /*T*/ int refcnt; /* count of fsobj's plus active threads */

    /* Constructors, destructors, and private utility routines. */
    volent(volent &) { abort(); } /* not supported! */
    void *operator new(size_t);
    int operator=(volent &)
    {
        abort();
        return (0);
    } /* not supported! */

protected:
    char *name;
    VolumeId vid;
    Realm *realm;
    VolFlags flags;
    /*T*/ VolumeStateType state;

    Unique_t FidUnique;

    rec_olink handle; /* link for repvol_hash/volrep_hash */

    /* Fso's. */
    /*T*/ struct dllist_head fso_list;

    /* State information. */
    /*T*/ short mutator_count;
    /*T*/ short observer_count;
    /*T*/ short resolver_count;
    /*T*/ short shrd_count; /* for volume pgid locking */
    /*T*/ int pgid; /* pgid of ASRLauncher and children (0 if none) */

    void operator delete(void *);
    volent(Realm *r, VolumeId vid, const char *name);
    ~volent();
    void ResetVolTransients();
    ViceVolumeType VolStatType(void);

public:
    /* Volume synchronization. */
    void hold();
    void release();
    int Enter(int, uid_t);
    void Exit(int, uid_t);
    void TakeTransition();
    int TransitionPending() { return flags.transition_pending; }
    void Wait();
    void Signal();
    void Lock(VolLockType, int = 0);
    void UnLock(VolLockType);
    int Collate(connent *, int code, int TranslateEINCOMP = 1);

    /* User-visible volume status. */
    int GetVolStat(VolumeStatus *, RPC2_BoundedBS *, VolumeStateType *,
                   unsigned int *age, unsigned int *hogtime, int *conflict,
                   int *cml_size, uint64_t *cml_bytes, RPC2_BoundedBS *,
                   RPC2_BoundedBS *, uid_t, int local_only);
    int SetVolStat(VolumeStatus *, RPC2_BoundedBS *, RPC2_BoundedBS *,
                   RPC2_BoundedBS *, uid_t);

    /* Utility routines. */
    void GetHosts(struct in_addr hosts[VSG_MEMBERS]);
    void GetVids(VolumeId out[VSG_MEMBERS]);
    int AVSGsize();
    int IsBackup() { return (!flags.replicated && flags.readonly); }
    int IsReplicated() { return flags.replicated; }
    int IsReadWriteReplica();
    int IsNonReplicated();
    int IsReadWrite() { return (IsReplicated() || IsNonReplicated()); }
    int IsUnreachable() { return (state == Unreachable); }
    int IsReachable() { return (state == Reachable); }
    int IsResolving() { return (state == Resolving); }
    int IsLocalRealm() { return (realm == LocalRealm); }
    void GetMountPath(char *, int = 1);
    void GetBandwidth(unsigned long *bw);

    /* local-repair addition */
    VenusFid GenerateFakeFid();
    RealmId GetRealmId() { return realm->Id(); } /*N*/
    VolumeId GetVolumeId() { return vid; } /*N*/
    const char *GetName() { return name; } /*N*/

    fsobj *NewFakeDirObj(const char *comp);
    fsobj *NewFakeMountLinkObj(VenusFid *fid, const char *comp);
    int IsRepairVol(void)
    {
        return (realm->Id() == LocalRealm->Id() && vid == FakeRepairVolumeId);
    }

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int);

    void ListCache(FILE *, int long_format = 1, unsigned int valid = 3);
};

class reintvol : public volent {
    friend class ClientModifyLog;
    friend class fsobj;
    friend class volent;
    friend class cmlent;
    friend class vdb;
    friend long VENUS_CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
    friend void VolInit(void);

private:
protected:
    static bool LogOpts;
    static bool VCBEnabled;
    /* Preallocated Fids. */
    FidRange FileFids;
    FidRange DirFids;
    FidRange SymlinkFids;

    /* Reintegration stuff. */
    ClientModifyLog CML;
    struct Lock CML_lock; /* for synchronization */
    int reint_id_gen; /* reintegration id generator */
    /*T*/ int cur_reint_tid; /* tid of reintegration in progress */

    unsigned int ReintLimit; /* work limit, in MILLISECONDS */
    unsigned int AgeLimit; /* min age of log records in SECONDS */

    /*T*/ int RecordsCancelled;
    /*T*/ int RecordsCommitted;
    /*T*/ int RecordsAborted;
    /*T*/ int FidsRealloced;
    /*T*/ long BytesBackFetched;
    /*?*/ cmlent *reintegrate_done; /* WriteBack Caching */

    /* Callback stuff */
    /*T*/ CallBackStatus VCBStatus; /* do we have a volume callback? */
    /*T*/ int VCBHits; /* # references hitting this callback */
    ViceVersionVector VVV; /* (maximal) volume version vector */

public:
    reintvol(Realm *r, VolumeId volid, const char *volname);

    long LengthOfCML() { return (CML.entries); }
    void ResetStats() { CML.ResetHighWater(); }

    /* local-repair */
    void ClearRepairCML(); /*U*/
    ClientModifyLog *GetCML() { return &CML; } /*N*/
    int ContainUnrepairedCML(); /*N*/
    int IsSync(void) { return (flags.sync_reintegrate || ReintLimit == 0); }
    int WriteDisconnect(unsigned int age  = V_UNSETAGE,
                        unsigned int time = V_UNSETREINTLIMIT);

    /* Reintegration routines. */
    void Reintegrate();
    int IncReintegrate(int);
    int PartialReintegrate(int, unsigned long *reint_time);
    int IsReintegrating() { return flags.reintegrating; }
    int ReadyToReintegrate();
    int GetReintId(); /*U*/
    void CheckTransition(); /*N*/
    void IncAbort(int); /*U*/
    int SyncCache(VenusFid *fid = NULL);

    void ReportVolState(void);

    /* ASR routines */
    int AllowASR(uid_t);
    int DisallowASR(uid_t);
    void EnableASR(uid_t);
    int DisableASR(uid_t);
    int IsASRAllowed() { return flags.allow_asrinvocation; }
    int IsASREnabled() { return flags.enable_asrinvocation; }
    void lock_asr();
    void unlock_asr();
    int asr_running() { return flags.asr_running; }
    void asr_pgid(pid_t new_pgid);
    pid_t asr_pgid() { return pgid; }

    int AllocFid(ViceDataType, VenusFid *, uid_t, int = 0);

    VenusFid GenerateLocalFid(ViceDataType);

    int GetConn(connent **c, uid_t uid, mgrpent **m, int *ph_ix,
                struct in_addr *phost);

    /* local-repair modifications to the following methods */
    /* Modlog routines. */
    int LogStore(time_t, uid_t, VenusFid *, RPC2_Unsigned, int prepend);
    int LogSetAttr(time_t, uid_t, VenusFid *, RPC2_Unsigned, Date_t, UserId,
                   RPC2_Unsigned, int prepend);
    int LogTruncate(time_t, uid_t, VenusFid *, RPC2_Unsigned, int prepend);
    int LogUtimes(time_t, uid_t, VenusFid *, Date_t, int prepend);
    int LogChown(time_t, uid_t, VenusFid *, UserId, int prepend);
    int LogChmod(time_t, uid_t, VenusFid *, RPC2_Unsigned, int prepend);
    int LogCreate(time_t, uid_t, VenusFid *, char *, VenusFid *, RPC2_Unsigned,
                  int prepend);
    int LogRemove(time_t, uid_t, VenusFid *, char *, const VenusFid *, int,
                  int prepend);
    int LogLink(time_t, uid_t, VenusFid *, char *, VenusFid *, int prepend);
    int LogRename(time_t, uid_t, VenusFid *, char *, VenusFid *, char *,
                  VenusFid *, const VenusFid *, int, int prepend);
    int LogMkdir(time_t, uid_t, VenusFid *, char *, VenusFid *, RPC2_Unsigned,
                 int prepend);
    int LogRmdir(time_t, uid_t, VenusFid *, char *, const VenusFid *,
                 int prepend);
    int LogSymlink(time_t, uid_t, VenusFid *, char *, char *, VenusFid *,
                   RPC2_Unsigned, int prepend);
    int LogRepair(time_t, uid_t, VenusFid *, RPC2_Unsigned, Date_t, UserId,
                  RPC2_Unsigned, int prepend);
    /* local-repair modifications to the above methods */

    int CheckPointMLEs(uid_t, char *);
    int LastMLETime(unsigned long *);
    int PurgeMLEs(uid_t);

    /* CML routines */
    void ListCML(FILE *fp);
    void PreserveAllLocalMutation(char *msg);
    void PreserveLocalMutation(char *msg);
    void DiscardAllLocalMutation(char *msg);
    void DiscardLocalMutation(char *msg);

    /* Callbacks routines */
    int HaveCallBack() { return (VCBStatus == CallBackSet); }
    int CallBackBreak();
    void ClearCallBack();
    void SetCallBack();
    int WantCallBack();
    int ValidateFSOs();
    int GetVolAttr(uid_t);
    void UpdateVCBInfo(RPC2_Integer VS, CallBackStatus CBStatus);
    void PackVS(int, RPC2_CountedBS *);
    int HaveStamp() { return (VV_Cmp(&VVV, &NullVV) != VV_EQ); }
};

class srvent;

/* A volume replica entry. */
class volrep : public reintvol {
    friend class vdb;
    friend class volent;
    friend void VolInit(void);

    VolumeId replicated; /* replicated `parent' volume */
    struct in_addr host; /* server that has this volume */

    /*T*/ srvent *volserver; /* srvent of the server hosting this volume */

    /* not yet used */
    /*T*/ struct dllist_head vollist; /* links volumes to a srvent */

    volrep(Realm *r, VolumeId vid, const char *name, struct in_addr *addr,
           int readonly, VolumeId parent = 0);
    ~volrep();
    void ResetTransient();

public:
#ifdef VENUSDEBUG
    static unsigned int allocs;
    static unsigned int deallocs;
#endif

    VolumeId ReplicatedVol() { return replicated; }
    int IsReadWriteReplica() { return (ReplicatedVol() != 0); }

    int GetConn(connent **, uid_t);
    void GetBandwidth(unsigned long *bw);

    void DownMember(struct in_addr *host);
    void UpMember(void);

    /* Utility routines. */
    void Host(struct in_addr *addr) { *addr = host; }
    int IsAvailable() { return flags.available; }
    int IsHostedBy(const struct in_addr *addr)
    {
        return (addr->s_addr == host.s_addr);
    }

    void print_volrep(int);
};

/* A replicated volume entry. */
class repvol : public reintvol {
    friend class cmlent;
    friend class fsobj;
    friend class reintvol;
    friend class vdb;
    friend class volent; /* CML_Lock */
    friend long VENUS_CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
    friend void Resolve(volent *);
    friend void Reintegrate(reintvol *);
    friend void VolInit(void);

    volrep *volreps[VSG_MEMBERS]; /* underlying volume replicas */
    volrep *ro_replica; /* R/O staging replica for this volume */
    vsgent *vsg;

    /* Resolution stuff. */
    /*T*/ olist *res_list;

    /* COP2 stuff. */
    /*T*/ dlist *cop2_list;

    repvol(Realm *r, VolumeId vid, const char *name, volrep *reps[VSG_MEMBERS]);
    ~repvol();
    void ResetTransient();

public:
#ifdef VENUSDEBUG
    static unsigned int allocs;
    static unsigned int deallocs;
#endif

    int GetMgrp(mgrpent **, uid_t, RPC2_CountedBS * = 0);
    void GetBandwidth(unsigned long *bw);

    void DownMember(struct in_addr *host);
    void UpMember(void);

    int Collate_NonMutating(mgrpent *, int);
    int Collate_COP1(mgrpent *, int, ViceVersionVector *);
    int Collate_Reintegrate(mgrpent *, int, ViceVersionVector *);
    int Collate_COP2(mgrpent *, int);

    /* Allocation routines. */
    int AllocFid(ViceDataType, VenusFid *, uid_t, int = 0);

    /* Utility routines. */
    void GetHosts(struct in_addr hosts[VSG_MEMBERS]);
    void GetVids(VolumeId out[VSG_MEMBERS]);
    int AVSGsize();
    int IsHostedBy(const struct in_addr *addr); /* XXX not called? */
    void SetStagingServer(struct in_addr *srvr);
    void Reconfigure(void);

    /* Allocation routines. */
    void RestoreObj(VenusFid *);

    /* Repair routines. */
    int Repair(VenusFid *, char *, uid_t, VolumeId *, int *);
    int ConnectedRepair(VenusFid *, char *, uid_t, VolumeId *, int *);
    int DisconnectedRepair(VenusFid *, char *, uid_t, VolumeId *, int *);
    int LocalRepair(fsobj *, ViceStatus *, char *fname, VenusFid *);

    /* Resolution routines */
    void Resolve();
    void ResSubmit(char **, VenusFid *, resent **requeue = NULL);
    int ResAwait(char *);
    int RecResolve(connent *, VenusFid *);
    int ResListCount() { return (res_list->count()); }

    /* COP2 routines. */
    int COP2(mgrpent *, RPC2_CountedBS *);
    int COP2(mgrpent *, ViceStoreId *, ViceVersionVector *, int donotpiggy = 0);
    int FlushCOP2(time_t = 0);
    int FlushCOP2(mgrpent *, RPC2_CountedBS *);
    void GetCOP2(RPC2_CountedBS *);
    cop2ent *FindCOP2(ViceStoreId *);
    void AddCOP2(ViceStoreId *, ViceVersionVector *);
    void ClearCOP2(RPC2_CountedBS *);
    void ClearCOP2(void);

    void CollateVCB(mgrpent *, RPC2_Integer *, CallBackStatus *);

    void print_repvol(int);
};

class volent_iterator : public rec_ohashtab_iterator {
public:
    volent_iterator(rec_ohashtab &hashtab, Volid *key);
    ~volent_iterator();
    volent *operator()();
};

class repvol_iterator : public volent_iterator {
public:
    repvol_iterator(Volid * = (Volid *)-1);
    repvol *operator()();
};

class nonrepvol_iterator : public volent_iterator {
public:
    nonrepvol_iterator(Volid * = (Volid *)-1);
    reintvol *operator()();
};

class volrep_iterator : public volent_iterator {
public:
    volrep_iterator(Volid * = (Volid *)-1);
    volrep *operator()();
};

class reintvol_iterator {
    nonrepvol_iterator non_rep_iterator;
    repvol_iterator rep_iterator;

public:
    reintvol_iterator(Volid *key = (Volid *)-1)
        : non_rep_iterator(key)
        , rep_iterator(key)
    {
    }
    reintvol *operator()();
};

/* Entries representing pending COP2 events. */
class cop2ent : public dlink {
    friend class repvol;

    ViceStoreId sid;
    ViceVersionVector updateset;
    time_t time;

    void *operator new(size_t);
    cop2ent(ViceStoreId *, ViceVersionVector *);
    cop2ent(cop2ent &); /* not supported! */
    int operator=(cop2ent &); /* not supported! */
    ~cop2ent();
    void operator delete(void *);

public:
#ifdef VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif /* VENUSDEBUG */

    void print();
    void print(FILE *);
    void print(int);
};

/* Entries representing fids that need to be resolved. */
class resent : public olink {
    friend void repvol::Resolve();
    friend void repvol::ResSubmit(char **, VenusFid *, resent **requeue);
    friend int repvol::ResAwait(char *);

    VenusFid fid;
    int result;
    int refcnt;
    int requeues;

    resent(VenusFid *);
    resent(resent &); /* not supported! */
    int operator=(resent &); /* not supported! */
    ~resent();

    void HandleResult(int);

public:
#ifdef VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif /* VENUSDEBUG */

    void print();
    void print(FILE *);
    void print(int);
};

/*  *****  Variables  *****  */
extern int vcbbreaks;
extern char voldaemon_sync;

/*  *****  Functions/Procedures  *****  */

/* venusvol.c */
void VolInit(void);
void VolInitPost(void);
int VOL_HashFN(void *);

/* vol_COP2.c */
const unsigned int COP2SIZE = 1024;

/* vol_daemon.c */
extern void VOLD_Init(void);

/* vol_reintegrate.c */
void Reintegrate(reintvol *);

/* vol_resolve.c */
extern void Resolve(volent *);

/* vol_cml.c */
extern void RecoverPathName(char *, VenusFid *, ClientModifyLog *, cmlent *);
extern int PathAltered(VenusFid *, char *, ClientModifyLog *, cmlent *);

#define VOL_ASSERT(v, ex)                                               \
    {                                                                   \
        if (!(ex)) {                                                    \
            (v)->print(GetLogFile());                                   \
            CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, \
                  __LINE__);                                            \
        }                                                               \
    }

#define PRINT_VOLSTATE(state)                     \
    ((state) == Resolving ?                       \
         "Resolving" :                            \
         (state) == Unreachable ? "Unreachable" : \
                                  (state) == Reachable ? "Reachable" : "???")
#define PRINT_VOLMODE(mode)                \
    ((mode)&VM_OBSERVING ?                 \
         "Observing" :                     \
         (mode)&VM_MUTATING ? "Mutating" : \
                              (mode)&VM_RESOLVING ? "Resolving" : "???")
#define PRINT_MLETYPE(op)                     \
    ((op) == CML_Store_OP ?                   \
         "Store" :                            \
         (op) == CML_Truncate_OP ?            \
         "Truncate" :                         \
         (op) == CML_Utimes_OP ?              \
         "Utimes" :                           \
         (op) == CML_Chown_OP ?               \
         "Chown" :                            \
         (op) == CML_Chmod_OP ?               \
         "Chmod" :                            \
         (op) == CML_Create_OP ?              \
         "Create" :                           \
         (op) == CML_Remove_OP ?              \
         "Remove" :                           \
         (op) == CML_Link_OP ?                \
         "Link" :                             \
         (op) == CML_Rename_OP ?              \
         "Rename" :                           \
         (op) == CML_MakeDir_OP ?             \
         "Mkdir" :                            \
         (op) == CML_RemoveDir_OP ?           \
         "Rmdir" :                            \
         (op) == CML_SymLink_OP ? "Symlink" : \
                                  (op) == CML_Repair_OP ? "Repair" : "???")

#define FAKEROOTFID(fid) \
    ((fid).Vnode == 0xffffffff) /* && ((fid).Unique == 0x80000)) */

#endif /* _VENUS_VOLUME_H_ */