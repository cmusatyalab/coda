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



#ifndef _LOCAL_H_
#define _LOCAL_H_ 1

/* from util */
#include <dlist.h>
#include <rec_dlist.h>

/* from venus */
#include "fso.h"
#include "venusvol.h"
#include <lwp/lock.h>

/* forward decl. */
class rfment;
class lgment;

extern void LRInit();								/*U*/
/* lrdb daemon */
extern void LRD_Init(void);
void LRDBDaemon(void);/*N*/ /* used to be member of class lrdb (Satya 3/31/95) */


#define LRDB	(rvg->recov_LRDB)
#ifndef MAXNAMELEN
#define MAXNAMELEN 255
#endif

/* lrdb - the definition of lrdb (local repair database) */
/* 
 * class lrdb (local-repair(representation)-data-base) contains all the
 * global information that are needed for creating and maintaining
 * representaions of local subtrees created by disconnected mutation in the
 * face of reintegration failure, and used by the repair tool for
 * reconciliation of the local state with the global state. Note: For data
 * fields, letter P indicates the data field is to be maintained as a
 * persistent entity while letter T indicates the data field is transient. For
 * methods, letter T indicates the method must be called from within a
 * transaction, letter U means the method must not be called from within a
 * transaction and letter N means that the method need not be called from
 * within a transaction. 
 */
class lrdb {
    struct Lock rfm_lock;		/* for synchronization on root_fid_map list */
private:
    int do_FindRepairObject(ViceFid *, fsobj **, fsobj **);

public:
    void *operator new(size_t);							/*T*/
    lrdb();									/*T*/
    ~lrdb();									/*T*/
    void operator delete(void *, size_t);
    void ResetTransient();							/*N*/
    /* below are data fields used for local subtree representation */
    rec_dlist local_global_map;	/*P*//* list of (local fid, global fid) map entry */
/*
  BEGIN_HTML
  <a name="rfmlist"><strong> root_fid_map stores a list of fake-joint </strong></a> 
  END_HTML
*/
    rec_dlist root_fid_map;	/*P*//* ist of map entries for local subtree root info */
    dlist dir_list;		/*T*//* temp list of dir-entries(processing uncached children) */
    int local_fid_unique_gen;	/*P*//* local fid uniquifier generator */

    /* below are methods for creating local subtree representation */
    ViceFid *LGM_LookupLocal(ViceFid *);					/*N*/
    ViceFid *LGM_LookupGlobal(ViceFid *);					/*N*/
    void LGM_Insert(ViceFid *, ViceFid *);					/*T*/
    void LGM_Remove(ViceFid *, ViceFid *);					/*T*/
    ViceFid *RFM_LookupGlobalRoot(ViceFid *);					/*N*/
    ViceFid *RFM_LookupLocalRoot(ViceFid *);					/*N*/
    ViceFid *RFM_LookupRootParent(ViceFid *);					/*N*/
    ViceFid *RFM_LookupGlobalChild(ViceFid *);					/*N*/
    ViceFid *RFM_LookupLocalChild(ViceFid *);					/*N*/
    fsobj *RFM_LookupRootMtPt(ViceFid *);					/*N*/
    ViceFid *RFM_ParentToFakeRoot(ViceFid *);					/*N*/
    ViceFid *RFM_FakeRootToParent(ViceFid *);					/*N*/
    void RFM_CoverRoot(ViceFid *);						/*N*/
    int RFM_IsRootParent(ViceFid *);						/*N*/
    int RFM_IsFakeRoot(ViceFid *);						/*N*/
    int RFM_IsGlobalRoot(ViceFid *);						/*N*/
    int RFM_IsGlobalChild(ViceFid *);						/*N*/
    int RFM_IsLocalRoot(ViceFid *);						/*N*/
    int RFM_IsLocalChild(ViceFid *);						/*N*/
    void RFM_Remove(ViceFid *);							/*T*/
    void RFM_Insert(ViceFid *, ViceFid *, ViceFid *, ViceFid *,
		    ViceFid*, ViceFid *, char *);				/*T*/
    ViceFid GenerateLocalFakeFid(ViceDataType);					/*T*/
    ViceFid GenerateFakeLocalFid();						/*T*/
    void TranslateFid(ViceFid *, ViceFid *);					/*T*/
    void PurgeRootFids();							/*N*/
    void DirList_Clear();							/*N*/
    void DirList_Insert(VolumeId, VnodeId, Unique_t, char *);			/*N*/
    void DirList_Process(fsobj *);						/*U*/    
    int InRepairSession() { return repair_root_fid != NULL; }			/*N*/

/*
  BEGIN_HTML
  <a name="state"> <strong> the following fields represent the key
  information in the current repair session </strong></a>
  END_HTML
*/
    /* below are data fields for repairing local subtrees */
    ViceFid *repair_root_fid;	/*T*//* subtree root fid of current repair session */
    cmlent *current_search_cml;	/*T*//* current search cmlent of current repair session */
    char subtree_view;		/*T*//* current view of the subtree being repaired */
    char repair_session_mode;	/*T*//* indicate whether in scratch mode or direct mode */
    int repair_session_tid;	/*T*//* current cmlent::tid of current repair session */
    int repair_tid_gen;		/*P*//* repair session cmlent::tid generator */
    dlist repair_obj_list;	/*T*//* list of fsobj-ptrs of involved local objects */
    dlist repair_vol_list;	/*T*//* list of volent-ptrs of involved volumes */
    dlist repair_cml_list;	/*T*//* list of cmlent-ptrs of involved mutations */

