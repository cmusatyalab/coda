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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif __MACH__
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <errno.h>
#include <sys/param.h>

extern int getbool(char *prompt, int defalt /* sic! */);

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <inconsist.h>


#include "resolve.h"
#include "predicate.h"


/* to add more predicates just append to the list and update nPredicates */
PtrFuncInt Predicates[] = {	/* the predicate routines */
    &ObjectOK, &WeaklyEqual, &AllPresent, &Renamed, &SubsetCreate, &SubsetRemove, &MaybeSubsetRemove
};
int nPredicates = 7;

int Equal C_ARGS((resdir_entry **deGroup, int nDirEntries))
{
    int i;
    for (i = 1; i < nDirEntries; i++){
	if (bcmp(&(deGroup[i]->VV.StoreId), &(deGroup[0]->VV.StoreId), sizeof(ViceStoreId)))
	    return 0;
	if (VV_Cmp(&(deGroup[i]->VV), &(deGroup[0]->VV)))
	    return 0;
	if ((deGroup[i]->VV).Flags != (deGroup[0]->VV).Flags) 
	    return 0;
    }

    /* return TRUE since all VVs looked the same */
    return 1;
}

PRIVATE void PrintArgs C_ARGS((char *name, resdir_entry **deGroup, int nDirEntries))
{
    printf("Predicate %s : %d entries \n", name, nDirEntries);
    for (int i = 0; i < nDirEntries; i++)
	/* print the dir entries */
	printf("replica %d; name %s\n", deGroup[i]->replicaid, deGroup[i]->name);
    printf("***********************");
}

/* The following predicates return TRUE (1) or FALSE (0) */
int ObjectOK C_ARGS((int nreplicas, resreplica *dirs,resdir_entry **deGroup, int nDirEntries))
{
    /* remember to check if the names are the same */
    if (nreplicas != nDirEntries) return 0; 
    return(Equal(deGroup, nDirEntries));
}


int WeaklyEqual C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries))
{
    if (nreplicas != nDirEntries) return 0;
    for(int i = 1; i < nDirEntries; i++)
	if (bcmp(&((deGroup[i]->VV).StoreId), &((deGroup[0]->VV).StoreId), sizeof(ViceStoreId)))
	    return 0;
    return 1;
}

int AllPresent C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries))
{
    return(nreplicas == nDirEntries);
}

/* given an object and its version vector at each replica */
/* nObjectSites returns the min number of sites the object was */
/* created at */
int nObjectSites C_ARGS((resdir_entry **deGroup, int nDirEntries))
{   
    ViceVersionVector vv;
    int	count = 0;
    int i;

    InitVV(&vv);
    
    for (i = 0; i < nDirEntries; i++)
	AddVVs(&vv, &(deGroup[i]->VV));
    
    for (i = 0; i < VSG_MEMBERS; i++)
	if ((&(vv.Versions.Site0))[i])
	    count++;
    
    return(count);
}

int nlinks C_ARGS((resdir_entry *re, resreplica *dir))
{
    char *path;
    struct stat buf;

    path = (char *)malloc(strlen(re->name) + strlen(dir->path) + 5);
    sprintf(path, "%s%s", dir->path, re->name);
    if (lstat(path, &buf) < 0)
	perror("stat");
    free(path);
    return(buf.st_nlink);
}

int Renamed C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries))
{
//    int objsites = nObjectSites(deGroup, nDirEntries);
//    if (objsites <= nDirEntries) return 0;

    int repfound[MAXHOSTS];
    int i;
    for (i = 0; i < MAXHOSTS; i++) repfound[i] = 0;

    // check if object exists in some other directory
    for (i = 0; i < nDirEntries; i++) {
	repfound[deGroup[i]->replicaid] = 1;
#ifdef notdef
	long replicaid = deGroup[i]->replicaid;
	for (int j = 0; j < nreplicas; j++) {
	    if (dirs[j].replicaid == replicaid) {
		repfound[j] = 1;
		break;
	    }
	}
#endif notdef
    }
    int renamed = 0;
    ViceFid tmpfid;
    tmpfid.Vnode = deGroup[0]->vno;
    tmpfid.Unique = deGroup[0]->uniqfier;
    for (i = 0; i < nreplicas; i++) {
	char path[MAXPATHLEN];
	char childpath[MAXNAMELEN];
	if (repfound[i]) continue;
	tmpfid.Volume = dirs[i].replicaid;
	ViceFid parentfid;
	if (!GetParent(&tmpfid, &parentfid, NULL, path, childpath)) {
	    renamed = 1;
	    printf("Object %s was renamed\n", deGroup[0]->name);
	    break;
	}
    }
    return(renamed);
}


