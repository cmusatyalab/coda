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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/venus/RCS/fso_dir.cc,v 4.2 1997/02/18 15:28:25 lily Exp $";
#endif /*_BLURB_*/








/*
 *
 * Implementation of the Venus File-System Object (fso) Directory subsystem.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#ifdef	__linux__
#include <endian.h>
#include <netinet/in.h>
#else
#include <machine/endian.h>
#endif


#define DIRBLKSIZ       1024

#include "bsd_dir.h"

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from dir */
#include <coda_dir.h>
#include <dir.private.h>

#include "fso.h"
#include "local.h"
#include "simulate.h"
#include "venusrecov.h"
#include "venus.private.h"

/* *****  Convert Directory  ***** */

struct CVDescriptor {
    int	dirFD;		    /* file descriptor */
    int	dirBytes;	    /* bytes written, to save ftruncate */
    int	dirPos;		    /* position in current block */
    ViceDataType vType;     /* type of directory entry */
};

PRIVATE	int minFreeSize	= -1;	    /* smallest a free block can be */

PRIVATE void CVOpen(char *, CVDescriptor *);
PRIVATE void CVClose(CVDescriptor *);
PRIVATE void CVWriteEntry(char *, ino_t, CVDescriptor *);
PRIVATE void CompleteCVBlock(CVDescriptor *);


/* CVOpen -- open a ``directory'' for writing */
PRIVATE void CVOpen(char *filename, CVDescriptor *cvd) {
    cvd->dirFD = open(filename, O_WRONLY | O_TRUNC, V_MODE);
    if (cvd->dirFD < 0) Choke("CVOpen: open failed (%d)", errno);

    cvd->dirBytes = 0;
    cvd->dirPos = 0;
    if (minFreeSize == -1) {
	struct direct dir; /* equated to struct dirent on BSD44 */
	dir.d_namlen = 1;
	minFreeSize = DIRSIZ(&dir);
    }
}


/* CVClose -- close it when done */
PRIVATE void CVClose(CVDescriptor *cvd) {
    CompleteCVBlock(cvd);

    struct stat tstat;
    if (fstat(cvd->dirFD, &tstat) < 0)
	Choke("CVClose: fstat failed (%d)", errno);
    if (tstat.st_size > cvd->dirBytes)
	if (ftruncate(cvd->dirFD, cvd->dirBytes) < 0)
	    Choke("CVClose: ftruncate failed (%d)", errno);
    if (close(cvd->dirFD) < 0)
	Choke("CVClose: close failed (%d)", errno);
}


/* CVWriteEntry -- add a new name */
PRIVATE void CVWriteEntry(char *name, ino_t inode, CVDescriptor *cvd) {
    if (name == 0 || *name == 0) return;

    struct direct dir;
    dir.d_namlen = strlen(name);

    dir.d_fileno = inode;
    dir.d_type = (u_int8_t) (cvd->vType == Directory ? DT_DIR : 
			     (cvd->vType == File ? DT_REG : 
			      (cvd->vType == SymbolicLink ? DT_LNK : 
			       DT_UNKNOWN)));

    dir.d_reclen = DIRSIZ(&dir);
    strcpy(dir.d_name, name);

    /* If this entry won't fit, put an empty entry in. */
    if (dir.d_reclen + cvd->dirPos > DIRBLKSIZ) CompleteCVBlock(cvd);

    /* If the space left after we write this entry would be too small to hold an empty entry, */
    /* make this entry finish the block. */
    if (dir.d_reclen + cvd->dirPos + minFreeSize > DIRBLKSIZ)
        dir.d_reclen = DIRBLKSIZ - cvd->dirPos;

    if (write(cvd->dirFD, (char *)&dir, dir.d_reclen) != dir.d_reclen)
	Choke("CVWriteEntry: write failed (%d)", errno);
    cvd->dirBytes += dir.d_reclen;
    cvd->dirPos = (cvd->dirPos + dir.d_reclen) % DIRBLKSIZ;
}


/* CompleteCVBlock -- finish out this block */
PRIVATE void CompleteCVBlock(CVDescriptor *cvd) {
    if (DIRBLKSIZ - cvd->dirPos > 0) {
	struct direct dir;

        dir.d_fileno = 0;
#if 0
        dir.d_ino = 0;
#endif
        dir.d_namlen = 0;
        dir.d_reclen = DIRBLKSIZ - cvd->dirPos;
        if (dir.d_reclen < minFreeSize)
            dir.d_reclen += DIRBLKSIZ;
        if (write(cvd->dirFD, (char *)&dir, dir.d_reclen) != dir.d_reclen)
	    Choke("CompleteCVBlock: write failed (%d)", errno);
        cvd->dirBytes += dir.d_reclen;
        cvd->dirPos = 0;
    }
}


/* *****  Auxilliary Routines for Dir Package  ***** */