    /* below are methods for repair local subtrees */
    void BeginRepairSession(ViceFid *, int, char *);				/*U*/
    void EndRepairSession(int, char *);						/*U*/
    void ContinueRepairSession(char *);						/*U*/
    void DiscardLocalMutation(char *);					        /*N*/
    void DiscardAllLocalMutation(char *);				        /*N*/
    void PreserveLocalMutation(char *);						/*N*/
    void PreserveAllLocalMutation(char *);					/*N*/
    void InitCMLSearch(ViceFid *);						/*U*/
    void ListCML(ViceFid *, FILE *);						/*U*/
    void AdvanceCMLSearch();							/*N*/
    void DeLocalization();							/*U*/
    int FindRepairObject(ViceFid *, fsobj **, fsobj **);			/*N*/
    fsobj *GetGlobalParentObj(ViceFid *);					/*N*/
    char GetSubtreeView();							/*N*/
    void SetSubtreeView(char, char *);						/*U*/
    void ReplaceRepairFid(ViceFid *, ViceFid *);				/*U*/
    void CheckLocalSubtree();							/*N*/
    void RemoveSubtree(ViceFid *);						/*U*/
    int GetRepairSessionTid() { return repair_session_tid; }		

    /* below are debugging methods */
    void print(FILE *);							      	/*N*/
    void print(int);								/*N*/
    void print();								/*N*/
};

/* lgment - the entry of the local-global fid-map is defined in lgment */
/* class for logal-global-map entry */
class lgment : public rec_dlink {
    ViceFid local;				/*P*/
    ViceFid global;				/*P*/
public:
    void *operator new(size_t);			/*T*/
    lgment(ViceFid *, ViceFid *);		/*T*/
    ~lgment();					/*T*/
    void operator delete(void *, size_t);	/*T*/
    ViceFid *GetLocalFid();			/*N*/
    ViceFid *GetGlobalFid();			/*N*/
    void SetLocalFid(ViceFid *);		/*T*/
    void SetGlobalFid(ViceFid *);		/*T*/

    void print(FILE *);				/*N*/
    void print(int);				/*N*/
    void print();				/*N*/
};

class lgm_iterator : public rec_dlist_iterator {
public:
    lgm_iterator(rec_dlist&);
    lgment *operator()();
};

/*
  BEGIN_HTML
  <a name="fakejoint"><strong> rfment defines the class that represents the fake-joint 
  of the representation of a local-global conflict </strong></a> 
  END_HTML
*/ 

/* 
 * class rfment defines information in a root-fid-map entry.
 */
class rfment : public rec_dlink {
    char *name;					/*P*/
    ViceFid fake_root_fid;			/*P*/
    ViceFid global_root_fid;			/*P*/
    ViceFid local_root_fid;			/*P*/
    ViceFid root_parent_fid;			/*P*/
    ViceFid local_child_fid;			/*P*/
    ViceFid global_child_fid;			/*P*/
    unsigned short covered;			/*P*/
    char view;					/*P*/
    fsobj *root_mtpt;				/*P*/
public:
    void *operator new(size_t);			/*T*/
    rfment(ViceFid *, ViceFid *, ViceFid *, ViceFid *, 
	   ViceFid *, ViceFid *, char *);	/*T*/
    ~rfment();					/*T*/
    void operator delete(void *, size_t);	/*T*/