/* Object was created at only a subset of the sites */
int SubsetCreate C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries))
{
    int	nl;

    if (nreplicas <= nDirEntries) return 0;

    /* on the client side we do not know which slot of the vv */
    /* corresponds to which replica - so we can only guess */
    int nSites = nObjectSites(deGroup, nDirEntries);
    if (nSites > nreplicas){
	printf("WARNING:  The repair for %s is going on without all servers in operation\nPlease look at it manually\n", 
	       deGroup[0]->name);
	return 0;
    }
    if (nSites < nreplicas)
	return 1;

    /* all sites in the vv know of the existence of the object */
    /* N.B. This is assuming that the non-zero slots in the vv */
    /* correspond to the replicas given */
    /* This should be a subset remove except when there are */
    /* Hard Links */
    /* nsites == nreplicas */
    if ((nl = nlinks(deGroup[0], &(dirs[deGroup[0]->replicaid]))) == 1 || ISDIR(deGroup[0]->vno))
	/* file has no hard link or object is */
	/* a directory (no hard links) */
	return 0;
    else{
	/* check that the sites that have the name are */
	/* strictly dominant to the sites that dont  - not sure about this */
	printf("%s exists at only a subset of the sites.\n", deGroup[0]->name);
	printf("It has hard links too so resolution cannot be automated\n");
	if (getbool("Do you want me to create it at all sites? [y]", 1))
	    return 1;
	else return 0;
    }
	
}

/* Object was removed at a subset of the sites */
int SubsetRemove C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries))
{
    char str[1024];

    if (nreplicas <= nDirEntries) return 0;
    
    /* once again we just make a decision based on nObjectSites */
    /* we need ghosts implemented before we can decide if the object */
    /* removed at a subset of the sites was not modified at other sites */
    /* after the remove */
    int nSites = nObjectSites(deGroup, nDirEntries);

    if (nSites > nDirEntries && nSites == nreplicas) {
	/* we can be sure only when object is a file and has no hard links */
	if (!ISDIR(deGroup[0]->vno) && nlinks(deGroup[0], &(dirs[deGroup[0]->replicaid])) >= 2){
	    printf("Object %s has hard links; Resolution cannot be automated\n", deGroup[0]->name);
	    return 0;
	}
	else {
	    sprintf(str, "%s was removed at some sites; should it be REMOVED at ALL sites?", deGroup[0]->name);
	    if (getbool(str, 0))
		return 1;
	}
    }
    return 0;
}


int MaybeSubsetRemove C_ARGS((int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries)) {
    char str[1024];

    if (nreplicas <= nDirEntries) return 0;
    
    /* once again we just make a decision based on nObjectSites */
    /* we need ghosts implemented before we can decide if the object */
    /* removed at a subset of the sites was not modified at other sites */
    /* after the remove */
    int nSites = nObjectSites(deGroup, nDirEntries);

    if (nSites > nDirEntries && nSites == nreplicas) {
	/* we can be sure only when object is a file and has no hard links */
	if (!ISDIR(deGroup[0]->vno) && nlinks(deGroup[0], &(dirs[deGroup[0]->replicaid])) >= 2){
	    printf("Object %s has hard links; Resolution cannot be automated\n", deGroup[0]->name);
	    return 0;
	}
	else {
	    sprintf(str, "Then should it be RECREATED at all sites?", deGroup[0]->name);
	    if (getbool(str, 1))
		return 1;
	}
    }
    return 0;

}
