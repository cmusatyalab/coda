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
class fso_vol_iterator;

class cmlent;			    /* we have compiler troubles if volume.h is included! */
class lrdb;

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/uio.h>

#include <rpc2.h>

#include <codadir.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>
#include <admon.h>

/* from util */
#include <bstree.h>
#include <rec_bstree.h>
#include <dlist.h>
#include <rec_dlist.h>
#include <ohash.h>
#include <rec_ohash.h>
#include <olist.h>
#include <rec_olist.h>

/* from venus */
#include "binding.h"
#include "comm.h"
#include "hdb.h"
#include "mariner.h"
#include "venusrecov.h"
#include "vproc.h"
#include "venus.private.h"

/* from coda include again, must appear AFTER venus.private.h */

/*  *****  Constants  ***** */

#define	FSDB	(rvg->recov_FSDB)
const int FSDB_MagicNumber = 3620289;
const int FSDB_NBUCKETS = 2048;
const int FSO_MagicNumber = 2687694;

const int BLOCKS_PER_FILE = 24;			    /* rule of thumb */
const int DFLT_CF = DFLT_CB / BLOCKS_PER_FILE;
const int UNSET_CF = -1;
const int MIN_CF = MIN_CB / BLOCKS_PER_FILE;

const int PIGGY_VALIDATIONS = 50;  /* number of objects we can validate on the side */

/* Priorities. */
const int FSO_MAX_SPRI = H_MAX_PRI;
const int FSO_MAX_MPRI = H_MAX_PRI;
const int DFLT_SWT = 25;
const int UNSET_SWT = -1;
const int DFLT_MWT = 75;
const int UNSET_MWT = -1;
const int DFLT_SSF = 4;
const int UNSET_SSF = -1;

const int CPSIZE = 8;

/* Replica Control Rights. */
/* Note that we presently do not distinguish between read and write rights. */
/* We may well do so in the future, however. */
#define	RC_STATUSREAD	1
#define	RC_STATUSWRITE	2
#define	RC_STATUS	(RC_STATUSREAD | RC_STATUSWRITE)
#define	RC_DATAREAD	4
#define	RC_DATAWRITE	8
#define	RC_DATA		(RC_DATAREAD | RC_DATAWRITE)


/*  *****  Types  ***** */
/* Cache stuff was removed here to move to venus.private.h  5/14/92 */

