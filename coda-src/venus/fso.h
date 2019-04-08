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
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    Specification of the Venus File-System Object (fso) abstraction.
 *
 *    ToDo:
 *
 */

#ifndef _VENUS_FSO_H_
#define _VENUS_FSO_H_ 1

/* Forward declarations. */
class fsdb;
class fsobj;
class fso_iterator;
class connent;
class cmlent; /* we have compiler troubles if volume.h is included! */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/uio.h>

#include <rpc2/rpc2.h>

#include <codadir.h>

extern int global_kernfd;
#ifdef __cplusplus
}
#endif

/* interfaces */
#include <user.h>
#include <vice.h>

/* from util */
#include <bstree.h>
#include <rec_bstree.h>
#include <dlist.h>
#include <rec_dlist.h>
#include <ohash.h>
#include <rec_ohash.h>
#include <olist.h>
#include <rec_olist.h>

/* from lka */
#include <lka.h>

/* from venus */
#include "binding.h"
#include "comm.h"
#include "hdb.h"
#include "mariner.h"
#include "realmdb.h"
#include "venusrecov.h"
#include "vproc.h"
#include "fso_cachefile.h"
#include "venus.private.h"

/* from coda include again, must appear AFTER venus.private.h */

/*  *****  Constants  ***** */

#define FSDB (rvg->recov_FSDB)
const int FSDB_MagicNumber = 3620289;
const int FSDB_NBUCKETS    = 2048;
const int FSO_MagicNumber  = 2687694;

/* Priorities. */
const int FSO_MAX_SPRI = H_MAX_PRI;
const int FSO_MAX_MPRI = H_MAX_PRI;

const int CPSIZE = 8;

/*  *****  Types  ***** */
/* Cache stuff was removed here to move to venus.private.h  5/14/92 */

void FSODaemon(void); /* used to be member of class fsdb (Satya 3/31/95) */

#define SERVER_SERVER 1
#define LOCAL_GLOBAL 2
#define MIXED_CONFLICT 3

#define FILE_CONFLICT 1
#define DIRECTORY_CONFLICT 2

/* The (cached) file-system database. */
class fsdb {
    friend void FSOInit();
    friend void FSOD_Init();
    friend void FSODaemon();
    friend class fsobj;
    friend class fso_iterator;
    friend class hdb;
    friend class vproc;
    friend void RecovInit();
    friend class volent;
    friend class repvol;

    int MagicNumber;
    int DataVersion;
    int damnitagain;

    /* Size parameters. */
    unsigned int MaxFiles;
    uint64_t WholeFileCachingMaxSize;
    uint64_t WholeFileCachingMinSize;
    uint32_t WholeFileCachingMaxStall;

    /* "files" is kept as count member of htab */
    int FreeFileMargin;
    /*T*/ uint64_t MaxBlocks;
    /*T*/ uint64_t blocks;
    /*T*/ int FreeBlockMargin;

    /* Priority parameters. */
    int swt; /* short-term component weight */
    int mwt; /* medium-term component weight */
    int ssf; /* short-term scaling factor */
    int maxpri; /* maximum priority */
    int stdpri; /* standard priority (for VFS operations) */
    int marginpri; /* margin priority (for GetDown) */

    /* The hash table. */
    rec_ohashtab htab;

    /* The free list. */
    rec_olist freelist;

    /* The priority queue. */
    /*T*/ bstree *prioq;
    long *LastRef;
    /*T*/ long RefCounter; /* used to compute short-term priority */

    /* The delete queue.  Objects are sent here to be garbage collected. */
    /*T*/ dlist *delq;

    /* Queue of files open for write. */
    /*T*/ olist *owriteq;

    /* Statistics. */
    /*T*/ CacheStats DirAttrStats;
    /*T*/ CacheStats DirDataStats;
    /*T*/ CacheStats FileAttrStats;
    /*T*/ CacheStats FileDataStats;
    int VolumeLevelMiss; /* Counter to pass to data collection; Stored in RVM */
    /*T*/ int Recomputes; /* total priority recomputations */
    /*T*/ int Reorders; /* number of resulting prioq reorders */

    /* Synchronization stuff for matriculating objects. */
    /*T*/ char matriculation_sync;
    /*T*/ int matriculation_count;

    /* Constructors, destructors. */
    void *operator new(size_t);
    void operator delete(void *);