/* This should be in the dir package! */
/* Look up the first fid in directory with given name. */
int LookupByFid(long *dir, char *name, long *fid) {
    int code = 0;

    struct DirHeader *dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp) { code = ENOENT; goto Exit; }

    int i;
    for (i = 0; i < NHASH; i++) {
	/* For each hash chain, enumerate everyone on the list. */
	int num = ntohs(dhp->hashTable[i]);
	while (num != 0) {
	    /* Walk down the hash table list. */
	    struct DirEntry *ep = GetBlob(dir, num);
	    if (!ep) break;

	    if (fid[1] == ntohl(ep->fid.mkvnode) &&  fid[2] == ntohl(ep->fid.mkvunique)) {
		strcpy(name, ep->name);
		DRelease((buffer *)ep, 0);
		DRelease((buffer *)dhp, 0);
		goto Exit;
	    }


	    num = ntohs(ep->next);
	    DRelease((buffer *)ep, 0);
	}
    }

    DRelease((buffer *)dhp, 0);
    code = ENOENT;

Exit:
    return(code);
}


char *DRead(long *dir, int page) {
    VenusDirData *vdp = (VenusDirData *)dir;
    return(vdp->pages[page]->data);
}


/* MUST be called from within transaction if dirty is TRUE! */
/* Need to ensure that this is only called with bp pointing to the START of a page! */
void DRelease(struct buffer *bp, int dirty) {
    if (dirty) {
	VenusDirPage *vdpp = (VenusDirPage *)bp;

	RVMLIB_REC_OBJECT(*vdpp);
    }
}


/* MUST be called from within transaction! */
char *DNew(long *dir, int page) {
    VenusDirData *vdp = (VenusDirData *)dir;

    /* Make sure that page isn't already allocated! */
    if (vdp->pages[page] != 0)
	Choke("DNew: vdp->pages[page] non-zero (%x, %d, %d)", vdp, page, vdp->pages[page]);

    /* Allocate the page. */
    RVMLIB_REC_OBJECT(vdp->pages[page]);
    vdp->pages[page] = (VenusDirPage *)RVMLIB_REC_MALLOC((int)sizeof(VenusDirPage));

    /* Note that this page was malloced in the bitmap. */
    unsigned int *mapword = &(vdp->mallocbitmap[page / (8 * sizeof(unsigned int))]);
    RVMLIB_REC_OBJECT(*mapword);
    (*mapword) |= (1 << (page % (8 * (int)sizeof(unsigned int))));

    return(vdp->pages[page]->data);
}


/* *****  FSO Directory Interface  ***** */

/* MUST be called from within transaction! */
void fsobj::dir_Create(char *Name, ViceFid *Fid) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Create: (%s, %x.%x.%x) no data", Name, Fid->Volume, Fid->Vnode, Fid->Unique); }

    int oldlength = dir_Length();

    if (::Create((long *)data.dir, Name, (long *)Fid) != 0)
	{ print(logFile); Choke("fsobj::dir_Create: (%s, %x.%x.%x) Create failed!", Name, Fid->Volume, Fid->Vnode, Fid->Unique); }

    data.dir->udcfvalid = 0;

    int newlength = dir_Length();
    int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
    UpdateCacheStats(&FSDB->DirDataStats, CREATE, delta_blocks);
}


int fsobj::dir_Length() {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Length: no data"); }

    if (Simulating)
	return(DIR_SIZE);

    return(::Length((long *)data.dir));
}


/* MUST be called from within transaction! */
void fsobj::dir_Delete(char *Name) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Delete: (%s) no data", Name); }

    int oldlength = dir_Length();

    if (::Delete((long *)data.dir, Name) != 0)
	{ print(logFile); Choke("fsobj::dir_Delete: (%s) Delete failed!", Name); }

    data.dir->udcfvalid = 0;

    int newlength = dir_Length();
    int delta_blocks = NBLOCKS(newlength) - NBLOCKS(oldlength);
    UpdateCacheStats(&FSDB->DirDataStats, REMOVE, delta_blocks);
}


/* MUST be called from within transaction! */
void fsobj::dir_MakeDir() {
    FSO_ASSERT(this, !HAVEDATA(this));

    data.dir = (VenusDirData *)RVMLIB_REC_MALLOC((int)sizeof(VenusDirData));
    RVMLIB_REC_OBJECT(*data.dir);
    bzero(data.dir, (int)sizeof(VenusDirData));

    if (::MakeDir((long *)data.dir, (long *)&fid, (long *)&pfid) != 0)
	{ print(logFile); Choke("fsobj::dir_MakeDir: MakeDir failed!"); }

    data.dir->udcfvalid = 0;
}


int fsobj::dir_Lookup(char *Name, ViceFid *Fid) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Lookup: (%s) no data", Name); }

    int code = ::Lookup((long *)data.dir, Name, (long *)Fid);
    if (code != 0) return(code);

    Fid->Volume = fid.Volume;
    return(0);
}