  /* For a description of the layout, please refer to Lu's thesis, page 82 */

    ViceFid *GetFakeRootFid();			/*N*/
    ViceFid *GetGlobalRootFid();		/*N*/
    ViceFid *GetLocalRootFid();			/*N*/
    ViceFid *GetRootParentFid();		/*N*/
    ViceFid *GetGlobalChildFid();		/*N*/
    ViceFid *GetLocalChildFid();		/*N*/
    char *GetName();				/*N*/
    void CoverRoot();				/*T*/
    unsigned short RootCovered();		/*N*/
    void SetView(char);				/*T*/
    char GetView();				/*N*/
    int IsVolRoot();				/*N*/
    fsobj *GetRootMtPt();			/*N*/
    void SetRootMtPt(fsobj *);			/*T*/

    void print(FILE *);				/*N*/
    void print(int);				/*N*/
    void print();				/*N*/
};

class rfm_iterator : public rec_dlist_iterator {
public:
    rfm_iterator(rec_dlist&);
    rfment *operator()();
};

/* class for a dir entry used for process uncached children
   (Satya, 8/12/96): had to change the name from dirent to
   vdirent to prevent name clash with sys/dirent.h in BSD44
*/
class vdirent : public dlink {
    ViceFid fid;
    char name[MAXNAMELEN];
public:
    vdirent(VolumeId, VnodeId, Unique_t, char *);
    ~vdirent();
    ViceFid *GetFid();
    char *GetName();
    
    void print(FILE *);
    void print();
    void print(int);
};

class dir_iterator : public dlist_iterator {
public:
    dir_iterator(dlist&);
    vdirent *operator()();
};

/* class for fsobj object-pointer */
class optent : public dlink {
    fsobj *obj;
    int tag;
public:
    optent(fsobj *);
    ~optent();
    fsobj *GetFso();
    void SetTag(int);
    int GetTag();
    
    void print(FILE *);
    void print();
    void print(int);
};

class opt_iterator : public dlist_iterator {
public:	
    opt_iterator(dlist&);
    optent *operator()();
};


/* class for repvol object-pointer */
class vptent : public dlink {
    repvol *vpt;
public:
    vptent(repvol *);
    ~vptent();    
    repvol *GetVol();

    void print(FILE *);
    void print();
    void print(int);
};

class vpt_iterator : public dlist_iterator {
public:	
    vpt_iterator(dlist&);
    vptent *operator()();
};

/* class for cmlent object-pointer */
class mptent : public dlink {
    cmlent *cml;
public:
    mptent(cmlent *);
    ~mptent();
    cmlent *GetCml();

    void print(FILE *);
    void print();
    void print(int);
};

class mpt_iterator : public dlist_iterator {
public:	
    mpt_iterator(dlist&);
    mptent *operator()();
};

/* constants for repair subtree views */
#define	SUBTREE_GLOBAL_VIEW	1
#define SUBTREE_LOCAL_VIEW	2
#define SUBTREE_MIXED_VIEW	4

/* 
 * constants for local mutation integrity check.
 * VV_CONFLICT: means version vector conflict.
 * NN_CONFLICT: means name/name conflict.
 * RU_CONFLICT: means remove(client)/update(server) conflict.
 */
#define	MUTATION_MISS_TARGET	0x1
#define MUTATION_MISS_PARENT	0x2
#define MUTATION_ACL_FAILURE	0x4
#define MUTATION_VV_CONFLICT	0x8
#define MUTATION_NN_CONFLICT	0x10
#define MUTATION_RU_CONFLICT	0x20

/* constants for local repair option */
#define REPAIR_FAILURE		0x1
#define REPAIR_OVER_WRITE	0x2
#define REPAIR_FORCE_REMOVE	0x4

/* constants for repair session mode */
#define REP_SCRATCH_MODE	0x1
#define REP_DIRECT_MODE		0x2

/* constant for the initial value of repair transaction-id number generator */
#define	REP_INIT_TID		1000000

/* object-based debug macro */
#define	OBJ_ASSERT(o, ex) \
{\
    if (!(ex)) {\
       (o)->print(logFile);\
       CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
    }\
}


#endif _LOCAL_H_