    fsdb();
    void ResetTransient();
    ~fsdb() { abort(); }

    /* Allocation/Deallocation routines. */
    fsobj *Create(VenusFid *, int, const char *comp, VenusFid *parent);
    int FreeFsoCount();
    int AllocFso(int, fsobj **);
    int GrabFreeFso(int, fsobj **);
    void ReclaimFsos(int, int);
    void FreeFso(fsobj *);
    int FreeBlockCount();
    int DirtyBlockCount();
    int AllocBlocks(int, int);
    int GrabFreeBlocks(int, int);
    void ReclaimBlocks(int, int);
    void FreeBlocks(int);
    void ChangeDiskUsage(int);

    /* Daemon. */
    void RecomputePriorities(int = 0);
    void GarbageCollect();
    void GetDown();
    void FlushRefVec();

public:
    uint64_t GetMaxBlocks() { return MaxBlocks; }
    unsigned int GetMaxFiles() { return MaxFiles; }
    uint64_t GetWholeFileMaxSize() { return WholeFileCachingMaxSize; }
    uint64_t GetWholeFileMinSize() { return WholeFileCachingMinSize; }
    uint64_t GetWholeFileMaxStall() { return WholeFileCachingMaxStall; }

    fsobj *Find(const VenusFid *);
    /* rcode arg added for local repair */
    int Get(fsobj **fso, VenusFid *fid, uid_t uid, int rights,
            const char *comp = NULL, VenusFid *parent = NULL, int *rcode = NULL,
            int GetInconsistent = 0);
    void Put(fsobj **);
    void Flush();
    void Flush(Volid *);
    int TranslateFid(VenusFid *, VenusFid *);
    int CallBackBreak(const VenusFid *);
    void ResetUser(uid_t);
    void ClearPriorities();
    void InvalidateMtPts();
    int MakePri(int spri, int mpri) { return (swt * spri + mwt * mpri); }
    int MaxPri() { return (maxpri); }
    int StdPri() { return (stdpri); }
    int MarginPri() { return (marginpri); }

    void GetStats(int *fa, int *fo, int *ba, int *bo)
    {
        *fa = MaxFiles;
        *fo = htab.count();
        *ba = MaxBlocks;
        *bo = blocks;
    }

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int, int = 0);
};

enum FsoState
{
    FsoRunt,
    FsoNormal,
    FsoDying
};

/* Condensed version of ViceStatus. */
struct VenusStat {
    ViceDataType VnodeType;
    unsigned char LinkCount;
    unsigned long Length;
    unsigned long DataVersion;
    ViceVersionVector VV;
    Date_t Date;
    uid_t Author;
    uid_t Owner;
    unsigned short Mode;
};

/* Condensed version of VenusStat. */
/* needed to restore objects after store cancellation */
struct MiniVenusStat {
    unsigned long Length;
    Date_t Date;
};

/* Access Control Rights */
struct AcRights {
    uid_t uid;
    unsigned char rights;
    unsigned inuse : 1;
    /*T*/ unsigned valid : 1;
};

struct FsoFlags {
    /*T*/ unsigned random : 16; /* help balance binary-search trees */
    unsigned fake : 1; /* is this object fake? (c.f. repair) */
    unsigned owrite : 1; /* file open for write? */
    unsigned dirty : 1; /* is this object dirty? */
    unsigned local : 1; /* local fake fid */
    /*T*/ unsigned ckmtpt : 1; /* mount point needs checked? */
    /*T*/ unsigned fetching : 1; /* fetch in progress? */
    unsigned expanded : 1; /* are we an expanded object */
    unsigned modified : 1; /* modified for expansion? */
    unsigned vastro : 1; /* is the file vastro?  */
    unsigned padding : 7;
};

enum MountStatus
{
    NORMAL,
    MOUNTPOINT,
    ROOT
};

struct VenusDirData {
    /* Vice format directory in VM/RVM. */
    struct DirHandle dh;
    /* Unix format directory in UFS. */
    /*T*/ unsigned udcfvalid : 1;
    /*T*/ CacheFile *udcf;
    /*T*/ int padding;
};

union VenusData {
    int havedata; /* generic test for null pointer (pretty gross, eh) */
    CacheFile *file; /* VnodeType == File */
    VenusDirData *dir; /* VnodeType == Directory */
    char *symlink; /* VnodeType == SymbolicLink */
};