void FSODaemon(); /* used to be member of class fsdb (Satya 3/31/95) */


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
  friend void VmonUpdateSession(vproc *vp, ViceFid *key, fsobj *f, volent *vol, vuid_t vuid, enum CacheType datatype, enum CacheEvent event, unsigned long blocks);

    int MagicNumber;
    int DataVersion;

    /* Size parameters. */
    int MaxFiles;
    /* "files" is kept as count member of htab */
    int FreeFileMargin;
    /*T*/int MaxBlocks;
    /*T*/int blocks;
    /*T*/int FreeBlockMargin;

    /* Priority parameters. */
    int	swt;			/* short-term component weight */
    int	mwt;			/* medium-term component weight */
    int	ssf;			/* short-term scaling factor */
    int	maxpri;			/* maximum priority */
    int	stdpri;			/* standard priority (for VFS operations) */
    int	marginpri;		/* margin priority (for GetDown) */

    /* The hash table. */
    rec_ohashtab htab;

    /* The free list. */
    rec_olist freelist;

    /* The priority queue. */
    /*T*/bstree *prioq;
    long *LastRef;
    /*T*/long RefCounter;	     /* used to compute short-term priority */

    /* The delete queue.  Objects are sent here to be garbage collected. */
    /*T*/dlist *delq;

    /* Queue of files open for write. */
    /*T*/olist *owriteq;

    /* Statistics. */
    /*T*/CacheStats DirAttrStats;
    /*T*/CacheStats DirDataStats;
    /*T*/CacheStats FileAttrStats;
    /*T*/CacheStats FileDataStats;
    int VolumeLevelMiss;              /* Counter to pass to data collection; Stored in RVM */
    /*T*/int Recomputes;			    /* total priority recomputations */
    /*T*/int Reorders;				    /* number of resulting prioq reorders */

    /* Synchronization stuff for matriculating objects. */
    /*T*/char matriculation_sync;
    /*T*/int matriculation_count;

    /* Device handle for opening files by <dev,ino> rather than name. */
    /*T*/dev_t device;

    /* Constructors, destructors. */
    void *operator new(size_t);
    void operator delete(void *, size_t);

    fsdb();
    void ResetTransient();
    ~fsdb() { abort(); }

    /* Allocation/Deallocation routines. */
    fsobj *Create(ViceFid *, LockLevel, int, char *);
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
    void RecomputePriorities(int =0);
    void GarbageCollect();
    void GetDown();
    void FlushRefVec();

  public:
    fsobj *Find(ViceFid *);
    /* rcode arg added for local repair */
    int Get(fsobj **fso, ViceFid *fid, vuid_t vuid, int rights, char *comp=0,
	    int *rcode=0, int GetInconsistent=0);
    void Put(fsobj **);
    void Flush();
    void Flush(VolumeId);
    int TranslateFid(ViceFid *, ViceFid *);
    int CallBackBreak(ViceFid *);
    void ResetVolume(VolumeId, int);
    void ResetUser(vuid_t);
    void ClearPriorities();
    void InvalidateMtPts();
    int MakePri(int spri, int mpri)
      { return(swt * spri + mwt * mpri); }
    int MaxPri()
      { return(maxpri); }
    int StdPri()
      { return(stdpri); }
    int MarginPri()
      { return(marginpri); }

    void SetDiscoRefCounter();
    void UnsetDiscoRefCounter();

    void DisconnectedCacheMiss(vproc *, vuid_t, ViceFid *, char *);
    void UpdateDisconnectedUseStatistics(volent *);
    void OutputDisconnectedUseStatistics(char *, int, int, int);

    void GetStats(int *fa, int *fo, int *ba, int *bo) 
      { *fa = MaxFiles; *fo = htab.count(); *ba = MaxBlocks; *bo = blocks; }


    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int, int =0);
};

enum FsoState {	FsoRunt,
		FsoNormal,
		FsoDying
};

/* Representation of UFS file in cache directory. */
/* Currently, CacheFiles and fsobjs are statically bound to each
   other, one-to-one, by embedding */
/* a single CacheFile in each fsobj.  An fsobj may use its CacheFile
   in several ways (or not at all). */
/* We guarantee that these uses are mutually exclusive (in time,
   per-fsobj), hence the static allocation */
/* policy works.  In the future we may choose not to make the uses
   mutually exclusive, and will then */
/* have to implement some sort of dynamic allocation/binding
   scheme. */
/* The different uses of CacheFile are: */
/*    1. Copy of plain file */
/*    2. Unix-format copy of directory */
class CacheFile {
    long length;
    long validdata; /* amount of successfully fetched data */

    int ValidContainer();
    void ResetContainer();

  public:
    ino_t inode;				/* for iopen() */
    char name[8];				/* "Vxxxxxx" */
    CacheFile(int);
    CacheFile();
    ~CacheFile();

    void Validate();
    void Reset();
    void Swap(CacheFile *);
    void Copy(CacheFile *);
    void Remove();

    void Stat(struct stat *);
    void Truncate(long);
    void SetLength(long);
    void SetValidData(long);

    char *Name()         { return(name); }
    ino_t Inode()        { return(inode); }
    long Length()        { return(length); }
    long ValidData(void) { return(validdata); }
    int  IsPartial(void) { return(length != validdata); }

