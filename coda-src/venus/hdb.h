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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/hdb.h,v 4.2 97/12/16 16:08:30 braam Exp $";
#endif /*_BLURB_*/







/*
 *  Hoard database management: first part used by Venus & hoard, latter only by Venus
 */

#ifndef _VENUS_HDB_H_
#define _VENUS_HDB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

#define HDB_ASSERT(ex) \
{\
   if (!(ex)) {\
     Choke("Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
   }\
}


/* Hoard priority range. */
#define	H_MAX_PRI	1000
#define	H_DFLT_PRI	10
#define	H_MIN_PRI	1

/* Hoard attribute flags. */
#define	H_INHERIT	1	/* New children inherit their parent's hoard status. */
#define	H_CHILDREN	2	/* Meta-expand directory to include its children. */
#define	H_DESCENDENTS	4	/* Meta-expand directory to include all its descendents. */
#define	H_DFLT_ATTRS	0


/* *****  Definition of pioctl message structures.  ***** */

/* Should be in venusioctl.h! -JJK */


struct hdb_clear_msg {
    vuid_t  cuid;
    vuid_t  ruid;
};

struct hdb_add_msg {
    VolumeId volno;
    char name[MAXPATHLEN];
    int priority;
    int attributes;
    vuid_t ruid;
};

struct hdb_delete_msg {
    VolumeId volno;
    char name[MAXPATHLEN];
    vuid_t ruid;
};

struct hdb_list_msg {
    char outfile[MAXPATHLEN];
    vuid_t luid;
    vuid_t ruid;
};

struct hdb_walk_msg {
    vuid_t ruid;
};

struct hdb_verify_msg {
    char outfile[MAXPATHLEN];
    vuid_t luid;
    vuid_t ruid;
    int   verbosity;
};



/*  ****************************************  */


#ifdef	VENUS  /* Portion below here not used in vtools/hoard.c */


/* from util */
#include <bstree.h>
#include <olist.h>
#include <rec_olist.h>
#include <ohash.h>
#include <rec_ohash.h>

/* from venus */
#include "fso.h"
#include "venusrecov.h"
#include "venus.private.h"


/*
 *    Allocated HDB entries are linked into a hash table, keyed by <vid, name>.
 *    Unallocated entries are linked into a free-list.
 *
 *    A transient "name-context" is associated with each allocated entry.
 *    A name-context may be linked into one of two priority queues, depending on its state.
 *    There are "suspect" and "indigent" queues for name-context's of those
 *    two states.  Name-context's in the third state, "valid," are not in a priority queue.
 *    An HDB entry which has been meta-expanded will have additional name-contexts
 *    associated with it.
 *
 */


/* Forward declarations. */
class hdb;
class hdb_key;
class hdbent;
class hdb_iterator;
class namectxt;


/*  *****  Constants  *****  */

#define	HDB	(rvg->recov_HDB)
const int HDB_MagicNumber = 5551212;
const int HDB_NBUCKETS = 2048;
const int HDBENT_MagicNumber = 8204933;
const int HDBMaxFreeEntries = 32;

const int BLOCKS_PER_HDBE = 16;
const int DFLT_HDBE = DFLT_CB / BLOCKS_PER_HDBE;
const int UNSET_HDBE = -1;
const int MIN_HDBE = MIN_CB / BLOCKS_PER_HDBE;


/*  *****  Types  *****  */

enum hdbd_request { HdbAdd,
		    HdbDelete,
		    HdbClear,
		    HdbList,
		    HdbWalk,
		    HdbVerify,
		    HdbEnable,
		    HdbDisable
};

/*
 * Allow user to specify if periodic hoard walks should happen. I put this outside
 * of the hdb class to avoid the necessity of a reinit. -- DCS 5/5/94
 */

extern char PeriodicWalksAllowed;
  
void HDBDaemon() /* used to be member of class hdb (Satya 3/31/95) */;


class hdb {
  friend void HDB_Init();
  friend void HDBD_Init();
  friend void HDBDaemon();
  friend class hdbent;
  friend class hdb_iterator;
  friend class namectxt;
  friend void RecovInit();

    int MagicNumber;

    /* Size parameters. */
    int MaxHDBEs;

    /* The table. */
    rec_ohashtab htab;

    /* The free list. */
    rec_olist freelist;

    /* The priority queue. */
    /*T*/bstree	*prioq;

    /* Advice Information */
    long TimeOfLastDemandWalk;
    int NumHoardWalkAdvice;
    int SolicitAdvice;

    /* Constructors, destructors. */
    void *operator new(size_t);
    void operator delete(void *, size_t);
    hdb();
    void ResetTransient();
    ~hdb() { abort(); }

    /* Allocation/Deallocation routines. */
    hdbent *Create(VolumeId, char *, vuid_t, int, int, int);


  public:
    hdbent *Find(VolumeId, char *);

    /* The external interface. */
    int Add(hdb_add_msg *);
    int Delete(hdb_delete_msg *);
    int Clear(hdb_clear_msg *);
    int List(hdb_list_msg *);
    int Walk(hdb_walk_msg *);
    int Verify(hdb_verify_msg *);
    int Enable(hdb_walk_msg *);
    int Disable(hdb_walk_msg *);
     
    void ResetUser(vuid_t);

    /* Helper Routines hdb::Walk */
    void ValidateCacheStatus(vproc *, int *, int *);
    void ListPriorityQueue();
    int GetSuspectPriority(int, char *, int);
    void WalkPriorityQueue(vproc *, int *, int *);
    int CalculateTotalBytesToFetch();
    void StatusWalk(vproc *, int *);
    void DataWalk(vproc *, int);
    void PostWalkStatus();

    /* Advice Related*/
    void SetSolicitAdvice(int uid)
        { LOG(0, ("SetSolicitAdvice: uid=%d\n",uid));
	  SolicitAdvice = uid; }
    int MakeAdviceRequestFile(char *);
    void RequestHoardWalkAdvice();

    void SetDemandWalkTime();
    long GetDemandWalkTime();

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int, int =0);
};