/* Name buffer had better be MAXNAMLEN bytes or more! */
int fsobj::dir_LookupByFid(char *Name, ViceFid *Fid) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_LookupByFid: (%x.%x.%x) no data", Fid->Volume, Fid->Vnode, Fid->Unique); }

    int code = ::LookupByFid((long *)data.dir, Name, (long *)Fid);
    if (code != 0) return(code);

    return(0);
}


struct RebuildDirHook {
    VolumeId vid;
    CVDescriptor cvd;
};

PRIVATE void RebuildDir(long hook, char *name, long vnode, long vunique) {
    struct RebuildDirHook *rbd_hook = (struct RebuildDirHook *)hook;

    /* We'll make . and .. first (below in [dir_Rebuild]) */
    if (strcmp(name, ".") && strcmp(name, ".."))
    /* so ignore them when they appear later. */

	/* Change FidToNodeid() also if this formula changes. */
	CVWriteEntry(name, (vunique + (vnode << 10) + (rbd_hook->vid << 20)), &rbd_hook->cvd);
}

/* Need not be called from within transaction. */
void fsobj::dir_Rebuild() {
    long int tmp[3];
    int ret;
    long vnode, vunique;

    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Rebuild: no data"); }

    struct RebuildDirHook hook;
    hook.vid = fid.Volume;
    hook.cvd.vType = stat.VnodeType;
    CVOpen(data.dir->udcf->Name(), &hook.cvd);

    /* write . */
    ret = ::Lookup((long *)data.dir, ".", tmp);
    vnode = tmp[1];
    vunique = tmp[2];
    CVWriteEntry(".", (vunique + (vnode << 10) + (hook.vid << 20)), &hook.cvd);
    /* write .. */
    ret = ::Lookup((long *)data.dir, "..", tmp);
    vnode = tmp[1];
    vunique = tmp[2];
    CVWriteEntry("..", (vunique + (vnode << 10) + (hook.vid << 20)), &hook.cvd);

    ::EnumerateDir((long *)data.dir, (int (*)(void * ...))RebuildDir, (long)(&hook));
    CVClose(&hook.cvd);

    data.dir->udcfvalid = 1;
}


int fsobj::dir_IsEmpty() {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_IsEmpty: no data"); }

    return(!(::IsEmpty((long *)data.dir)));	    /* ::IsEmpty() has backwards polarity! */
}


int fsobj::dir_IsParent(ViceFid *target_fid) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_IsParent: (%x.%x.%x) no data", target_fid->Volume, target_fid->Vnode, target_fid->Unique); }

    /* Volumes must be the same. */
    if (fid.Volume != target_fid->Volume) return(0);

    /* Don't match "." or "..". */
    if (FID_EQ(*target_fid, fid) || FID_EQ(*target_fid, pfid)) return(0);

    /* Lookup the target object. */
    char Name[MAXPATHLEN];
    return(::LookupByFid((long *)data.dir, Name, (long *)target_fid) == 0);
}


/* local-repair modification */
/* MUST be called from within transaction! */
void fsobj::dir_TranslateFid(ViceFid *OldFid, ViceFid *NewFid) {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_TranslateFid: (%x.%x.%x) -> (%x.%x.%x) no data", OldFid->Volume, OldFid->Vnode, OldFid->Unique, NewFid->Volume, NewFid->Vnode, NewFid->Unique); }

    if ((fid.Volume != OldFid->Volume && !IsLocalFid(OldFid) && !IsLocalFid(&fid)) ||
 	(fid.Volume != NewFid->Volume && !IsLocalFid(NewFid) && !IsLocalFid(&fid))) {
	print(logFile); Choke("fsobj::dir_TranslateFid: (%x.%x.%x) -> (%x.%x.%x) cross-volume", 
			      OldFid->Volume, OldFid->Vnode, OldFid->Unique, 
			      NewFid->Volume, NewFid->Vnode, NewFid->Unique); 
    }

    if (FID_EQ(*OldFid, *NewFid)) return;

    char Name[MAXPATHLEN];
    while (::LookupByFid((long *)data.dir, Name, (long *)OldFid) == 0) {
	dir_Delete(Name);
	dir_Create(Name, NewFid);
    }

    data.dir->udcfvalid = 0;
}


PRIVATE void PrintDir(long hook, char *name, long vnode, long vunique) {
    LOG(1000, ("\t%-16s : (%x.%x.%x)\n",
		name, hook, vnode, vunique));
}

void fsobj::dir_Print() {
    if (!HAVEDATA(this))
	{ print(logFile); Choke("fsobj::dir_Print: no data"); }

    if (LogLevel >= 1000) {
	LOG(1000, ("fsobj::dir_Print: %s, %d, %d\n",
		   data.dir->udcf->Name(), data.dir->udcf->Length(), data.dir->udcfvalid));

	::EnumerateDir((long *)data.dir, (int (*)(void * ...))PrintDir, fid.Volume);
    }
}
