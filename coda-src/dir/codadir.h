/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef CODA_DIR_H_
#define CODA_DIR_H_ 1
#include <lwp/lock.h>
#include <sys/types.h>
#include <time.h>
#include <coda.h>
#include <vcrcommon.h>
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
	int df_vnode;	/* file's vnode slot */
	int df_unique;	/* the slot incarnation number */
};

/* File identifier in network order */
struct DirNFid {
	int dnf_vnode;	/* file's vnode slot */
	int dnf_unique;	/* the slot incarnation number */
};

/* A directory entry */
struct DirEntry    {
    char flag;
    char length;	/* currently unused */
    short next;
    struct DirNFid fid;
    char name[16];   /* 16 is deceiving; actual name[] arrays
			have extra blobs appended to make name[]
			as long as needed  (Satya, May 04) */
			
};

int DIR_Init(int data_loc);

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
int DH_Convert(PDirHandle dh, char *file, VolumeId vol, RealmId realm);
int DH_Create(PDirHandle dh, char *entry, struct ViceFid *vfid);
int DH_IsEmpty(PDirHandle dh);
int DH_Lookup(PDirHandle dh, char *entry, struct ViceFid *vfid, int flags);
char * DH_FindName(PDirHandle dh, struct DirFid *fid, char *name, int len);
int DH_LookupByFid(PDirHandle dh, char *entry, struct ViceFid *vfid);
int DH_Delete(PDirHandle dh, char *entry);
int DH_DirOK(PDirHandle dh);
void DH_Free(PDirHandle dh, int in_rvm);
void DH_Print(PDirHandle dh, FILE *f);
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
int FID_IsVolRoot(const struct ViceFid *fid);
void FID_MakeRoot(struct ViceFid *fid);



/* check if this is a local directory or file */
int FID_IsDisco(const struct ViceFid *x);
int FID_IsLocalDir(const struct ViceFid *fid);
int FID_IsLocalFile(const struct ViceFid *fid);
void FID_MakeDiscoFile(struct ViceFid *fid, VolumeId vid, 
			      Unique_t unique);
void FID_MakeDiscoDir(struct ViceFid *fid, VolumeId vid,
			     Unique_t unique);

/* directory vnode number for dangling links during conflicts - 
   two versions, one for the remote copy one for the local oopy*/

/* make the root of a repair subtree residing on the server */
int FID_IsFakeRoot(struct ViceFid *fid);
void FID_MakeSubtreeRoot(struct ViceFid *fid, VolumeId vid, 
				Unique_t unique);
/* fill fids residing in the local tree */
void FID_MakeLocalDir(struct ViceFid *fid, Unique_t unique);
void FID_MakeLocalFile(struct ViceFid *fid, Unique_t unique);
void FID_MakeLocalSubtreeRoot(struct ViceFid *fid, Unique_t unique);

/* compare parts of fids */
int FID_EQ(const struct ViceFid *fa, const struct ViceFid *fb);
int FID_VolEQ(const struct ViceFid *fa, const struct ViceFid *fb);
int FID_Cmp(const struct ViceFid *, const struct ViceFid *);

/*  copy or transform parts of fids */
void FID_CpyVol(struct ViceFid *target, const struct ViceFid *source);
void FID_VFid2DFid(const struct ViceFid *vf, struct DirFid *df);
void FID_DFid2VFid(const struct DirFid *df, struct ViceFid *vf);
void FID_PrintFid(const struct DirFid *fid);
void FID_Int2DFid(struct DirFid *fid, const int vnode, const int unique);
void FID_NFid2Int(const struct DirNFid *fid, VnodeId *vnode, Unique_t *unique);

/* print fids */
char *FID_(const struct ViceFid *fid);

/* extern definitions for dirbody.c */
int DIR_init(int);
int DIR_Compare (PDirHeader, PDirHeader);
int DIR_Length(PDirHeader);
void DIR_Print(PDirHeader, FILE *f);
#define DIR_intrans()  DIR_check_trans(__FUNCTION__, __FILE__)
void DIR_check_trans(const char *where, const char *file);
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
void DC_SetRefcount(PDCEntry pdc, int count);

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
PDCEntry DC_DH2DC(PDirHandle pdh);
void DC_Rehash(PDCEntry);
void DC_HashInit();


#endif /* _DIR_H_ */
