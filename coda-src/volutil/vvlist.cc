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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vvlist.cc,v 4.4 1998/01/10 18:40:08 braam Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>
#ifdef __cplusplus
}
#endif __cplusplus
    
#include <util.h>
#include <vcrcommon.h>
#include <voltypes.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <srv.h>
#include <vrdb.h>
#include <vutil.h>

#include "vvlist.h"

/*
 * I'm currently using the VRDB to find the groupid of the parent volume.
 * However, I'm not convinced this is the most efficient way. I keep having
 * these nagging suspicions that alot of the information in the header of
 * the backup clone should reflect the state of the parent, not of itself.
 * Groupid is one such field... - dcs 2/19/91
 */
void getlistfilename(char *filename, VolumeId groupId, VolumeId repId, char *suffix)
{
    if (groupId > 0)
	sprintf(filename, "/vice/backup/%x.%x.", groupId, repId);
    else
	sprintf(filename, "/vice/backup/%x.", repId);
	
    strcat(filename, suffix);
}

/*
 * Verify the correctness of the dump header and that it was of the same rw
 * volume. Return the uniquifier of the ancient volume to mark the dump.
 */
int ValidListVVHeader(FILE *Ancient, register Volume *vp, int *unique)
{
    char buffer[LISTLINESIZE];
    char dummy[13];
    int volid, parid;
    
    fgets(buffer, LISTLINESIZE, Ancient);

    if (sscanf(buffer, "%s dump of backup vol %x(%x) for R/W vol %x\n",
	   dummy, &volid, unique, &parid) < 4)
	return FALSE;

    if (parid != V_parentId(vp))
	return FALSE;

    return TRUE;
}

void DumpListVVHeader(int VVListFd,register Volume *vp,int Incremental,int unique)
{
    char buffer[LISTLINESIZE];
    long time = V_copyDate(vp);

    /* Don't put "\n" on sprintf format since ctime() puts one there. */
    if (VVListFd > 0) {
	if (V_type(vp) == BACKVOL) /* Only Backups or R/O are dumped. */
	    sprintf(buffer,
		    "%s dump of backup vol %x(%x) for R/W vol %x, backup at %s",
		    (Incremental? "Incremental" : "Full"),
		    V_id(vp), unique, V_parentId(vp), ctime(&time));
	else 
	    sprintf(buffer,
		    "%s dump of clone vol %x(%x) for R/W vol %x, cloned at %s",
		    (Incremental? "Incremental" : "Full"),
		    V_id(vp), unique, V_parentId(vp), ctime(&time));

	if (write(VVListFd, buffer, (int)strlen(buffer)) != strlen(buffer))
	    LogMsg(0, VolDebugLevel, stdout, "DumpListVVHeader write didn't succeed");    
    }
}

/* Output the VV for a vnode to a file. */
void ListVV(int fd, int vnode, struct VnodeDiskObject *vnp)
{
    char buffer[LISTLINESIZE];
    ViceVersionVector *vv = (ViceVersionVector *)(&(vnp->versionvector));
    
    if (fd > 0) {
	sprintf(buffer, "%d.%d (%d.%d.%d.%d.%d.%d.%d.%d) (%x.%x)\n",
		vnode, vnp->uniquifier,
		vv->Versions.Site0,     vv->Versions.Site1, 
		vv->Versions.Site2,     vv->Versions.Site3, 
		vv->Versions.Site4,     vv->Versions.Site5, 
		vv->Versions.Site6,     vv->Versions.Site7, 
		vv->StoreId.Host,       vv->StoreId.Uniquifier);

	if (write(fd, buffer, (int)strlen(buffer)) != strlen(buffer))
	    LogMsg(0, VolDebugLevel, stdout, "ListVV didn't write out correctly (%d)", errno);
    }
}

/* Definition for vvlist class */

