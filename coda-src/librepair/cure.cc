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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/resolve/cure.cc,v 1.1 1996/11/22 19:06:36 braam Exp $";
#endif /*_BLURB_*/







/* cure.c 
 * Created 12/09/89 - Puneet Kumar
 *
 * Procedures to repair inconsistent directories 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

extern int getbool(char *prompt, int defalt /* sic! */);

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>

#include "repio.h"
#include "resolve.h"
extern int IsCreatedEarlier C_ARGS((struct listhdr **, int, long, long));

#define FidEq(a, b) \
(((a).Vnode == (b).Vnode) && \
 ((a).Unique == (b).Unique))

int ObjExists C_ARGS((resreplica *dir, long vnode, long unique))
{
    for (int i = dir->entry1; i < (dir->entry1 + dir->nentries); i++)
	if ((*(direntriesarr + i)).vno == vnode && (*(direntriesarr + i)).uniqfier == unique)
	    return 1;
    return 0;
}

int RepairRename C_ARGS((int nreplicas, resreplica *dirs, 
			 resdir_entry **deGroup, int nDirEntries, 
			 listhdr **ops, char *volmtpt))
{
    char parentpath[MAXHOSTS][MAXPATHLEN];
    ViceFid parentfid[MAXHOSTS];
    char childpath[MAXHOSTS][MAXNAMELEN];
    int usepath[MAXHOSTS];
    int i;

    for (i = 0; i < MAXHOSTS; i++){
	parentfid[i].Volume = 0;
	parentfid[i].Vnode = 0;
	parentfid[i].Unique = 0;
	usepath[i] = 0;
    }

    // get the paths for parents 
    int nobjfound = 0;
    ViceFid tmpfid;
    tmpfid.Vnode = deGroup[0]->vno;
    tmpfid.Unique = deGroup[0]->uniqfier;
    for (i = 0; i < nreplicas; i++) {
	tmpfid.Volume = dirs[i].replicaid;
	if (!GetParent(&tmpfid, &parentfid[i], volmtpt, parentpath[i], childpath[i])) {
	    nobjfound ++;
	    usepath[i] = 1;
	}
    }
    printf("Object was found at %d replicas\n", nobjfound);
    if (nreplicas != nobjfound) {
	printf("***************************************************************\n");
	printf("Warning:  Object was renamed at some sites and removed at other\n");
	printf("If you decide to preserve the object, you might need to perform\n");
	printf("another compare/doRepair cycle after repairing  the directory with the");
	printf(" current fix file\n");
	printf("***************************************************************\n");
	printf("\n\n\n");
    }

    // prompt user for which one should be used 
    // what name should be used 
    printf("Object exists in the following directories\n");
    for (i = 0; i < nreplicas; i++) 
	if (usepath[i])
	    printf("\t%s as %s\n", parentpath[i], childpath[i]);

    int prevset = 0;
    char prevpath[2048];
    char curpath[2048];
    for (i = 0; i < nreplicas; i++) {
	char buf[2048];
	if (!usepath[i]) continue;
	sprintf(curpath, "%s/%s", parentpath[i], childpath[i]);
	if (prevset &&  (!strcmp(curpath, prevpath))) continue; 
	sprintf(buf, "Do you want to preserve %s? ", curpath);
	if (getbool(buf, 1)) break;
	strcpy(prevpath, curpath);
	prevset = 1;
    }
    struct repair rep;
    if (i == nreplicas) {
	printf("No renames specified - will try to remove object\n");
	int isDir = ISDIR(deGroup[0]->vno);
	for (int j = 0; j < nreplicas; j++) 
	    if ((parentfid[j].Vnode == dirs[j].vnode) && 
		(parentfid[j].Unique == dirs[j].uniqfier) && 
		usepath[j]) {
		if (isDir)
		    rep.opcode = REPAIR_REMOVED;
		else 
		    rep.opcode = REPAIR_REMOVEFSL;
		strcpy(&(rep.name[0]), childpath[j]);
		rep.parms[0] = 0;
		rep.parms[1] = 0;
		rep.parms[2] = 0;
		InsertListHdr(&rep, ops, j);
	    }
    }
    else 
	for (int j = 0; j < nreplicas; j++) {
	    if (i == j) continue;
	    if (!usepath[j]) continue;
	    if (FidEq(parentfid[j], parentfid[i]) &&
		!strcmp(childpath[i], childpath[j]))
		continue;
	    // Search whether the two parent fid are created in the 
	    // earlier repair operations. Skip it if so, and notify the repairer.
	    if (IsCreatedEarlier(ops, j, parentfid[j].Vnode, parentfid[j].Unique) ||
		IsCreatedEarlier(ops, j, parentfid[i].Vnode, parentfid[i].Unique)) {
		printf("more repair actions are needed on replica %s ", dirs[j].path);
		printf("please use the \"comparedir\" and \"dorepair\" commands again\n");
		continue;
	    }
	    rep.opcode = REPAIR_RENAME;
	    strcpy(rep.name, childpath[j]);
	    strcpy(rep.newname, childpath[i]);
	    rep.parms[0] = RepVolume;
	    rep.parms[1] = parentfid[j].Vnode;
	    rep.parms[2] = parentfid[j].Unique;
	    rep.parms[3] = parentfid[i].Vnode;
	    rep.parms[4] = parentfid[i].Unique;
	    InsertListHdr(&rep, ops, j);
	    
	    //printf("%s/%s in (%x.%x) should be renamed to %s in (%x.%x)\n",
	    // parentpath[j], childpath[j], parentfid[j].Vnode, parentfid[j].Unique,
	    // childpath[i], parentfid[i].Vnode, parentfid[i].Unique);
	}
    return(0);
}
/* given an object, RepairSubsetCreate decides which replicas */
/* donot have that object and places that info in the repair ops list */
int RepairSubsetCreate C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, listhdr **ops))
{
    int isDir = ISDIR(deGroup[0]->vno);
    struct repair rep;
    int *ObjFound;
    int i;
    ObjFound = (int *)malloc(sizeof(int) * nreplicas);
    
    for(i = 0; i < nreplicas; i++)
	ObjFound[i] = 0;
    /* set flag whereever object exists */
    for (i = 0; i < nDirEntries ; i++)
	ObjFound[(deGroup[i]->replicaid)] = 1;

    /* object has to be created at all zero sites in ObjFound */
    for (i = 0; i < nreplicas; i ++){
	if (!ObjFound[i]){
	    if (isDir)
		rep.opcode = REPAIR_CREATED;
	    else if (deGroup[0]->MtPt) 
		rep.opcode = REPAIR_CREATES;
	    else {
		/* it is a CREATES, CREATEF or CREATEL */
		char *str;
		struct stat buf;

		str = (char *)malloc(strlen(&(deGroup[0]->name[0])) + strlen(dirs[i].path) + 1);
		strcpy(str, dirs[deGroup[0]->replicaid].path);
		strcat(str, &(deGroup[0]->name[0]));
		lstat(str, &buf);
		if ((buf.st_mode & S_IFMT) == S_IFLNK)
		    /* object is a symbolic link */
		    rep.opcode = REPAIR_CREATES;
		else {
		    /* object is a file - if same object already exists in the */
		    /* directory with a different name or in the  */
		    /* replist with creates */
		    /* then it is a CREATEL, ow it is a CREATEF */
		    if (ObjExists(&(dirs[i]), deGroup[0]->vno, deGroup[0]->uniqfier) || InRepairList(&((*ops)[i]),  REPAIR_CREATEF, deGroup[0]->vno, deGroup[0]->uniqfier))
			rep.opcode = REPAIR_CREATEL;
		    else rep.opcode = REPAIR_CREATEF;
		}
		free(str);
	    }
	}
	else continue;

	strcpy(&(rep.name[0]), &(deGroup[0]->name[0]));
	rep.parms[0] = RepVolume;
	rep.parms[1] = deGroup[0]->vno;
	rep.parms[2] = deGroup[0]->uniqfier;
	InsertListHdr(&rep, ops, i);
    }
    free(ObjFound);
    return (0);
}

/* given an object, RepairSubsetRemove decides which replicas */
/* have that object and places that info in the repair ops list */
int RepairSubsetRemove C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, listhdr **ops))
{
    int isDir = ISDIR(deGroup[0]->vno);
    struct repair rep;
    int *ObjFound;
    int i;

    ObjFound = (int *)malloc(sizeof(int) * nreplicas);
    
    for(i = 0; i < nreplicas; i++)
	ObjFound[i] = 0;
    /* set flag whereever object exists */
    for (i = 0; i < nDirEntries ; i++)
	ObjFound[(deGroup[i]->replicaid)] = 1;

    /* object has to be removed at all 1 sites in ObjFound */
    for (i = 0; i < nreplicas; i ++){
	if (ObjFound[i]){
	    if (isDir){
		rep.opcode = REPAIR_REMOVED;
	    }
	    else 
		rep.opcode = REPAIR_REMOVEFSL;
	}
	else 
	    continue;

	strcpy(&(rep.name[0]), &(deGroup[0]->name[0]));
	rep.parms[0] = 0;
	rep.parms[1] = 0;
	rep.parms[2] = 0;
	InsertListHdr(&rep, ops, i);
    }
    free(ObjFound);
    return (0); 
}