/* local-repair modification */
typedef enum
{
    FROMHEAP,
    FROMFREELIST
} fso_alloc_t; /* placement argument to operator new() */

typedef enum
{
    HF_Fetch,
    HF_DontFetch
} HoardFetchState;
typedef enum
{
    HA_Ask,
    HA_DontAsk
} HoardAskState;

class ClientModifyLog;
class fsobj {
    friend void FSOInit();
    friend int FSO_PriorityFN(bsnode *, bsnode *);
    friend class fsdb;
    friend class fso_iterator;
    friend long VENUS_CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
    friend class vproc;
    friend class namectxt;
    friend class volent;
    friend class repvol;
    friend class reintvol;
    friend class ClientModifyLog;
    friend class cmlent;
    friend class cml_iterator;
    friend class resent;
    friend class mgrpent;
    friend class hdb;
    friend class Realm; /* ~Realm */
    friend class plan9server;
    friend void RecoverPathName(char *, VenusFid *, ClientModifyLog *,
                                cmlent *);

    int MagicNumber;

    /*T*/ CacheChunkList
        active_segments; /**< List of active cache file segments */

    /* Keys. */
    VenusFid fid; /* unique id for object */
    char *comp; /* most recently used component */
    /*T*/ volent *vol; /* pointer to object's volume */

    /* Links for various lists. */
    rec_olink primary_handle; /* link for {fstab, free-list} */
    /*T*/ struct dllist_head vol_handle; /* link for volent fso_list */
    /*T*/ bsnode prio_handle; /* link for priority queue */
    /*T*/ dlink del_handle; /* link for delete queue */
    /*T*/ olink owrite_handle; /* link for owrite queue */

    /* General status. */
    enum FsoState state; /* {FsoRunt, FsoNormal, FsoDying} */
    VenusStat stat;
    /*T*/ uint64_t GotThisDataStart; /* used during fetch to keep track of
                                        where we are. Signalling the start
                                        point of the current fetch segment */
    /*T*/ uint64_t GotThisDataEnd; /* used during fetch to keep track of where
                                      we are. Signalling the end of the current
                                      fetch segment */
    /*T*/ int RcRights; /* replica control rights */
    AcRights AnyUser; /* access control rights: any user */
    AcRights SpecificUser[CPSIZE]; /* access control rights: specific users */
    FsoFlags flags; /* some of these are transient */

    /* if non-zero, the saved SHA of the file; used by lookaside code; */
    unsigned char VenusSHA[SHA_DIGEST_LENGTH];

    /* Mount state. */
    MountStatus mvstat; /* { NORMAL, MOUNTPOINT, ROOT } */
    /*T*/ union {
        /* mvstat == NORMAL */
        fsobj *root; /* mvstat == MOUNTPOINT */
        fsobj *mtpoint; /* mvstat == ROOT */
    } u;

    /* Child/Parent linkage. */
    VenusFid pfid;
    /*T*/ fsobj *pfso; /* back pointer from child to parent */
    /*T*/ dlist *children; /* for directories; list of cached children */
    /*T*/ dlink child_link; /* link used for that list */

    /* Priority state. */
    /*T*/ int priority; /* f(spri, mpri) */
    /*T*/ int HoardPri; /* max of priorities of binders */
    /*T*/ uid_t HoardVuid; /* uid of that entry */
    /*T*/ dlist *
        hdb_bindings; /* list of (bindings to) hdbents referencing this object */
    /*T*/ VnodeId LocalFid_Vnode; /* Values of the vnode and */
    /*T*/ Unique_t LocalFid_Unique; /* uniquifier when this object
						   had a local fid */

    /* MLE linkage. */
    /* T */ dlist *
        mle_bindings; /* list of (bindings to) mlents referencing this object */
    MiniVenusStat CleanStat; /* last status before becoming dirty */
    /* T */ ViceStoreId tSid; /* temporary for serializing MLEs */
    /*T*/ CacheFile *shadow; /* shadow copy, temporary during reintegration */

    /* Data contents. */
    VenusData data;

    /* Statically allocated cache-file stuff. */
    /* Eventually we might make cache-file allocation dynamic, in which case there would be */
    /* various of these pointed to by the VenusData descriptors! */
    int ix;
    CacheFile cf;

