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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/volutil/dumpstream.h,v 1.1.1.1 1996/11/22 19:13:35 rvb Exp";
#endif /*_BLURB_*/





#ifndef _DUMPSTREAM_H_
#define _DUMPSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <lwp.h>		/* Include all files referenced herein */
#include <nfs.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>

#define MAXSTRLEN 80
class dumpstream {
    FILE *stream;
    char name[MAXSTRLEN];
    VnodeClass IndexType;
    int skip_vnode_garbage();
	
  public:
    dumpstream(char *);
    int getDumpHeader(struct DumpHeader *);
    int getVolDiskData(VolumeDiskData *);
    int getVnodeIndex(VnodeClass, long *, long *);
    int getNextVnode(VnodeDiskObject *, long *, int *, long *);
    int getVnode(int vnum, long unique, long offset, VnodeDiskObject *vdo);
    int copyVnodeData(DumpBuffer_t *);		/* Copy entire vnode into DumpFd*/
    int EndOfDump();				/* See if ENDDUMP is present */
    void setIndex(VnodeClass);
};

#endif	_DUMPSTREAM_H_

