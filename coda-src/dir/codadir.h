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

#endif /*_BLURB_*/





#ifndef CODA_DIR_H_
#define CODA_DIR_H_ 1
#include <lock.h>
#include <cfs/coda.h>
#include <dllist.h>

/* bytes per page */
#define DIR_PAGESIZE 2048	

/* maximum pages of a directory */
#define DIR_MAXPAGES  128

/* where is directory data */
#define DIR_DATA_IN_RVM 1
#define DIR_DATA_IN_VM  0

typedef struct DirEntry *PDirEntry;
typedef struct DirFid *PDirFid;
typedef struct DirHeader *PDirHeader;
typedef struct DirHandle *PDirHandle;
typedef struct DCEntry *PDCEntry;


struct DirHandle {
	Lock              dh_lock;
	PDirHeader        dh_data;
	int               dh_dirty;  /* used by server only */
};
/* A file identifier in host order. */
struct DirFid {
	long df_vnode;	/* file's vnode slot */
	long df_unique;	/* the slot incarnation number */
};

/* File identifier in network order */
struct DirNFid {
	long dnf_vnode;	/* file's vnode slot */
	long dnf_unique;	/* the slot incarnation number */
};

/* A directory entry */
struct DirEntry    {
    char flag;
    char length;	/* currently unused */
    short next;
    struct DirNFid fid;
    char name[16];
};


/* DH interface */
void DH_LockW(PDirHandle dh);
void DH_LockR(PDirHandle dh);
void DH_UnLockW(PDirHandle dh);
void DH_UnLockR(PDirHandle dh);
void DH_Init(PDirHandle dh);
void DH_Alloc(PDirHandle dh, int size, int in_rvm);
PDirHeader DH_Data(PDirHandle dh);
void DH_Free(PDirHandle dh, int in_rvm);
void DH_FreeData(PDirHandle dh);
PDirHandle DH_New(int in_rvm, PDirHeader vmdata, PDirHeader rvmdata);
int DH_Length(PDirHandle dh);
int DH_Convert(PDirHandle dh, char *file, VolumeId vol);
int DH_Create(PDirHandle dh, char *entry, struct ViceFid *vfid);
int DH_IsEmpty(PDirHandle dh);
int DH_Lookup(PDirHandle dh, char *entry, struct ViceFid *vfid);
char * DH_FindName(PDirHandle dh, struct DirFid *fid, char *name, int len);
int DH_LookupByFid(PDirHandle dh, char *entry, struct ViceFid *vfid);
int DH_Delete(PDirHandle dh, char *entry);
int DH_DirOK(PDirHandle dh);
void DH_Free(PDirHandle dh, int in_rvm);
void DH_Print(PDirHandle dh);
void DH_PrintStats(FILE *fp);
int DH_MakeDir(PDirHandle dh, struct ViceFid *vme, struct ViceFid *vparent);
int DH_EnumerateDir(PDirHandle dh, int (*hookproc)(struct DirEntry *de, void *hook), void *hook);
int DH_Commit(PDirHandle dh);
void DH_Get(PDirHandle, PDirHeader);
void DH_Put(PDirHandle);
void DH_Init(PDirHandle dh);

/* fid support */

#define	ISDIR(fid)  ((fid).Vnode & 1)	     /* Directory fids are odd */

#define FID_LT(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) < ((b).Unique)))

#define FID_LTE(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) <= ((b).Unique)))


/* local fid and local volume related stuff */
inline int FID_IsVolRoot(struct ViceFid *fid);
inline void FID_MakeRoot(struct ViceFid *fid);



/* check if this is a local directory or file */
inline int FID_IsDisco(struct ViceFid *x);
inline int FID_IsLocalDir(struct ViceFid *fid);
inline int FID_IsLocalFile(struct ViceFid *fid);
inline void FID_MakeDiscoFile(struct ViceFid *fid, VolumeId vid, 
			      Unique_t unique);