    /* Local synchronization state. */
    /*T*/ char fso_sync; /* for waiting/signalling */
    /*T*/ short readers; /* entry readers, not object readers */
    /*T*/ short writers; /* entry writers, not object writers */
    /*T*/ short openers; /* object openers */
    /*T*/ short Writers; /* object writers */
    /*T*/ short Execers; /* object execers (we don't know this under VFS!) */
    /*T*/ short refcnt; /* readers + writers + openers + temporary_refs */

    // for asr invocation
    /*T*/ long lastresolved; // time when object was last resolved

    /* Constructors, destructors. */
    void *operator new(size_t, fso_alloc_t,
                       int); /* for allocation from freelist */
    void *operator new(size_t, fso_alloc_t); /* for allocation from heap */
    void *operator new(size_t); /* dummy to pacify g++ */
    void operator delete(void *);
    fsobj(int);
    fsobj(VenusFid *, const char *);
    void ResetPersistent();
    void ResetTransient();
    fsobj(fsobj &) { abort(); } /* not supported! */
    int operator=(fsobj &)
    {
        abort();
        return (0);
    } /* not supported! */
    ~fsobj();
    void Recover();

    /* General status. */
    void Matriculate();
    void Demote(void);
    void Kill(int = 1);
    void GC();
    int Flush();
    void UpdateStatus(ViceStatus *, ViceVersionVector *, uid_t);
    int StatusEq(ViceStatus *);
    void ReplaceStatus(ViceStatus *, ViceVersionVector *);
    int CheckRcRights(int);
    void SetRcRights(int);
    void ClearRcRights();
    int IsValid(int);
    void SetAcRights(uid_t uid, long my_rights, long any_rights);
    void DemoteAcRights(uid_t);
    void PromoteAcRights(uid_t);
    void ClearAcRights(uid_t);
    void SetParent(VnodeId, Unique_t);
    void MakeDirty();
    void MakeClean();

    /* Mount state. */
    int TryToCover(VenusFid *, uid_t);
    void CoverMtPt(fsobj *);
    void UncoverMtPt();
    void MountRoot(fsobj *);
    void UnmountRoot();

    /* Child/Parent linkage. */
    void AttachChild(fsobj *);
    void DetachChild(fsobj *);

    /* Priority state. */
    void Reference();
    void ComputePriority(int Force = 0);
    void EnableReplacement();
    void DisableReplacement();
    binding *AttachHdbBinding(namectxt *);
    void DemoteHdbBindings();
    void DemoteHdbBinding(binding *);
    void DetachHdbBindings();
    void DetachHdbBinding(binding *, int = 0);

    /* MLE Linkage. */
    void AttachMleBinding(binding *);
    void DetachMleBinding(binding *);

    /* Data contents. */
    void DiscardData();
    void DiscardPartialData();

    /* Fake object management. */
    int Fakeify(uid_t uid);
    int IsFake() { return (flags.fake); }
    int IsFakeDir() { return (flags.fake && IsDir() && !IsMtPt()); }
    int IsFakeMtPt() { return (flags.fake && IsMtPt()); }
    int IsFakeMTLink() { return (flags.fake && IsMTLink()); }

    /* expansion related functions */
    int ExpandObject(void);
    int CollapseObject(void);
    int IsExpandedObj(void) { return (flags.expanded); }
    int IsExpandedDir(void) { return (flags.expanded && IsDir()); }
    int IsExpandedMTLink(void) { return (flags.expanded && IsMTLink()); }
    int IsModifiedObj(void) { return (flags.modified); }
    void SetMtLinkContents(VenusFid *fid);

    /* cmlent expansion related functions */
    void ExpandCMLEntries(void);
    void CollapseCMLEntries(void);
    int HasExpandedCMLEntries(void);

#define LOCALCACHE "_localcache" /* implies we have a locally cached copy */
#define LOCALCACHE_HIDDEN ".localcache" /* implies we don't */

    /* Local synchronization. */
    void Lock(LockLevel);
    void PromoteLock();
    void DemoteLock();
    void UnLock(LockLevel);

    /* Local-global conflict detection */
    int IsToBeRepaired(void);
    uid_t WhoIsLastAuthor(void);
    int LaunchASR(int, int);

