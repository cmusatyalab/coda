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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vol/index.cc,v 4.4 1998/01/10 18:39:40 braam Exp $";
#endif /*_BLURB_*/








/* Operations on volume/vnode index files */



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef __MACH__
#include <sys/inode.h>
#endif /* __MACH__ */
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <struct.h>

#include <lwp.h>
#include <lock.h>

#ifdef __MACH__
#include <mach.h>
#endif /* __MACH__ */
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <rvmlib.h>

#include "vice.h"
#include "cvnode.h"
#include "volume.h"
#include "vutil.h"
#include "index.h"
#include "recov.h"
#include "camprivate.h"
#include "coda_globals.h"
#include "volhash.h"



/* Initialize the vindex structure with client supplied parameters */
/* if no arguments are supplied, they are initialized to -1 */
vindex::vindex(VolumeId volid, int vnodetype, Device dev, int size, int volindex)
/*    volid	    unique identifier of object's volume */
/*    vnodetype	    vLarge or vSmall */
/*    dev	    device on which index file resides */
/*    size	    size of object contained in index file */
/*    volindex      If supplied, use this index rather than looking it */
/*                  up in the hash table, added for norton. */
{
    int objsize = 0;

#ifdef notdef
    /* hoping this isn't necessary...*/
    /* Make sure objsize is a power of 2 */
    for (objsize = 1; objsize < size; objsize *=2);
    assert(objsize == size);
#endif notdef

    vol_id = volid;
    if (((vol_index = volindex) == -1) &&
	(vol_index = HashLookup(volid)) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "vindex constructor: bogus volumeid %x", volid);
	return;
    }
    vtype = vnodetype;
    unix_dev = dev;
    camindex = -1;
}

/* how many elements are there in the index? */
int vindex::elts()
{
    return(ActiveVnodes(vol_index, vtype));
}

/* how many vnodes are being used in the index? */
int vindex::vnodes()
{
    return(AllocatedVnodes(vol_index, vtype));
}

/* IsEmpty - checks if there are any Vnodes at a particular slot */
int vindex::IsEmpty(VnodeId offset) {
    bit32 vnodeindex = vnodeIdToBitNumber(offset);
    rec_smolist *l;
    if (vtype == vSmall)
	l = &(CAMLIB_REC(VolumeList[vol_index]).data.smallVnodeLists[vnodeindex]);
    else 
	l = &(CAMLIB_REC(VolumeList[vol_index]).data.largeVnodeLists[vnodeindex]);
    return(l->IsEmpty());
}
/* Get the vnode with the specified id from the vnode array */
int vindex::get(VnodeId offset, Unique_t u, VnodeDiskObject *buf) {

    Error ec;

    bit32 vnodeindex = vnodeIdToBitNumber(offset);
    return(ExtractVnode(&ec, vol_index, vtype, vnodeindex, u, buf));
}

/* Get the vnode at the specified offset in the vnode array */
int vindex::oget(bit32 offset, Unique_t u, VnodeDiskObject *buf) {

    Error ec;
    return(ExtractVnode(&ec, vol_index, vtype, offset, u, buf));
}

/* Write out the vnode with the specified id to the appropriate */
/* index in the vnode array */
int vindex::put(VnodeId offset, Unique_t u, VnodeDiskObject *buf) {

    bit32 vnodeindex = vnodeIdToBitNumber(offset);
    return(ReplaceVnode(vol_index, vtype, vnodeindex, u, buf));
}

/* Write the given vnode to the specified index in the vnode array */
int vindex::oput(bit32 offset, Unique_t u, VnodeDiskObject *buf) {

    return(ReplaceVnode(vol_index, vtype, offset, u, buf));
}

/* Destructor for cleaning up state; close any open file descriptors */
vindex::~vindex()
{
}

vindex_iterator::vindex_iterator(vindex& myind)
{
    v_ind = &myind;
    if (v_ind->vtype == vSmall)
	vlists = CAMLIB_REC(VolumeList[v_ind->vol_index]).data.smallVnodeLists;
    else
	vlists = CAMLIB_REC(VolumeList[v_ind->vol_index]).data.largeVnodeLists;
    clist = 0;
    nlists = v_ind->elts();
    nextlink = new rec_smolist_iterator(vlists[clist]);
}

vindex_iterator::~vindex_iterator()
{
    if (nextlink)
	delete nextlink;
}

/* Iterate through all vnode in specified index, skipping over */
/* null slots. Fills in vnode argument, and returns the */
/* recoverable vnode index or -1 if there are no more vnodes */
/* in the specified index */
int vindex_iterator::operator()(VnodeDiskObject *vnode) {
    rec_smolink *p;
    while (clist < nlists) {
	p = nextlink ? (*nextlink)() : 0;
	if (p) {
	    VnodeDiskObject *vdo = strbase(VnodeDiskObject, p, nextvn);
	    if (vdo->type == vNull)
		continue;
	    else if(v_ind->vtype == vSmall)
		bcopy((const void *)vdo, (void *)vnode, SIZEOF_SMALLDISKVNODE);
	    else if(v_ind->vtype == vLarge)
		bcopy((const void *)vdo, (void *)vnode, SIZEOF_LARGEDISKVNODE);
	    else
		LogMsg(0, 0, stdout, "vindex_iterator::operator() bad vnode type %d", v_ind->vtype);

	    return(clist);
	}
	else {
	    delete nextlink;
	    nextlink = 0;
	    clist++;
	    if (clist < nlists) 
		nextlink = new rec_smolist_iterator(vlists[clist]);
	}
    }
    return(-1);
}