inline void FID_MakeDiscoDir(struct ViceFid *fid, VolumeId vid,
			     Unique_t unique);

/* directory vnode number for dangling links during conflicts - 
   two versions, one for the remote copy one for the local oopy*/

/* make the root of a repair subtree residing on the server */
inline int FID_IsFakeRoot(struct ViceFid *fid);
inline void FID_MakeSubtreeRoot(struct ViceFid *fid, VolumeId vid, 
				Unique_t unique);
/* fill fids residing in the local tree */
inline void FID_MakeLocalDir(struct ViceFid *fid, Unique_t unique);
inline void FID_MakeLocalFile(struct ViceFid *fid, Unique_t unique);
inline void FID_MakeLocalSubtreeRoot(struct ViceFid *fid, Unique_t unique);

/* check if the volume is local */
inline void FID_MakeVolFake(VolumeId *id);
inline int  FID_VolIsLocal(struct ViceFid *x);
inline int FID_VolIsFake(VolumeId id);

/* compare parts of fids */
int FID_EQ(struct ViceFid *fa, struct ViceFid *fb);
int FID_VolEQ(struct ViceFid *fa, struct ViceFid *fb);
int FID_Cmp(struct ViceFid *, struct ViceFid *);

/*  copy or transform parts of fids */
void FID_CpyVol(struct ViceFid *target, struct ViceFid *source);
void FID_VFid2DFid(struct ViceFid *vf, struct DirFid *df);
void FID_DFid2VFid(struct DirFid *df, struct ViceFid *vf);
void FID_PrintFid(struct DirFid *fid);
void FID_Int2DFid(struct DirFid *fid, int vnode, int unique);
void FID_NFid2Int(struct DirNFid *fid, VnodeId *vnode, Unique_t *unique);

/* print fids */
char *FID_(struct ViceFid *);
char *FID_2(struct ViceFid *);


/* extern definitions for dirbody.c */
int DIR_init(int);
int DIR_Compare (PDirHeader, PDirHeader);
int DIR_Length(PDirHeader);
#define DIR_intrans()  DIR_check_trans(__FUNCTION__, __FILE__)
inline void DIR_check_trans(char *where, char *file);
struct PageHeader *DIR_Page(struct DirHeader *dirh, int page);


/* Directory Inode interface */
struct DirInode {
	void *di_pages[DIR_MAXPAGES];
	int  di_refcount;             /* for copy on write */
};
typedef struct DirInode *PDirInode;

void DC_SetDI(PDCEntry pdce, PDirInode pdi);
PDirHeader DI_DiToDh(PDirInode pdi);
void DI_DhToDi(PDCEntry pdce);
void DI_Copy(PDirInode oldinode, PDirInode *newinode);
void DI_Dec(PDirInode pdi);
void DI_Inc(PDirInode pdi);
int DI_Count(PDirInode);
int DI_Pages(PDirInode);
void *DI_Page(PDirInode, int);
void DI_VMCopy(PDirInode oldinode, PDirInode *newinode);
void DI_VMFree(PDirInode pdi);
void DI_VMDec(PDirInode pdi);
void DC_SetCount(PDCEntry pdce, int count);

/* dir handle cache */
PDirInode DC_DC2DI(PDCEntry pdce);
int DC_Refcount(PDCEntry);
void DC_Put(PDCEntry);
void DC_Drop(PDCEntry);
int DC_Count(PDCEntry pdce) ;
void DC_SetDirh(PDCEntry pdce, PDirHeader pdh);
PDirInode DC_Cowpdi(PDCEntry);
void DC_SetCowpdi(PDCEntry, PDirInode);
int DC_Dirty(PDCEntry);
void DC_SetDirty(PDCEntry, int);
PDCEntry DC_Get(PDirInode);
PDCEntry DC_New();
PDirHandle DC_DC2DH(PDCEntry);
void DC_Rehash(PDCEntry);
void DC_HashInit();


#endif _DIR_H_