    /* Interface to the dir package. */
    void dir_Create(const char *, VenusFid *);
    int dir_Length();
    void dir_Delete(const char *);
    void dir_MakeDir();
    int dir_LookupByFid(char *, VenusFid *);
    void dir_Rebuild();
    int dir_IsEmpty();
    int dir_IsParent(VenusFid *);
    void dir_Zap();
    void dir_Flush();
    void dir_TranslateFid(VenusFid *, VenusFid *);
    void dir_Print();

    /* Private portions of the CFS interface. */
    void LocalStore(Date_t, unsigned long);
    int DisconnectedStore(Date_t, uid_t, unsigned long, int prepend = 0);
    void LocalSetAttr(Date_t, unsigned long, Date_t, uid_t, unsigned short);
    int DisconnectedSetAttr(Date_t, uid_t, unsigned long, Date_t, uid_t,
                            unsigned short, int prepend = 0);
    void LocalCreate(Date_t, fsobj *, char *, uid_t, unsigned short);
    int DisconnectedCreate(Date_t, uid_t, fsobj **, char *, unsigned short, int,
                           int prepend = 0);
    void LocalRemove(Date_t, char *, fsobj *);
    int DisconnectedRemove(Date_t, uid_t, char *, fsobj *, int prepend = 0);
    void LocalLink(Date_t, char *, fsobj *);
    int DisconnectedLink(Date_t, uid_t, char *, fsobj *, int prepend = 0);
    void LocalRename(Date_t, fsobj *, char *, fsobj *, char *, fsobj *);
    int DisconnectedRename(Date_t, uid_t, fsobj *, char *, fsobj *, char *,
                           fsobj *, int prepend = 0);
    void LocalMkdir(Date_t, fsobj *, char *, uid_t, unsigned short);
    int DisconnectedMkdir(Date_t, uid_t, fsobj **, char *, unsigned short, int,
                          int prepend = 0);
    void LocalRmdir(Date_t, char *, fsobj *);
    int DisconnectedRmdir(Date_t, uid_t, char *, fsobj *, int prepend = 0);
    void LocalSymlink(Date_t, fsobj *, char *, char *, uid_t, unsigned short);
    int DisconnectedSymlink(Date_t, uid_t, fsobj **, char *, char *,
                            unsigned short, int, int prepend = 0);
    int GetContainerFD(void);
    int LookAside(void);
    int FetchFileRPC(connent *con, ViceStatus *status, uint64_t offset,
                     int64_t len, RPC2_CountedBS *PiggyBS, SE_Descriptor *sed);
    int OpenPioctlFile(void);

    void UpdateVastroFlag(uid_t uid, int force = 0, int state = 0x0);

public:
    /* The public CFS interface (Vice portion). */
    int Fetch(uid_t);
    int Fetch(uid_t uid, uint64_t pos, int64_t count);
    int GetAttr(uid_t, RPC2_BoundedBS * = 0);
    int GetACL(RPC2_BoundedBS *, uid_t);
    int Store(unsigned long, Date_t, uid_t);
    int SetAttr(struct coda_vattr *, uid_t);
    int SetACL(RPC2_CountedBS *, uid_t);
    int Create(char *, fsobj **, uid_t, unsigned short, int);
    int Remove(char *, fsobj *, uid_t);
    int Link(char *, fsobj *, uid_t);
    int Rename(fsobj *, char *, fsobj *, char *, fsobj *, uid_t);
    int Mkdir(char *, fsobj **, uid_t, unsigned short, int);
    int Rmdir(char *, fsobj *, uid_t);
    int Symlink(char *, char *, uid_t, unsigned short, int);
    int SetVV(ViceVersionVector *, uid_t);

    /* The public CFS interface (non-Vice portion). */
    int Open(int writep, int truncp, venus_cnode *cp, uid_t uid);
    int Sync(uid_t uid);
    void Release(int writep);
    int Close(int writep, uid_t uid);
    int Access(int rights, int modes, uid_t);
    int Lookup(fsobj **, VenusFid *, const char *, uid_t, int flags,
               int GetInconsistent = 0);
// These are defined in coda-src/kerndep/coda.h
// #define CLU_CASE_SENSITIVE	0x01
// #define CLU_CASE_INSENSITIVE 0x02
#define CLU_CASE_MASK 0x03
#define CLU_TRAVERSE_MTPT 0x04
    int Readdir(char *, int, int, int *, uid_t);
    int Readlink(char *, unsigned long, int *, uid_t);
    int ReadIntent(uid_t uid, int priority, uint64_t pos, int64_t count);
    int ReadIntentFinish(uint64_t pos, int64_t count);