    void print() { print (stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

/* Condensed version of ViceStatus. */
struct VenusStat {
    ViceDataType VnodeType;
    unsigned char LinkCount;
    long	  Length;
    unsigned int  DataVersion;
    ViceVersionVector VV;
    Date_t Date;
    vuid_t Author;
    vuid_t Owner;
    unsigned short Mode;
};

/* Condensed version of VenusStat. */
/* needed to restore objects after store cancellation */
struct MiniVenusStat {
    long   Length;
    Date_t Date;
};

/* Access Control Rights */
struct AcRights	{
    vuid_t uid;
    unsigned char rights;
    unsigned inuse : 1;
    /*T*/unsigned valid : 1;
};

struct FsoFlags {
    /*T*/unsigned backup : 1;			/* volume type; copied from volume */
    /*T*/unsigned readonly : 1;			/* volume type; copied from volume */
    /*T*/unsigned replicated : 1;		/* volume type; copied from volume */
    /*T*/unsigned rwreplica : 1;		/* volume type; copied from volume */
    /*T*/unsigned usecallback :	1;		/* volume characteristic; copied from volume */
    unsigned fake : 1;				/* is this object fake? (c.f. repair) */
    unsigned owrite : 1;			/* file open for write? */
    unsigned fetching :	1;			/* fetch in progress? */
    /*T*/unsigned replaceable : 1;		/* is this object replaceable? */
    /*T*/unsigned era : 1;			/* early returns allowed? */
    unsigned dirty : 1;				/* is this object dirty? */
    /*T*/unsigned ckmtpt : 1;			/* mount point needs checked? */
    unsigned local: 1;				/* local fake fid */
    unsigned discread : 1;			/* read during the last disconnection */
    /*T*/unsigned random : 16;			/* help balance binary-search trees */
};

enum MountStatus {  NORMAL,
		    MOUNTPOINT,
		    ROOT
};


struct VenusDirData {
	/* Vice format directory in VM/RVM. */
	struct DirHandle dh;
	/* Unix format directory in UFS. */
	/*T*/unsigned udcfvalid : 1;
	/*T*/CacheFile *udcf;
	/*T*/int padding;
};

union VenusData {
    int	havedata;	/* generic test for null pointer (pretty gross, eh) */
    CacheFile *file;	/* VnodeType == File */
    VenusDirData *dir;	/* VnodeType == Directory */
    char *symlink;	/* VnodeType == SymbolicLink */
};


/* local-repair modification */
typedef enum {FROMHEAP, FROMFREELIST} fso_alloc_t; /* placement argument to operator new() */

typedef enum { HF_Fetch, HF_DontFetch } HoardFetchState;
typedef enum { HA_Ask, HA_DontAsk } HoardAskState;

class fsobj {
  friend void FSOInit();
  friend int FSO_PriorityFN(bsnode *, bsnode *);
  friend class fsdb;
  friend class fso_iterator;
  friend class fso_vol_iterator;
  friend long CallBackFetch(RPC2_Handle, ViceFid *, SE_Descriptor *);
  friend class vproc;
  friend class namectxt;
  friend class volent;
  friend class ClientModifyLog;
  friend class cmlent;
  friend class cml_iterator;
  friend class resent;
  friend class mgrpent;
  friend class hdb;
  friend class lrdb;
  friend void RecoverPathName(char *, ViceFid *, ClientModifyLog *, cmlent *);
  friend void VmonUpdateSession(vproc *, ViceFid *, fsobj *, volent *, vuid_t, enum CacheType, enum CacheEvent, unsigned long);

    int MagicNumber;

    /* Keys. */
    ViceFid fid;				/* unique id for object */
    char *comp;					/* most recently used component */
    /*T*/volent *vol;				/* pointer to object's volume */

    /* Links for various lists. */
    rec_olink primary_handle;			/* link for {fstab, free-list} */
    /*T*/olink vol_handle;                      /* link for volent fso_list */
    /*T*/bsnode prio_handle;			/* link for priority queue */
    /*T*/dlink del_handle;			/* link for delete queue */
    /*T*/olink owrite_handle;			/* link for owrite queue */

    /* General status. */
    enum FsoState state;			/* {FsoRunt, FsoNormal, FsoDying} */
    VenusStat stat;
    /*T*/long GotThisData;			/* used during fetch to keep
						   track of where we are */
    /*T*/int RcRights;				/* replica control rights */
    AcRights AnyUser;				/* access control rights: any user */
    AcRights SpecificUser[CPSIZE];		/* access control rights: specific users */
    FsoFlags flags;				/* some of these are transient */

    /* Mount state. */
    MountStatus	mvstat;				/* { NORMAL, MOUNTPOINT, ROOT } */
    /*T*/union {
						/* mvstat == NORMAL */
	fsobj *root;				/* mvstat == MOUNTPOINT */
	fsobj *mtpoint;				/* mvstat == ROOT */
    } u;

    /* Child/Parent linkage. */
    ViceFid pfid;
    /*T*/fsobj *pfso;				/* back pointer from child to parent */
    /*T*/dlist *children;			/* for directories; list of cached children */
    /*T*/dlink child_link;			/* link used for that list */

    /* Priority state. */
    /*T*/int priority;				/* f(spri, mpri) */
    /*T*/int HoardPri;				/* max of priorities of binders */
    /*T*/vuid_t HoardVuid;			/* vuid of that entry */
    /*T*/dlist *hdb_bindings;			/* list of (bindings to) hdbents referencing this object */
    /*T*/int FetchAllowed;                      /* Allow a hoard walk to fetch object? */
    /*T*/int AskingAllowed;                     /* Ask user for hoard fetch advice? */


    /* MLE linkage. */
    /* T */dlist *mle_bindings;			/* list of (bindings to) mlents referencing this object */
    MiniVenusStat CleanStat;			/* last status before becoming dirty */
    /* T */ViceStoreId tSid;			/* temporary for serializing MLEs */
    /*T*/CacheFile *shadow;                     /* shadow copy, temporary during reintegration */

    /* Data contents. */
    VenusData data;

    /* Statically allocated cache-file stuff. */
    /* Eventually we might make cache-file allocation dynamic, in which case there would be */
    /* various of these pointed to by the VenusData descriptors! */
    int	ix;
    CacheFile cf;

    /* Local synchronization state. */
    /*T*/char sync;				/* for waiting/signalling */
    /*T*/short readers;				/* entry readers, not object readers */
    /*T*/short writers;				/* entry writers, not object writers */
    /*T*/short openers;				/* object openers */
    /*T*/short Writers;				/* object writers */
    /*T*/short Execers;				/* object execers (we don't know this under VFS!) */
    /*T*/short refcnt;				/* readers + writers + openers + temporary_refs */   
    CacheEventRecord cachehit;                  /* cache reference count */
    CacheEventRecord cachemiss;                 /* cache miss count */
    CacheEventRecord cachenospace;              /* cache no space */

    // Disconnected Use Statistics
    long DisconnectionsSinceUse;
    long DisconnectionsUsed;
    long DisconnectionsUnused;

    // for asr invocation
    /*T*/long lastresolved;			// time when object was last resolved

    /* Constructors, destructors. */
    void *operator new(size_t, fso_alloc_t, int); /* for allocation from freelist */
    void *operator new(size_t, fso_alloc_t); /* for allocation from heap */
    void *operator new(size_t); /* dummy to pacify g++ */
    void operator delete(void *, size_t);
    fsobj(int);
    fsobj(ViceFid *, char *);
    void ResetPersistent();
    void ResetTransient();
    fsobj(fsobj&) { abort(); }                          /* not supported! */
    operator=(fsobj&) { abort(); return(0); }           /* not supported! */
    ~fsobj();
    void Recover();

    /* General status. */
    void Matriculate();
    void Demote(int =1);
    void Kill(int =1);
    void GC();
    int Flush();
    void UpdateStatus(ViceStatus *, vuid_t);
    void UpdateStatus(ViceStatus *, ViceVersionVector *, vuid_t);
    int StatusEq(ViceStatus *, int);
    void ReplaceStatus(ViceStatus *, ViceVersionVector *);
    int CheckRcRights(int);
    void SetRcRights(int);
    void ReturnRcRights();
    void ClearRcRights();
    int IsValid(int);
    int CheckAcRights(vuid_t, long, int);
    void SetAcRights(vuid_t, long);
    void DemoteAcRights(vuid_t);
    void PromoteAcRights(vuid_t);
    void ClearAcRights(vuid_t);
    void SetParent(VnodeId, Unique_t);
    void MakeDirty();
    void MakeClean();

    /* Mount state. */
    int TryToCover(ViceFid *, vuid_t);
    void CoverMtPt(fsobj *);
    void UncoverMtPt();
    void MountRoot(fsobj *);
    void UnmountRoot();

    /* Child/Parent linkage. */
    void AttachChild(fsobj *);
    void DetachChild(fsobj *);

    /* Priority state. */
    void Reference();
    void ComputePriority();
    void EnableReplacement();
    void DisableReplacement();
    void AttachHdbBinding(binding *);
    void DemoteHdbBindings();
    void DemoteHdbBinding(binding *);
    void DetachHdbBindings();
    void DetachHdbBinding(binding *, int =0);
    int PredetermineFetchState(int, int);
    void SetFetchAllowed(enum HoardFetchState new_state)
        { FetchAllowed = new_state; }
    int IsFetchAllowed()
        { if (FetchAllowed == HF_Fetch) 
              return 1; 
          else 
              return 0; 
        }
    void SetAskingAllowed(enum HoardAskState new_state)
        { AskingAllowed = new_state; }
    int IsAskingAllowed()
        { if (AskingAllowed == HA_Ask)
            return 1;
          else
            return 0;
        }

    /* advice routines */
    CacheMissAdvice ReadDisconnectedCacheMiss(vproc *, vuid_t);
    CacheMissAdvice WeaklyConnectedCacheMiss(vproc *, vuid_t);
    void DisconnectedCacheMiss(vproc *, vuid_t, char *);

    /* MLE Linkage. */
    void AttachMleBinding(binding *);
    void DetachMleBinding(binding *);
    void CancelStores();

    /* Data contents. */
    void DiscardData();

    /* Fake object management. */
    int Fakeify();
    int IsFake() { return(flags.fake); }
    int IsFakeDir() { return(flags.fake && IsDir() && !IsMtPt()); }
    int IsFakeMtPt() { return(flags.fake && IsMtPt()); }
    int IsFakeMTLink() { return(flags.fake && IsMTLink()); }

    /* Local synchronization. */
    void Lock(LockLevel);
    void PromoteLock();
    void DemoteLock();
    void UnLock(LockLevel);

    /* Interface to the dir package. */
    void dir_Create(char *, ViceFid *);
    int dir_Length();
    void dir_Delete(char *);
    void dir_MakeDir();
    int dir_Lookup(char *, ViceFid *, int);
    int dir_LookupByFid(char *, ViceFid *);
    void dir_Rebuild();
    int dir_IsEmpty();
    int dir_IsParent(ViceFid *);
    void dir_Zap();
    void dir_Flush();
    void dir_TranslateFid(ViceFid *, ViceFid *);
    void dir_Print();

    /* Private portions of the CFS interface. */
    void LocalStore(Date_t, unsigned long);
    int ConnectedStore(Date_t, vuid_t, unsigned long);
    int DisconnectedStore(Date_t, vuid_t, unsigned long, int);
    void LocalSetAttr(Date_t, unsigned long, Date_t,
		       vuid_t, unsigned short);
    int ConnectedSetAttr(Date_t, vuid_t, unsigned long, Date_t,
			  vuid_t, unsigned short, RPC2_CountedBS *);
    int DisconnectedSetAttr(Date_t, vuid_t, unsigned long, Date_t,
			     vuid_t, unsigned short, int);
    void LocalCreate(Date_t, fsobj *, char *,
		      vuid_t, unsigned short);
    int ConnectedCreate(Date_t, vuid_t, fsobj **,
			 char *, unsigned short, int);
    int DisconnectedCreate(Date_t, vuid_t, fsobj **,
			    char *, unsigned short, int, int);
    void LocalRemove(Date_t, char *, fsobj *);
    int ConnectedRemove(Date_t, vuid_t, char *, fsobj *);
    int DisconnectedRemove(Date_t, vuid_t, char *, fsobj *, int);
    void LocalLink(Date_t, char *, fsobj *);
    int ConnectedLink(Date_t, vuid_t, char *, fsobj *);
    int DisconnectedLink(Date_t, vuid_t, char *, fsobj *, int);
    void LocalRename(Date_t, fsobj *, char *,
		      fsobj *, char *, fsobj *);
    int ConnectedRename(Date_t, vuid_t, fsobj *,
			 char *, fsobj *, char *, fsobj *);
    int DisconnectedRename(Date_t, vuid_t, fsobj *,
			    char *, fsobj *, char *, fsobj *, int);
    void LocalMkdir(Date_t, fsobj *, char *, vuid_t, unsigned short);
    int ConnectedMkdir(Date_t, vuid_t, fsobj **,
			char *, unsigned short, int);
    int DisconnectedMkdir(Date_t, vuid_t, fsobj **,
			   char *, unsigned short, int, int);
    void LocalRmdir(Date_t, char *, fsobj *);
    int ConnectedRmdir(Date_t, vuid_t, char *, fsobj *);
    int DisconnectedRmdir(Date_t, vuid_t, char *, fsobj *, int);
    void LocalSymlink(Date_t, fsobj *, char *,
		       char *, vuid_t, unsigned short);
    int ConnectedSymlink(Date_t, vuid_t, fsobj **, char *,
			  char *, unsigned short, int);
    int DisconnectedSymlink(Date_t, vuid_t, fsobj **, char *,
			     char *, unsigned short, int, int);

  public:
    /* The public CFS interface (Vice portion). */
    int Fetch(vuid_t);
    int GetAttr(vuid_t, RPC2_BoundedBS * =0);
    int GetACL(RPC2_BoundedBS *, vuid_t);
    int Store(unsigned long, Date_t, vuid_t);
    int SetAttr(struct coda_vattr *, vuid_t, RPC2_CountedBS * =0);
    int SetACL(RPC2_CountedBS *, vuid_t);
    int Create(char *, fsobj **, vuid_t, unsigned short, int);
    int Remove(char *, fsobj *, vuid_t);
    int Link(char *, fsobj *, vuid_t);
    int Rename(fsobj *, char *, fsobj *, char *, fsobj *, vuid_t);
    int Mkdir(char *, fsobj **, vuid_t, unsigned short, int);
    int Rmdir(char *, fsobj *, vuid_t);
    int Symlink(char *, char *, vuid_t, unsigned short, int);
    int SetVV(ViceVersionVector *, vuid_t);

    /* The public CFS interface (non-Vice portion). */
    int Open(int, int, int, venus_cnode *, vuid_t);
    int Close(int, int, vuid_t);
    /*    int RdWr(char *, enum uio_rw, int, int, int *, vuid_t); */
    int Access(long, int, vuid_t);
    int Lookup(fsobj **, ViceFid *, char *, vuid_t, int);
    int Readdir(char *, int, int, int *, vuid_t);
    int Readlink(char *, int, int *, vuid_t);

    /* Miscellaneous utility routines. */
    void GetVattr(struct coda_vattr *);		/* translate attributes to VFS format */
    void ReturnEarly();
    void GetPath(char *, int =0);		/* from volume-root (NOT Venus-root) */
    int IsFile() { return(stat.VnodeType == (int)File); }
    int IsDir() { return(stat.VnodeType == (int)Directory); }
    int IsSymLink() { return(stat.VnodeType == (int)SymbolicLink); }
    int IsNormal() { return(mvstat == NORMAL); }
    int IsRoot() { return(mvstat == ROOT); }
    int IsVenusRoot() { return(FID_EQ(&fid, &rootfid)); }
    int	IsMtPt() { return(mvstat == MOUNTPOINT); }      /* covered mount point */
    int	IsMTLink() { return(stat.VnodeType == (int)SymbolicLink && stat.Mode == 0644); }
                                                        /* uncovered mount point */
    int	IsVirgin();                             /* file which has been created, but not yet stored */
    int IsBackFetching();			/* fso involved in an ongoing reintegration */
    int SetLastResolved(long t) { lastresolved = t; return(0); }
    void MakeShadow();
    void RemoveShadow();
    void CacheReport(int, int);

    int /*(secs)*/ fsobj::EstimatedFetchCost(int =1);  /* 0 = status; 1 = data (default) */
    void RecordReplacement(int, int);

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);

    void ListCache(FILE *, int long_format=0, unsigned int valid=3);
    void ListCacheShort(FILE *);
    void ListCacheLong(FILE *);

    /* local-repair additions */
    void GetOperationState(int *, int *);                       /*N*/
    cmlent *FinalCmlent(int);                                   /*N*/
    void SetComp(char *);                                       /*U*/
    void SetLocalObj();						/*T*/
    void UnsetLocalObj();					/*T*/
    int IsLocalObj() { return flags.local; }			/*N*/
    int IsAncestor(ViceFid *);					/*N*/
    int ReplaceLocalFakeFid();					/*U*/
    int LocalFakeify();						/*U*/
    int LocalFakeifyRoot();					/*U*/

    void MixedToGlobal(ViceFid *, ViceFid *, char *);        	/*U*/
    void MixedToLocal(ViceFid *, ViceFid *, char *);        	/*U*/
    void GlobalToMixed(ViceFid *, ViceFid *, char *);        	/*U*/
    void LocalToMixed(ViceFid *, ViceFid *, char *);        	/*U*/
    void DeLocalRootParent(fsobj *, ViceFid *, fsobj *);	/*U*/
    void RecoverRootParent(ViceFid *, char *);			/*U*/

    int RepairStore();
    int RepairSetAttr(unsigned long, Date_t, vuid_t, unsigned short, RPC2_CountedBS *);
    int RepairCreate(fsobj **, char *, unsigned short, int);
    int RepairRemove(char *, fsobj *);
    int RepairLink(char *, fsobj *);
    int RepairRename(fsobj *, char *, fsobj *, char *, fsobj *);
    int RepairMkdir(fsobj **, char *, unsigned short, int);
    int RepairRmdir(char *, fsobj *);
    int RepairSymlink(fsobj **, char *, char *, unsigned short, int);

    void FetchProgressIndicator(long offset);
};

class fso_iterator : public rec_ohashtab_iterator {
    LockLevel clevel;	    /* locking level */
    volent *cvol;	    /* 0 --> all volumes */

  public:
    fso_iterator(LockLevel, ViceFid * =(ViceFid *)-1);
    fsobj *operator()();
};

class fso_vol_iterator : public olist_iterator {
    LockLevel clevel;

 public:
    fso_vol_iterator(LockLevel, volent *);
    fsobj *operator()();
};

/*  *****  Variables  ***** */

extern int CacheFiles;
extern int FSO_SWT;
extern int FSO_MWT;
extern int FSO_SSF;


/*  *****  Functions/Procedures  *****  */

/* fso0.c */
extern void FSOInit();
extern int FSO_PriorityFN(bsnode *, bsnode *);
extern void UpdateCacheStats(CacheStats *, enum CacheEvent, unsigned long);
extern void PrintCacheStats(char* description, CacheStats *, int);
extern void VenusToViceStatus(VenusStat *, ViceStatus *);

/* fso_daemon.c */
extern void FSOD_Init();

/* More locking macros. */
#define	FSO_HOLD(f)	    { (f)->refcnt++; }
#define FSO_RELE(f)	    { (f)->refcnt--; }

/* Some useful state predicates. */
#define	HOARDING(f)	((f)->vol->state == Hoarding)
#define	EMULATING(f)	((f)->vol->state == Emulating)
#define LOGGING(f)      ((f)->vol->state == Logging)
#define	DIRTY(f)	((f)->flags.dirty)
#define	HAVESTATUS(f)	((f)->state != FsoRunt)
#define	STATUSVALID(f)	((f)->IsValid(RC_STATUS))
#define	HAVEDATA(f)	((f)->data.havedata != 0)
#define	PARTIALDATA(f)	((f)->IsFile() && (f)->cf.IsPartial())
#define	HAVEALLDATA(f)	(HAVEDATA(f) && !PARTIALDATA(f))
#define	DATAVALID(f)	((f)->IsValid(RC_DATA))
#define	EXECUTABLE(f)	(HAVESTATUS(f) &&\
			 (f)->IsFile() &&\
			 ((f)->stat.Mode & 0111))
#define	DYING(f)	((f)->state == FsoDying)
#define	READING(f)	(((f)->openers - (f)->Writers) > 0)
#define	WRITING(f)	((f)->Writers > 0)
#define	EXECUTING(f)	(EXECUTABLE(f) &&\
			 !k_Purge(&(f)->fid))
