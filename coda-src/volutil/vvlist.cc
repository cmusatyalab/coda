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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>

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
    CODA_ASSERT(nlists > 0);
    vvlist = (vvent **)malloc(sizeof(vvent*) * nlists);
    CODA_ASSERT(vvlist != NULL);
    bzero((void *)vvlist, sizeof(vvent*) * nlists);
    
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
		CODA_ASSERT(tmp != NULL);
		tmp->StoreId.Host = vvStoreIdHost;
		tmp->StoreId.Uniquifier = vvStoreIdUniquifier;
		tmp->unique = unique;
		tmp->isThere = 0;

		/* Transform vnode to index */
		int bitnum = vnodeIdToBitNumber(vnum);

		if (bitnum < 0 || bitnum >= nlists)
		{
		    LogMsg(0, VolDebugLevel, stdout, "vvtable: Vnode %d.%d StoreId %x.%x, has a bad index %d for vvlist[%d]\n",
			   vnum, unique, vvStoreIdHost, vvStoreIdUniquifier, bitnum, nlists);
		    CODA_ASSERT(0);
		}

		tmp->next = vvlist[bitnum];
		vvlist[bitnum] = tmp;
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
    if ((i >= table.nlists) || (i < 0)) 
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