    /* Miscellaneous utility routines. */
    int dir_Lookup(const char *, VenusFid *, int);
    int CheckAcRights(uid_t uid, long rights, int connected);
    void GetVattr(struct coda_vattr *); /* translate attributes to VFS format */
    void GetFid(VenusFid *f) { *f = fid; }

#define PATH_VOLUME 0
#define PATH_FULL 1
#define PATH_REALM 2
#define PATH_COMPONENT 3
    void GetPath(char *, int scope = PATH_VOLUME);

    ViceVersionVector *VV() { return (&stat.VV); }
    int IsFile() { return (stat.VnodeType == (int)File); }
    int IsDir() { return (stat.VnodeType == (int)Directory); }
    int IsSymLink() { return (stat.VnodeType == (int)SymbolicLink); }
    int IsNormal() { return (mvstat == NORMAL); }
    int IsRoot() { return (mvstat == ROOT); }
    int IsVenusRoot() { return (FID_EQ(&fid, &rootfid)); }
    int IsMtPt() { return (mvstat == MOUNTPOINT); } /* covered mount point */
    int IsMTLink() { return (IsSymLink() && stat.Mode == 0644 && IsNormal()); }
    /* uncovered mount point */
    int IsVirgin(); /* file which has been created, but not yet stored */
    int IsBackFetching(); /* fso involved in an ongoing reintegration */
    int IsPioctlFile()
    { /* Test for pioctl object. */
        return (fid.Realm == LocalRealm->Id() &&
                fid.Volume == FakeRootVolumeId && fid.Vnode == 0xfffffffa);
    }
    int SetLastResolved(long t)
    {
        lastresolved = t;
        return (0);
    }
    int MakeShadow();
    void RemoveShadow();
    void CacheReport(int, int);
    CacheChunkList *GetHoles(uint64_t start, int64_t len);

    void print() { print(stdout); }
    void print(FILE *fp)
    {
        fflush(fp);
        print(fileno(fp));
    }
    void print(int);

    void ListCache(FILE *, int long_format = 0, unsigned int valid = 3);
    void ListCacheShort(FILE *);
    void ListCacheLong(FILE *);

    /* local-repair additions */
    cmlent *FinalCmlent(int); /*N*/
    void SetComp(const char *); /*U*/
    const char *GetComp(void);
    void SetLocalObj(); /*T*/
    void UnsetLocalObj(); /*T*/
    int IsLocalObj() { return flags.local; } /*N*/
    int IsAncestor(VenusFid *); /*N*/

    void DeLocalRootParent(fsobj *, VenusFid *, fsobj *); /*U*/
    void RecoverRootParent(VenusFid *, char *); /*U*/

    int SetLocalVV(ViceVersionVector *);

    int RepairStore();
    int RepairSetAttr(unsigned long, Date_t, uid_t, unsigned short,
                      RPC2_CountedBS *);
    int RepairCreate(fsobj **, char *, unsigned short, int);
    int RepairRemove(char *, fsobj *);
    int RepairLink(char *, fsobj *);
    int RepairRename(fsobj *, char *, fsobj *, char *, fsobj *);
    int RepairMkdir(fsobj **, char *, unsigned short, int);
    int RepairRmdir(char *, fsobj *);
    int RepairSymlink(fsobj **, char *, char *, unsigned short, int);

    void FetchProgressIndicator(unsigned long offset);

    size_t Size(void) { return stat.Length; }
};

class fso_iterator : public rec_ohashtab_iterator {
    LockLevel clevel; /* locking level */
    volent *cvol; /* 0 --> all volumes */

public:
    fso_iterator(LockLevel, const VenusFid * = (VenusFid *)-1);
    fsobj *operator()();
};

/*  *****  Variables  ***** */

extern unsigned int PartialCacheFilesRatio;
extern int FSO_SWT;
extern int FSO_MWT;
extern int FSO_SSF;

/*  *****  Functions/Procedures  *****  */