#define	BUSY(f)		((f)->refcnt > 0 ||\
			 EXECUTING(f))
#define	HOARDABLE(f)	((f)->HoardPri > 0)
#define	FETCHABLE(f)	(!DYING(f) &&\
			 (HOARDING(f) ||\
			  (LOGGING(f) && !DIRTY(f))) &&\
			 (!HAVESTATUS(f) ||\
			  (!WRITING(f) && !EXECUTING(f))))
#define	REPLACEABLE(f)	((f)->flags.replaceable)
#define	GCABLE(f)	(DYING(f) && !DIRTY(f) && !BUSY(f))
#define	FLUSHABLE(f)	(((DYING(f) && !DIRTY(f)) ||\
			 REPLACEABLE(f)) && !BUSY(f))
#define	BLOCKS(f)	(NBLOCKS((f)->stat.Length))


#define	FSO_ASSERT(f, ex)\
{\
    if (!(ex)) {\
	(f)->print(logFile);\
	CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
    }\
}

#define	CFSOP_PRELUDE(str, comp, fid)\
{\
    char buf[256];\
    strcpy(buf, (comp));\
    if (buf[0] == '\0')\
	sprintf(buf, "%s", FID_(&(fid)));\
    MarinerLog((str), buf);\
}
#define	CFSOP_POSTLUDE(str)\
    MarinerLog((str));

#define	PrintFsoState(state)\
    (state == FsoRunt ? "Runt" :\
     state == FsoNormal ? "Normal":\
     state == FsoDying ? "Dying" :\
     "???")
#define	PrintVnodeType(vnodetype)\
    (vnodetype == (int)File ? "File" :\
     vnodetype == (int)Directory ? "Directory" :\
     vnodetype == (int)SymbolicLink ? "Symlink" :\
     "???")
#define	PrintMvStat(mvstat)\
    (mvstat == NORMAL ? "Normal" :\
     mvstat == MOUNTPOINT ? "MountPoint" :\
     mvstat == ROOT ? "Root" :\
     "???")

#endif	/* _VENUS_FSO_H_ */
