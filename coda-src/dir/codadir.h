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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/dir/codadir.h,v 4.2 1998/08/31 12:23:09 braam Exp $";
#endif /*_BLURB_*/





#ifndef CODA_DIR_H_
#define CODA_DIR_H_ 1
#include <lock.h>
#include <cfs/coda.h>
#include <dllist.h>

/* bytes per page */
#define DIR_PAGESIZE 2048	
#define DIR_MAXPAGES  128
/* where is directory data */
#define DIR_DATA_IN_RVM 1
#define DIR_DATA_IN_VM  0

typedef struct DirEntry *PDirEntry;
typedef struct DirFid *PDirFid;
typedef struct DirHeader *PDirHeader;
typedef struct DirHandle *PDirHandle;
typedef struct DCEntry *PDCEntry;

#if 0
/* moved from vice/file.h to remove circular dependency */
typedef struct ODirHandle {
    /* device+inode+vid are low level disk addressing + validity check */
    /* vid+vnode+unique+cacheCheck are to guarantee validity of cached copy */
    /* ***NOTE*** size of this stucture must not exceed size in buffer
       package (dir/buffer.cc) */
    bit16	device;
    bit16 	cacheCheck;
    Inode	inode;
    VolumeId 	volume;
    Unique_t 	unique;
    VnodeId	vnode;	/* Not really needed; conservative AND
			   protects us against non-unique uniquifiers
			   that were generated in days of old */
} ODirHandle;
#endif


struct DirHandle {
	Lock              dh_lock;
	PDirHeader        dh_vmdata;
	PDirHeader        dh_rvmdata;
	int               dh_refcount;
	int               dh_dirty;
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
/* local fid and local volume related stuff */
static VnodeId ROOT_VNODE = 1;
static Unique_t ROOT_UNIQUE = 1;

#define	ISDIR(fid)  ((fid).Vnode & 1)	     /* Directory fids are odd */

/* to determine if the volume is the local copy during a repair/conflict */
static VolumeId LocalFakeVid = 0xffffffff;
#define FID_IsLocal(x) ((x)->Volume == LocalFakeVid)

/* directory vnode number for dangling links during conflicts*/
static VnodeId FakeVnode = 0xfffffffc;
#define	FID_IsFake(fid) ((fid)->Vnode == FakeVnode)

/* was this fid created during a disconnection */
static VnodeId LocalFileVnode = 0xfffffffe;
static VnodeId LocalDirVnode  = 0xffffffff;
#define FID_IsDisco(x) ( ((x)->Vnode == LocalFileVnode) || ((x)->Vnode == LocalDirVnode))

#define FID_LT(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) < ((b).Unique)))

#define FID_LTE(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) <= ((b).Unique)))
int FID_EQ(struct ViceFid *fa, struct ViceFid *fb);
int FID_VolEQ(struct ViceFid *fa, struct ViceFid *fb);
void FID_CpyVol(struct ViceFid *target, struct ViceFid *source);
void FID_VFid2DFid(struct ViceFid *vf, struct DirFid *df);
void FID_DFid2VFid(struct DirFid *df, struct ViceFid *vf);
void FID_PrintFid(struct DirFid *fid);
void FID_Int2DFid(struct DirFid *fid, int vnode, int unique);
void FID_NFid2Int(struct DirNFid *fid, VnodeId *vnode, Unique_t *unique);
int FID_Cmp(struct ViceFid *, struct ViceFid *);
char *FID_(struct ViceFid *);
char *FID_2(struct ViceFid *);


/* extern definitions for dir.c */
#define DIR_intrans()  DIR_check_trans(__FUNCTION__, __FILE__)
inline void DIR_check_trans(char *where, char *file);
extern int DIR_init(int);
extern void DIR_Free(struct DirHeader *, int);
extern int DirHash (char *);
extern int DirToNetBuf(long *, char *, int, int *);
void DIR_CpyVol(struct ViceFid *target, struct ViceFid *source);
int DIR_MakeDir(struct DirHeader **dir, struct DirFid *me, struct DirFid *parent);
int DIR_LookupByFid(PDirHeader dhp, char *name, struct DirFid *fid);
int DIR_Lookup(struct DirHeader *dir, char *entry, struct DirFid *fid);
int DIR_EnumerateDir(struct DirHeader *dhp, 
		     int (*hookproc)(struct DirEntry *de, void *hook), void *hook);
int DIR_Create(struct DirHeader **dh, char *entry, struct DirFid *fid);
int DIR_Length(struct DirHeader *dir);
int DIR_Delete(struct DirHeader *dir, char *entry);
int DIR_Init(int data_loc);
void DIR_PrintChain(PDirHeader dir, int chain);
int DIR_Hash (char *string);
int DIR_DirOK (PDirHeader pdh);
int DIR_Convert (PDirHeader dir, char *file, VolumeId vol);
int DIR_Compare (PDirHeader, PDirHeader);
struct PageHeader *DIR_Page(struct DirHeader *dirh, int page);


/* Directory Inode interface */
struct DirInode {
	void *di_pages[DIR_MAXPAGES];
	int  di_refcount;             /* for copy on write */
};
typedef struct DirInode *PDirInode;

PDirHeader DI_DiToDh(PDirInode pdi);
PDirInode DI_DhToDi(PDCEntry pdce, PDirInode pdi);
void DI_Copy(PDirInode oldinode, PDirInode *newinode);
void DI_Dec(PDirInode pdi);
void DI_Inc(PDirInode pdi);
int DI_Count(PDirInode);
int DI_Pages(PDirInode);
void *DI_Page(PDirInode, int);
void DI_VMCopy(PDirInode oldinode, PDirInode *newinode);
void DI_VMFree(PDirInode pdi);
void DI_VMDec(PDirInode pdi);

/* dir handle cache */

int DC_Refcount(PDCEntry);
void DC_Put(PDCEntry);
void DC_Drop(PDCEntry);
void DC_SetDirh(PDCEntry pdce, PDirHeader pdh);
PDirInode DC_Cowpdi(PDCEntry);
void DC_SetCowpdi(PDCEntry, PDirInode);
int DC_Dirty(PDCEntry);
void DC_SetDirty(PDCEntry, int);
PDCEntry DC_Get(PDirInode);
PDCEntry DC_New();
PDirHandle DC_DC2DH(PDCEntry);
void DC_Commit(PDCEntry);
void DC_HashInit();


/* old stuff */
extern int DirOK (long *);
extern int DirSalvage (long *, long *);
extern void DStat (int *, int *, int *);
extern int DInit (int );
extern void DFlushEntry (long *);

#endif _DIR_H_