/* fso0.c */
extern void FSOInit();
extern int FSO_PriorityFN(bsnode *, bsnode *);
extern void UpdateCacheStats(CacheStats *c, enum CacheEvent event,
                             unsigned long blocks);
extern void PrintCacheStats(const char *description, CacheStats *, int);
extern void VenusToViceStatus(VenusStat *, ViceStatus *);

/* fso_daemon.c */
void FSOD_Init(void);
void FSOD_ReclaimFSOs(void);

/* More locking macros. */
#define FSO_HOLD(f)    \
    {                  \
        (f)->refcnt++; \
    }
#define FSO_RELE(f)    \
    {                  \
        (f)->refcnt--; \
    }

/* Some useful state predicates. */
#define UNREACHABLE(f) ((f)->vol->IsUnreachable())
#define REACHABLE(f) ((f)->vol->IsReachable())
#define RESOLVING(f) ((f)->vol->IsResolving())
#define DIRTY(f) ((f)->flags.dirty)
#define HAVESTATUS(f) ((f)->state != FsoRunt)
#define STATUSVALID(f) ((f)->IsValid(RC_STATUS))
#define HAVEDATA(f) ((f)->data.havedata != 0)
#define PARTIALDATA(f) ((f)->IsFile() && !(f)->cf.IsComplete())
#define HAVEALLDATA(f) (HAVEDATA(f) && !PARTIALDATA(f))
#define DATAVALID(f) ((f)->IsValid(RC_DATA))
#define EXECUTABLE(f) \
    (HAVESTATUS(f) && (f)->IsFile() && ((f)->stat.Mode & 0111))
#define DYING(f) ((f)->state == FsoDying)
#define READING(f) (((f)->openers - (f)->Writers) > 0)
#define WRITING(f) ((f)->Writers > 0)
#define EXECUTING(f) (EXECUTABLE(f) && READING(f) && !k_Purge(&(f)->fid))
#define ACTIVE(f) (WRITING(f) || READING(f)) // was EXECUTING(f)
#define BUSY(f) ((f)->refcnt > 0 || EXECUTING(f))
#define HOARDABLE(f) ((f)->HoardPri > 0)
#define ISVASTRO(f) ((f)->flags.vastro)
#define FETCHABLE(f)                           \
    (!DYING(f) && REACHABLE(f) && !DIRTY(f) && \
     (!HAVESTATUS(f) || !WRITING(f) || ISVASTRO(f)) && !f->IsLocalObj())
/* we are replaceable whenever we are linked into FSDB->prioq */
#define REPLACEABLE(f) ((f)->prio_handle.tree() != 0)
#define GCABLE(f) (DYING(f) && !DIRTY(f) && !BUSY(f))
#define FLUSHABLE(f) ((DYING(f) || REPLACEABLE(f)) && !DIRTY(f) && !BUSY(f))
#define BLOCKS(f) (NBLOCKS((f)->stat.Length))

#define FSO_ASSERT(f, ex)                                               \
    {                                                                   \
        if (!(ex)) {                                                    \
            (f)->print(logFile);                                        \
            CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, \
                  __LINE__);                                            \
        }                                                               \
    }

#define CFSOP_PRELUDE(str, comp, fid)       \
    {                                       \
        char buf[CODA_MAXNAMLEN + 1];       \
        if (comp && comp[0] != '\0')        \
            strcpy(buf, comp);              \
        else                                \
            sprintf(buf, "%s", FID_(&fid)); \
        MarinerLog((str), buf);             \
    }
#define CFSOP_POSTLUDE(str) MarinerLog((str));

#define PrintFsoState(state) \
    (state == FsoRunt ?      \
         "Runt" :            \
         state == FsoNormal ? "Normal" : state == FsoDying ? "Dying" : "???")
#define PrintVnodeType(vnodetype)      \
    (vnodetype == (int)File ?          \
         "File" :                      \
         vnodetype == (int)Directory ? \
         "Directory" :                 \
         vnodetype == (int)SymbolicLink ? "Symlink" : "???")
#define PrintMvStat(mvstat)                    \
    (mvstat == NORMAL ?                        \
         "Normal" :                            \
         mvstat == MOUNTPOINT ? "MountPoint" : \
                                mvstat == ROOT ? "Root" : "???")

#endif /* _VENUS_FSO_H_ */
