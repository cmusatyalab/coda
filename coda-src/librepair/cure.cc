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







/* cure.c 
 * Created 12/09/89 - Puneet Kumar
 *
 * Procedures to repair inconsistent directories 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <parser.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>

#include "repio.h"
#include "resolve.h"
extern int IsCreatedEarlier(struct listhdr **, int, long, long);

#define FidEq(a, b) \
(((a).Vnode == (b).Vnode) && \
 ((a).Unique == (b).Unique))

int ObjExists(resreplica *dir, long vnode, long unique)
{
    for (int i = dir->entry1; i < (dir->entry1 + dir->nentries); i++)
	if ((*(direntriesarr + i)).vno == vnode && (*(direntriesarr + i)).uniqfier == unique)
	    return 1;
    return 0;
}

int RepairRename (int nreplicas, resreplica *dirs, 
			 resdir_entry **deGroup, int nDirEntries, 
			 listhdr **ops, char *volmtpt, VolumeId RepVolume)
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
#ifndef DJGPP  /* under Windows 95 this call will never run */
	if (Parser_getbool(buf, 1)) break;
#endif
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
/* do not have that object and places that info in the repair ops list */
int RepairSubsetCreate (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, listhdr **ops, VolumeId RepVolume)
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

		str = (char *)malloc(strlen(deGroup[0]->name) + strlen(dirs[deGroup[0]->replicaid].path) + 1);
		strcpy(str, dirs[deGroup[0]->replicaid].path);
		strcat(str, deGroup[0]->name);
#ifdef S_IFLNK
		lstat(str, &buf);
		if ((buf.st_mode & S_IFMT) == S_IFLNK)
		    /* object is a symbolic link */
		    rep.opcode = REPAIR_CREATES;
		else {
#endif
		    /* object is a file - if same object already exists in the */
		    /* directory with a different name or in the  */
		    /* replist with creates */
		    /* then it is a CREATEL, ow it is a CREATEF */
		    if (ObjExists(&(dirs[i]), deGroup[0]->vno, 
				  deGroup[0]->uniqfier) 
			|| InRepairList(&((*ops)[i]),  REPAIR_CREATEF, 
					deGroup[0]->vno, deGroup[0]->uniqfier))
			    rep.opcode = REPAIR_CREATEL;
		    else 
			    rep.opcode = REPAIR_CREATEF;
#ifdef S_IFLNK
		}
#endif
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
int RepairSubsetRemove (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, listhdr **ops)
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

