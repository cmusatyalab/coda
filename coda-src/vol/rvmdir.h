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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/rvmdir.h,v 4.1 1997/01/08 21:52:16 rvb Exp $";
#endif /*_BLURB_*/






/*
 * rvmdir.h 
 * Created 01/90
 *  Puneet Kumar
 */

#ifndef _DIR_RVMDIR_H_
#define _DIR_RVMDIR_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "lwp.h"
#include "lock.h"

#ifdef __cplusplus
}
#endif __cplusplus


#include <dhash.h>
#include "cvnode.h"
#include "volume.h"

#define	PAGESIZE 2048
#define	MAXPAGES 128

struct VFid
{
    long volume;    /* volume number */
    long vnode;	    /* vnode number */
    long vunique;   /* uniquefier */
};

class shadowDirPage : public dlink {
  friend int FidCmp(shadowDirPage *, shadowDirPage *);
  friend void dirtest();
  friend dlist *GetDirShadowPages(struct VFid *, dhashtab *);
  friend int DCommit(Vnode *);
  friend int DAbort(Vnode *);
  friend void ICommit(struct VFid *, long *);
  friend ReallyRead(void *, long, char *);
  friend ReallyWrite(void *, long, char *);
    struct VFid Fid;
    int	    PageNum;
    char    Data[PAGESIZE];
    
    shadowDirPage(struct VFid, int, char *);
    ~shadowDirPage();
  public:
    void print();
    void print(FILE *);
    void print(int);
};
typedef struct DirInode
{
    long    *Pages[MAXPAGES];
    int	    refcount;
} DirInode;

#define	HTBSIZE 32
extern int  DirHtbHash(void *);
extern int  FidCmp(shadowDirPage *, shadowDirPage *);
extern int  DirHtbInit();
extern dlist *GetDirShadowPages(struct VFid *, dhashtab *);
extern void ICommit(struct VFid *, long *);
extern int DCommit(Vnode *);
extern int DAbort(Vnode *);
extern void DDec(DirInode *);
extern int CopyDirInode(DirInode *, DirInode **);
extern void VMFreeDirInode(DirInode *inode);
extern int VMCopyDirInode(DirInode *oldinode, DirInode **newinode);
extern dhashtab *DirHtb;
#endif not _DIR_RVMDIR_H_