class hdb_key {
  public: 
    VolumeId vid;
    char *name;

    hdb_key(VolumeId, char *);
};

class hdbent {
  friend void HDB_Init();
  friend class hdb;
  friend class hdb_iterator;
  friend class fsobj;

    int MagicNumber;

    /* Key. */
    VolumeId vid;
    char *path;

    /* Assoc(key). */
    vuid_t vuid;
    int priority;
    unsigned expand_children : 1;		/* meta-expand children */
    unsigned expand_descendents	: 1;		/* meta-expand descendents */
    unsigned time : 30;				/* currently unused! */
    /*T*/namectxt *nc;				/* pre-computed path expansion */

    /* Linkage. */
    rec_olink tbl_handle;			/* link for {allocated-table, free-list} */

    /* Constructors, destructors. */
    void *operator new(size_t);
    hdbent(VolumeId, char *, vuid_t, int, int, int);
    void ResetTransient();
    ~hdbent();
    void operator delete(void *, size_t);

  public:
    void print() { print(stdout); }
    void print(FILE *fp)  { fflush(fp); print(fileno(fp)); }
    void print(int);
    void printsuspect(int, int);
};

class hdb_iterator : public rec_ohashtab_iterator {
    vuid_t vuid;

  public:
    hdb_iterator();
    hdb_iterator(vuid_t);
    hdb_iterator(hdb_key *);
    hdbent *operator()();
};

enum pestate {	PeValid,		/* expansion need not be checked */
		PeSuspect,		/* expansion must be checked at next walk */
		PeIndigent,		/* expansion is impeded due to resource shortage */
		PeInconsistent		/* expansion is impeded due to inconsistency */
};

class namectxt {
  friend class fsobj;
  friend class hdb;
  friend class hdbent;
  friend class hdb_iterator;
  friend void MetaExpand(long, char *, long, long);
  friend int NC_PriorityFN(bsnode *, bsnode *);
  friend void NotifyUsersOfKillEvent(dlist *, int);

    /* Key. */
    ViceFid cdir;			/* starting directory of expansion */
    char *path;				/* subsequent components */

