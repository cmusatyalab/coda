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
 * resutil.h
 */
#ifndef _RESUTIL_H_
#define _RESUTIL_H_ 1

#include <res.h>


#define ISNONRESOLVEOP(a)	(((a) < RESOLVE_OPERATION_BASE) || \
				 ((a) == ResolveNULL_OP))
#define FormFid(fid, vol, vn, un)	\
{(fid).Volume = (vol); (fid).Vnode = (vn); (fid).Unique= (un);}


#define MAXCOMPS	100	// defined by Kudo in rp2gen - 
                                // this is the max size of an array  that can 
                                // be passed as arg in a rpc call 

/**************** ilink class functions *******************/
/* declaration of functions that parse the list of inconsistencies 
 * at end of phase 1 during resolution and create a unique entry list
 */
#define SIZEOF_INCFID	5 * sizeof(long)

#include "dlist.h" 

struct ilink : public dlink {
    char *name;
    long vnode;
    long unique;
    long pvnode;
    long punique;
    long type;

    ilink(char *c, long vn, long unq, long pvn, long punq, long t) {
	name = (char *)malloc(strlen(c) + 1);
	strcpy(name, c);
	vnode = vn;
	unique = unq;
	pvnode = pvn;
	punique = punq;
	type = t;
    }
    ~ilink() {
	if (name)
	    free(name);
    }
};

/* *********** respath class functions ************** */
/* respath is used to pass info to the coordinator about 
   the state of all vnodes on the path from the volume root
*/
struct respath : public olink {
    long vnode;
    long unique;
    ViceVersionVector vv;
    // ViceStoreId sid;	when directories have only store ids.
    ResStatus st;

    respath(long vn, long unq, ViceVersionVector *v, ResStatus *s) {
	vnode = vn;
	unique = unq;
	vv = *v;
	st = *s;
    }

    ~respath() {

    }
    init(long vn, long unq, ViceVersionVector *v, ResStatus *s) {
	vnode = vn;
	unique = unq;
	vv = *v;
	st = *s;
	return(0); /* keep C++ 3.0 happy */
    }
};

extern void GetResStatus(unsigned long *, ResStatus **, ViceStatus *);
extern void AllocStoreId(ViceStoreId *);
extern long CheckRetCodes(unsigned long *, unsigned long *, unsigned long *);
extern long CheckResRetCodes(unsigned long *, unsigned long *, unsigned long *);
extern void BSToDlist(RPC2_BoundedBS *, dlist *);
extern void DlistToBS(dlist *, RPC2_BoundedBS *);
extern void ParseIncBSEntry(char **, char **, long *, long *, long *, long *, 
			    long *);
extern void AllocIncBSEntry(RPC2_BoundedBS *, char *, ViceFid *, 
			    ViceFid *, long);
extern int CompareIlinkEntry(ilink *, ilink *);
extern ilink *AddILE(dlist &, char *, long, long, long, long, long);
extern void CleanIncList(dlist *);
extern void MarkObjInc(ViceFid *, Vnode *);
extern int CreateObjToMarkInc(Volume *, ViceFid *, ViceFid *, char *, int, dlist *,int*);
extern long GetPath(ViceFid *, int, int *, ResPathElem *);
class res_mgrpent;
extern int ComparePath(int *, ResPathElem **, res_mgrpent *, int *, ViceFid *, ViceVersionVector **, ResStatus **);
void ObtainResStatus(ResStatus *, VnodeDiskObjectStruct *);


extern int GetPhase2Objects(ViceFid *, dlist *, dlist *, Volume **);
extern int CreateResPhase2Objects(ViceFid *, dlist *, dlist *, Volume *, 
				   VolumeId, int *);
extern void GetRemoteRemoveStoreId(ViceStoreId *, olist *, unsigned long, ViceFid *, ViceFid *, char *);
extern ViceStoreId *GetRemoteRemoveStoreId(olist *, unsigned long , ViceFid *, ViceFid *,  char*);
extern int GetNameInParent(Vnode *, dlist *, Volume *, char *);

/* declarations from rescoord.c */
extern long DirResolve(res_mgrpent *, ViceFid *, ViceVersionVector **, int *);
extern long OldDirResolve(res_mgrpent *, ViceFid *, ViceVersionVector **);
extern int IsWeaklyEqual(ViceVersionVector **, int);


/* declarations from weres.c */
extern int WERes(ViceFid *, ViceVersionVector **, ResStatus **,
		 res_mgrpent *, unsigned long *);

#endif not _RESUTIL_H_