vvtable::vvtable(FILE *Ancient, VnodeClass vclass, int listsize)
{
    char buffer[LISTLINESIZE];
    int vnum, unique, d, vvStoreIdHost, vvStoreIdUniquifier;
    nlists = listsize;
    assert(nlists > 0);
    vvlist = (vvent **)malloc(sizeof(vvent) * nlists);
    bzero((void *)vvlist, sizeof(vvent) * nlists);
    
    LogMsg(9, VolDebugLevel, stdout, "After malloc of vvlist, entering do loop");
    do {
	if (fgets(buffer, LISTLINESIZE, Ancient) == NULL) {
	    if (!feof(Ancient)) {
		LogMsg(10, VolDebugLevel, stdout, "Dump: fgets indicates error."); /* Abort? */
	    }	
	} else {
	    if (sscanf(buffer, "%d.%d (%d.%d.%d.%d.%d.%d.%d.%d) (%x.%x)\n",
		       &vnum, &unique, &d,  &d,  &d,  &d,  &d,  &d,  &d,  &d, 
		       &vvStoreIdHost, &vvStoreIdUniquifier) == 12) {
		/* Found an vnode, Insert it at the appropriate place! */
		LogMsg(19, VolDebugLevel, stdout, "vvtable: found a vnode %d.%d StoreId %x.%x.",
		    vnum, unique, vvStoreIdHost, vvStoreIdUniquifier);
		vvent *tmp = (vvent *)malloc(sizeof(vvent));
		tmp->StoreId.Host = vvStoreIdHost;
		tmp->StoreId.Uniquifier = vvStoreIdUniquifier;
		tmp->unique = unique;
		tmp->isThere = 0;

		vnum = vnodeIdToBitNumber(vnum);  /* Transform vnode to index */
		tmp->next = vvlist[vnum];
		vvlist[vnum] = tmp;
	    }
	}
    } while ((vclass == vLarge) ? strcmp(buffer, ENDLARGEINDEX) : !feof(Ancient));
    LogMsg(9, VolDebugLevel, stdout, "After do loop");
}

vvtable::~vvtable()
{
    for (int i = 1; i < nlists; i++) {
	while (vvlist[i]) {
	    vvent *tmp = vvlist[i];
	    vvlist[i] = vvlist[i]->next;
	    free((char *)tmp);
	}
    }
    free((char *)vvlist);
}

int vvtable::IsModified(int vnodeIndex, long unique, ViceStoreId *StoreId)
{
    if (vnodeIndex >= nlists) {
	/* This means we have a new file, since the list has grown. */
	return TRUE;
    }
    vvent *eptr = vvlist[vnodeIndex];
    while (eptr) {
	if (eptr->unique == unique) {
	    /* Found object, mark it as touched. */
	    eptr->isThere = 1;
	    
	    if ((eptr->StoreId.Host == StoreId->Host) &&
		(eptr->StoreId.Uniquifier == StoreId->Uniquifier)) {
		LogMsg(29, VolDebugLevel, stdout, "Object %d.%d unchanged.", vnodeIndex, unique);
		return FALSE; /* Object was found, StoreId unchanged */
	    } else {
		LogMsg(29, VolDebugLevel, stdout, "Object %d.%d changed.", vnodeIndex, unique);
		return TRUE;      /* Object was found, StoreId unchanged */
	    }
	}
	eptr = eptr->next;
    }
    LogMsg(29, VolDebugLevel, stdout, "Object %d.%d created.", vnodeIndex, unique);
    return TRUE;		/* Object wasn't found, must be new */
}		    

vvent_iterator::vvent_iterator(vvtable& table, int i)
{
    if ((i > table.nlists) || (i < 0)) 
	cvvent = (vvent *)0;
    else
	cvvent = table.vvlist[i];
}

vvent * vvent_iterator::operator()()
{
    vvent *current = cvvent;
    if (cvvent) 
	cvvent = cvvent->next;

    return current;
}
