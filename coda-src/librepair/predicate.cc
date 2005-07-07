/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>

#include <parser.h>
#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <inconsist.h>

#include "resolve.h"
#include "predicate.h"

static int Equal (resdir_entry **deGroup, int nDirEntries)
{
    int i;
    for (i = 1; i < nDirEntries; i++){
	if (memcmp(&(deGroup[i]->VV.StoreId), &(deGroup[0]->VV.StoreId), sizeof(ViceStoreId)) != 0)
	    return 0;
	if (VV_Cmp(&(deGroup[i]->VV), &(deGroup[0]->VV)))
	    return 0;
	if ((deGroup[i]->VV).Flags != (deGroup[0]->VV).Flags) 
	    return 0;
    }

    /* return TRUE since all VVs looked the same */
    return 1;
}

#ifdef UNUSED
static void PrintArgs (char *name, resdir_entry **deGroup, int nDirEntries)
{
    printf("Predicate %s : %d entries \n", name, nDirEntries);
    for (int i = 0; i < nDirEntries; i++)
	/* print the dir entries */
	printf("replica %d; name %s\n", deGroup[i]->replicaid, deGroup[i]->name);
    printf("***********************");
}
#endif

/* The following predicates return TRUE (1) or FALSE (0) */
static int ObjectOK (int nreplicas, resreplica *dirs,resdir_entry **deGroup, int nDirEntries, char *realm)
{
    /* remember to check if the names are the same */
    if (nreplicas != nDirEntries) return 0; 
    return(Equal(deGroup, nDirEntries));
}


static int WeaklyEqual (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
{
    if (nreplicas != nDirEntries) return 0;
    for(int i = 1; i < nDirEntries; i++)
	if (memcmp((const void *)&((deGroup[i]->VV).StoreId), (const void *)&((deGroup[0]->VV).StoreId), sizeof(ViceStoreId)))
	    return 0;
    return 1;
}

static int AllPresent (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
{
    return(nreplicas == nDirEntries);
}

/* given an object and its version vector at each replica */
/* nObjectSites returns the min number of sites the object was */
/* created at */
static int nObjectSites (resdir_entry **deGroup, int nDirEntries)
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

static int nlinks (resdir_entry *re, resreplica *dir)
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

static int Renamed (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
{
//    int objsites = nObjectSites(deGroup, nDirEntries);
//    if (objsites <= nDirEntries) return 0;

    int repfound[MAXHOSTS];
    int i;
    for (i = 0; i < MAXHOSTS; i++) repfound[i] = 0;

    // check if object exists in some other directory
    for (i = 0; i < nDirEntries; i++) {
	repfound[deGroup[i]->index] = 1;
#ifdef notdef
	long replicaid = deGroup[i]->replicaid;
	for (int j = 0; j < nreplicas; j++) {
	    if (dirs[j].replicaid == replicaid) {
		repfound[j] = 1;
		break;
	    }
	}
#endif /* notdef */
    }
    int renamed = 0;
    ViceFid tmpfid;
    tmpfid.Vnode = deGroup[0]->fid.Vnode;
    tmpfid.Unique = deGroup[0]->fid.Unique;
    for (i = 0; i < nreplicas; i++) {
	char path[MAXPATHLEN];
	char childpath[MAXNAMELEN];
	if (repfound[i]) continue;
	tmpfid.Volume = dirs[i].fid.Volume;
	ViceFid parentfid;
	if (!GetParent(realm, &tmpfid, &parentfid, path, childpath)) {
	    renamed = 1;
	    printf("Object %s was renamed\n", deGroup[0]->name);
	    break;
	}
    }
    return(renamed);
}


/* Object was created at only a subset of the sites */
static int SubsetCreate (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
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
    if ((nl = nlinks(deGroup[0], &(dirs[deGroup[0]->index]))) == 1 || ISDIRVNODE(deGroup[0]->fid.Vnode))
	/* file has no hard link or object is */
	/* a directory (no hard links) */
	return 0;
    else{
	/* check that the sites that have the name are */
	/* strictly dominant to the sites that dont  - not sure about this */
	printf("%s exists at only a subset of the sites.\n", deGroup[0]->name);
	printf("It has hard links too so resolution cannot be automated\n");
	if (Parser_getbool("Do you want me to create it at all sites? [y]", 1))
	    return 1;
	else return 0;
    }
	
}

/* Object was removed at a subset of the sites */
static int SubsetRemove (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
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
	if (!ISDIRVNODE(deGroup[0]->fid.Vnode) && nlinks(deGroup[0], &(dirs[deGroup[0]->index])) >= 2){
	    printf("Object %s has hard links; Resolution cannot be automated\n", deGroup[0]->name);
	    return 0;
	}
	else {
	    sprintf(str, "%s was removed at some sites; should it be REMOVED at ALL sites?", deGroup[0]->name);
	    if (Parser_getbool(str, 0))
		return 1;
	}
    }
    return 0;
}


static int MaybeSubsetRemove (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nDirEntries, char *realm)
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
	if (!ISDIRVNODE(deGroup[0]->fid.Vnode) && nlinks(deGroup[0], &(dirs[deGroup[0]->index])) >= 2){
	    printf("Object %s has hard links; Resolution cannot be automated\n", deGroup[0]->name);
	    return 0;
	}
	else {
	    sprintf(str, "Then should %s be RECREATED at all sites?", deGroup[0]->name);
	    if (Parser_getbool(str, 1))
		return 1;
	}
    }
    return 0;

}

/* to add more predicates just append to the list and update nPredicates */
PtrFuncInt Predicates[] = {	/* the predicate routines */
    &ObjectOK, &WeaklyEqual, &AllPresent, &Renamed, &SubsetCreate,
    &SubsetRemove, &MaybeSubsetRemove
};
int nPredicates = 7;