    /* Assoc(key). */
    vuid_t vuid;			/* owner of this context */
    int	priority;			/* priority to be used for resource allocation */
    enum pestate state;			/* {Valid, Suspect, Indigent} */
    unsigned inuse : 1;			/* state cannot change when inuse */
    unsigned dying : 1;			/* commit suicide when next !inuse */
    unsigned demote_pending : 1;	/* transit to "suspect" state when next !inuse */
    unsigned meta_expanded : 1;		/* this context was created due to meta-expansion */
    unsigned expand_children : 1;	/* meta-expand the children of bound object */
    unsigned expand_descendents	: 1;	/* meta-expand the descendents of bound object */
    unsigned depth : 10;		/* depth of meta-expansion (0 if not meta-expanded) */
    unsigned random : 16;		/* for binary-tree balancing */
    dlist expansion;			/* ordered set of bindings */
    dlist_iterator *next;		/* shadow list for validation */

    /* Expander info. */
    dlist *children;			/* list of expanded children */
    ViceFid expander_fid;		/* Fid of expanded directory */
    ViceVersionVector expander_vv;	/* VersionVector of expanded directory */
    long expander_dv;			/* DataVersion of expanded directory */

    /* Expandee info. */
    namectxt *parent;			/* back pointer to expander */
    dlink child_link;			/* link for expander's children list */

    /* Linkage. */
    bsnode prio_handle;			/* link for HDB priority queues */
    dlink fl_handle;			/* link for freelist */

    /* Private member functions. */
    void hold();			/* inhibit state transitions */
    void release();			/* permit state transitions */
    void Transit(enum pestate);		/* transit to specified state */
    void Kill();			/* delete this context at first opportunity */
    void KillChildren();		/* delete children contexts at first opportunity */
    pestate CheckExpansion();		/* return next state */
    void MetaExpand();

  public:
    void *operator new(size_t);
    namectxt(ViceFid *, char *, vuid_t, int, int, int);
    namectxt(namectxt *, char *);
    namectxt(namectxt&);		/* not supported! */
    operator=(namectxt&);		/* not supported! */
    ~namectxt();
    void operator delete(void *, size_t);

    void Demote(int =0);		/* --> immediate or eventual transition to suspect state */
    void CheckComponent(fsobj *);

    int GetPriority()
        { return(priority); }
    vuid_t GetUid()
        { return(vuid); }

    void print()  { print(stdout); }
    void print(FILE *fp)  { fflush(fp); print(fileno(fp)); }
    void print(int);
    void printsuspect(int, int);
    void getpath(char *);

};

#ifdef	VENUSDEBUG
    /* Too many problems in trying to keep these variables static member of class namectxt */
    extern int NameCtxt_allocs;
    extern int NameCtxt_deallocs;
#endif	VENUSDEBUG



/*  *****  Variables  *****  */

extern int HDBEs;
extern int IndigentCount;


/*  *****  Functions/Procedures  *****  */

/* hdb.c */
extern void HDB_Init();
extern void MetaExpand(long, char *, long, long);
extern int NC_PriorityFN(bsnode *, bsnode *);

/* hdb_daemon.c */
extern void HDBD_Init();
extern int HDBD_Request(hdbd_request, void *, vuid_t, vuid_t);
extern long HDBD_GetNextHoardWalkTime();

#define	PRINT_HDBDREQTYPE(type)\
    ((type) == HdbAdd ? "Add" :\
     (type) == HdbDelete ? "Delete" :\
     (type) == HdbClear ? "Clear" :\
     (type) == HdbList ? "List" :\
     (type) == HdbWalk ? "Walk" :\
     (type) == HdbVerify ? "Verify" :\
     (type) == HdbEnable ? "Enable" :\
     (type) == HdbDisable ? "Disable" :\
     "???")

#define	PRINT_PESTATE(state)\
    ((state) == PeValid ? "Valid" :\
     (state) == PeSuspect ? "Suspect" :\
     (state) == PeIndigent ? "Indigent" :\
     (state) == PeInconsistent ? "Inconsistent" :\
     "???")

#endif	VENUS

#endif	not _VENUS_HDB_H_
