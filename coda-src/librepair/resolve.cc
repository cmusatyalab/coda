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







/* 
 * resolve.c 
 * Created 09/18/89  - Puneet Kumar
 */

/* This file contains functions that interface with the
 * manual resolution system 
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <inodeops.h>

#ifdef __cplusplus
}
#endif

#include <venusioctl.h>
#include <vice.h>
#include <parser.h>
#include "coda_assert.h" 
#include "resolve.h"
#include "repio.h"
#include "predicate.h"
#include "cure.h"

int	resdirCompareByFidName (resdir_entry **, resdir_entry **);
int	resdirCompareByName (resdir_entry **, resdir_entry **);
static int nextindex();


/* globals */
resdir_entry	*direntriesarr;
int direntriesarrsize;
int nextavailindex = -1;
resdir_entry	**sortedArrByFidName;	/* for sorting the direntries in fid order*/
resdir_entry	**sortedArrByName;	/* for sorting the direntries in name order */
int totaldirentries = 0;
/* VolumeId RepVolume; */
int nConflicts;
static char AclBuf[2048];

static int getfid (char *path, VenusFid *Fid, ViceVersionVector *VV, struct ViceIoctl *vi)
{
    char buf[2048];
    vi->out = buf;
    vi->out_size = sizeof(buf);
                      
    if (pioctl(path, VIOC_GETFID, vi, 0)) {
	char symval[MAXPATHLEN];
	symval[0] = 0;
	if (errno != ENOENT){
		perror("pioctl:GETFID");
		return errno;
	}
	/* in case volume root is in conflict */
	if (readlink(path, symval, MAXPATHLEN) < 0 || symval[0] != '@'){
		perror("res_getfid: readlink");
		return errno;	
	}
	sscanf(symval, "@%x.%x.%x.%x", &(Fid->Realm), &(Fid->Volume), &(Fid->Vnode), &(Fid->Unique));
	/* return garbage in VV */
	return 0;
    }
    memcpy((void *) Fid, (void *)buf, sizeof(VenusFid));
    memcpy((void *)VV, (char *)buf+sizeof(VenusFid), sizeof(ViceVersionVector));
    return 0;
}

int res_getfid (char *path, VenusFid *Fid, ViceVersionVector *VV)
{
    struct ViceIoctl vi;
    vi.in = 0;
    vi.in_size = 0;
    return getfid(path, Fid, VV, &vi);
}

int res_getmtptfid (char *path, VenusFid *Fid, ViceVersionVector *VV)
{
    int getmtpt = 1;
    struct ViceIoctl vi;
    vi.in = (char *)&getmtpt;
    vi.in_size = sizeof(int);
    return getfid(path, Fid, VV, &vi);
}
char *SkipLine (char *buf)
{
    while (*buf != '\n') buf++;
    buf++;
    return buf;
}

struct Acl *ParseAcl (char *buf)
{
    struct Acl *ta;
    struct AclEntry *alist;
    int i;

    /* get the number of + and - entries */
    ta = (struct Acl *) malloc(sizeof(struct Acl));
    sscanf(buf, "%d", &(ta->nplus));
    buf = SkipLine(buf);
    sscanf(buf, "%d", &(ta->nminus));
    buf = SkipLine(buf);

    ta->pluslist = (struct AclEntry *) malloc(ta->nplus * sizeof(struct AclEntry));
    ta->minuslist = (struct AclEntry *) malloc(ta->nminus * sizeof(struct AclEntry));

    /* get the + entries */
    alist = ta->pluslist;
    for (i = 0; i < ta->nplus; i++){
	sscanf(buf, "%255s %d", alist[i].name, &(alist[i].rights));
	buf = SkipLine(buf);
    }

    /* get the - entries */
    alist = ta->minuslist;
    for (i = 0; i < ta->nminus; i++){
	sscanf(buf, "%100s %d", alist[i].name, &(alist[i].rights));
	buf = SkipLine(buf);
    }    
    return ta;
}

void FreeAcl (struct Acl *ta)
{
    if (ta){
	if (ta->pluslist) free(ta->pluslist);
	if (ta->minuslist) free(ta->minuslist);
	free(ta);
    }
}
struct Acl *res_getacl (char *path)
{
    struct ViceIoctl vi;

    vi.in_size = 0;
    vi.in = 0;
    vi.out = AclBuf;
    vi.out_size = sizeof(AclBuf);
    if (pioctl(path, VIOCGETAL, &vi, 0)){
	perror("pioctl: GETAL");
	if (errno == EINVAL) {
	    printf("can not get ACL on fake object %s\n", path);
	}
	return (struct Acl *) 0;
    }
    return(ParseAcl(AclBuf));
}

int getunixdirreps (int nreplicas, char *names[], resreplica **reps)
{
  DIR *dirp;
  struct dirent *dp;
  struct stat buf;
  resreplica *dirs;
  int i,j;
  
  /* allocate space for making replica headers */
  *reps = dirs = (resreplica *)malloc(nreplicas * sizeof(resreplica));

  /* get approximate size of directories */
  if (stat(names[0], &buf)){
      perror(names[0]);
      return(-1);
  }
  /* allocate space for dir entries  - allocate some extra nodes */
  direntriesarrsize = (nreplicas + 1) * (buf.st_size/AVGDIRENTRYSIZE);
  CODA_ASSERT(direntriesarr = (resdir_entry *)malloc(direntriesarrsize * sizeof(resdir_entry)));
  nextavailindex = -1;
  totaldirentries = 0;

  for(j = 0; j < nreplicas; j++){
      int count;
      VenusFid   Fid;
      ViceVersionVector VV;

      dirs[j].entry1 = totaldirentries;
      dirp = opendir(names[j]);
      if (dirp == NULL) {
	  perror("opendir");
	  return -1;
      }

      /* fill in the all the resdir_entry structs for this replica */
      for (count = 0, dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
      {
          char *path;

          if (!strcmp(".", dp->d_name) || !strcmp("..", dp->d_name))
              continue;
          path = (char *)malloc(strlen(names[j]) + strlen(dp->d_name) + 1);
          strcpy(path, names[j]);
          strcat(path, dp->d_name);
          if (res_getfid(path, &Fid, &VV)) {
	      free(path);
              continue;
	  }
    
	  /* get index of direntry */
	  i = nextindex();
	  if (Fid.Vnode != 1 || Fid.Unique != 1) {
	      direntriesarr[i].vno = Fid.Vnode;
	      direntriesarr[i].uniqfier = Fid.Unique;
	      direntriesarr[i].MtPt = 0;
	  }
	  else {
	      if (res_getmtptfid(path, &Fid, &VV)) return -1;
	      direntriesarr[i].vno = Fid.Vnode;
	      direntriesarr[i].uniqfier = Fid.Unique;
	      direntriesarr[i].MtPt = 1;
	  }
	  strcpy(direntriesarr[i].name, dp->d_name);
	  direntriesarr[i].VV = VV;
	  direntriesarr[i].replicaid = j;
	  direntriesarr[i].lookedAt = 0;
	  count++;
	  free(path);
      }
      closedir(dirp);

      /* fill in the resreplica */
      if (res_getfid(names[j], &Fid,  &VV)) return -1;
      dirs[j].nentries = count;
      dirs[j].replicaid = Fid.Volume;
      dirs[j].vnode = Fid.Vnode;
      dirs[j].uniqfier = Fid.Unique;
      dirs[j].path = (char *)malloc(strlen(names[j]) + 1);
      strcpy(dirs[j].path, names[j]);
      /* fill in access list and modebits */
      if (stat(names[j], &buf)){
	  perror(names[j]);
	  return(-1);
      }
      dirs[j].modebits = buf.st_mode;
      if (!(dirs[j].al = res_getacl(names[j]))) /* return -1; */
	  fprintf(stderr, "\t--> getacl on \"%s\" FAILED!!!\n", names[j]);
      dirs[j].owner = buf.st_uid;
  }
  return(0);
}

/* gives the index of the next entry free in the global direntries table */
static int nextindex()
{   
    totaldirentries++;
    if (totaldirentries  >= direntriesarrsize){
	direntriesarr = (resdir_entry *)realloc(direntriesarr, (direntriesarrsize + GROWSIZE) * sizeof(resdir_entry));
	if (!direntriesarr){
	    perror("realloc");
	    exit(-1);
	}
	direntriesarrsize += GROWSIZE;
    }
    nextavailindex++;
    return(nextavailindex);
}

void MarkEntriesByFid (long Vnode, long unique)
{
    resdir_entry *direntryptr = direntriesarr;
    for(int i = 0; i < totaldirentries; i++)
	if ((direntryptr[i].vno == Vnode) && (direntryptr[i].uniqfier == unique))
	    direntryptr[i].lookedAt = 1;
    
}

/* this assumes mark_arr to be an array of resdir_entry/s to be marked */
void MarkEntriesByGroup (resdir_entry **mark_arr, int nentries)
{
    for (int i = 0; i < nentries; i++)
	mark_arr[i]->lookedAt = 1;
}

int GetConflictType (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nentries, int *conflictType, char *volmtpt)
{
    int i;
    *conflictType = STRONGLY_EQUAL;
    for (i = 0; i < nPredicates; i++) {
	if ((*(Predicates[i]))(nreplicas, dirs, deGroup, nentries))
	    break;
    }
    if (i == nPredicates){
	*conflictType = UNKNOWN_CONFLICT;
	nConflicts++;
	return	-1;
    }
    *conflictType = i;
    if (i != STRONGLY_EQUAL && i != WEAKLY_EQUAL && i != ALL_PRESENT)
	nConflicts++;
    return 0;
    
}

void InitListHdr (int nreplicas, resreplica *dirs, struct listhdr **opList)
{   struct listhdr *lh;
    *opList = lh = (struct listhdr *) malloc(nreplicas * sizeof(struct listhdr));
    for (int i = 0; i < nreplicas; i++){
	lh[i].replicaId = dirs[i].replicaid;
	lh[i].repairCount = 0;
	lh[i].repairList = NULL;
    }
}

/* inserts a copy of a repair struct into the repair ops list */
int InsertListHdr (struct repair *rep, struct listhdr **ops, int index)
{
    int size = (*ops)[index].repairCount;
    struct repair *repList;
    
    repList = (struct repair *)malloc(sizeof(struct repair) * (size + 1));
    if (repList == 0) return -1;
    if (size > 0) 
	memmove((void *) repList, (const void *)(*ops)[index].repairList, (size * sizeof(struct repair))); 
    memmove((void *)&(repList[size]), (const void *)rep, sizeof(struct repair)); 
    /*    free ((*ops)[index].repairList);  */
    ((*ops)[index]).repairList = repList; 
    ((*ops)[index]).repairCount ++; 
    return 0; 
}

/* checks if an entry exists in a repair list */
int InRepairList (struct listhdr *opList, unsigned opcode, long vnode, long unique)
{
    struct repair *repList = opList->repairList;

    for(int i = 0; i < opList->repairCount; i ++)
	if ((repList[i].opcode == opcode) && (repList[i].parms[1] == vnode) && (repList[i].parms[2] == unique))
	    return 1;
    return 0;
}

/* checks if a fid has been created by an earlier operation in a repair list */
int IsCreatedEarlier (struct listhdr **opList, int index, long vnode, long unique)
{
    struct repair *repList = (*opList)[index].repairList;
    unsigned int count = (*opList)[index].repairCount;

    for (int i = 0; i < count; i++) 
      if ((repList[i].opcode == REPAIR_CREATED) && (repList[i].parms[1] == vnode) && (repList[i].parms[2] == unique))
	return 1;
    return 0;
}

void ResolveConflict (int nreplicas, resreplica *dirs, resdir_entry **deGroup, int nentries, int conflictType, listhdr **opList, char *volmtpt, VolumeId RepVolume)
{
    /* call the appropriate repair function */
    switch (conflictType){
      case STRONGLY_EQUAL:
      case WEAKLY_EQUAL:
      case ALL_PRESENT:
	break;
      case SUBSET_RENAME:
	RepairRename(nreplicas, dirs, deGroup, nentries, opList, volmtpt, RepVolume);
	break;
      case SUBSET_CREATE:
	RepairSubsetCreate(nreplicas, dirs, deGroup, nentries, opList, RepVolume);
	break;
      case SUBSET_REMOVE:
	RepairSubsetRemove(nreplicas, dirs, deGroup, nentries, opList);
	break;
      case MAYBESUBSET_REMOVE:
	RepairSubsetCreate(nreplicas, dirs, deGroup, nentries, opList, RepVolume);
	break;
      case UNKNOWN_CONFLICT:
	printf("ResolveConflict: Unknown Conflict for %s \n", deGroup[0]->name);
	break;
      default:
	fprintf(stderr, "ResolveConflict: Unknown conflict switch \n");
	break;
    }
    /* mark the group - if conflict is resolvable */
    MarkEntriesByGroup(deGroup, nentries);
    if (conflictType == SUBSET_RENAME) 
	MarkEntriesByFid(deGroup[0]->vno, deGroup[0]->uniqfier);
}

int NameNameResolve(int first, int last, int nreplicas, resreplica *dirs, struct listhdr **opList, struct repinfo *inf) {
    printf("\n \nNAME/NAME CONFLICT EXISTS FOR %s\n\n", sortedArrByName[first]->name);

    // first print the ls output for each replica
    // get the replicated path name
    int uselsoutput = 0;
    int i;
    char replicatedname[MAXPATHLEN];
    strcpy(replicatedname, dirs[sortedArrByName[first]->replicaid].path);
    char *lastslash = rindex(replicatedname, '/'); // the trailing /
    if (!lastslash) 
	printf("Couldn't find the parent directory of %s\n", sortedArrByName[first]->name);
    else {
	*lastslash = '\0';
	lastslash = rindex(replicatedname, '/'); // the / before the replica name
	if (!lastslash)
	    printf("Couldn't find the parent directory of %s\n", sortedArrByName[first]->name);
	else {
	    uselsoutput = 1;
	    *lastslash = '\0';
	}
    }
    if (uselsoutput) {
	char cmd[2 * MAXPATHLEN];
	resdir_entry *rde;
	char *path, *replicaname;
	char cwdpath[MAXPATHLEN];
	char *cwd = getcwd(cwdpath, MAXPATHLEN);
	chdir(replicatedname);
	for (i = first; i < last; i++) {
	    rde = sortedArrByName[i];
	    path = dirs[rde->replicaid].path;  	// this has a trailing /
	    path[strlen(path)-1] = '\0'; 		// erase the last /
	    replicaname = rindex(path, '/') + 1;
	    sprintf(cmd, "/bin/ls -lF %s/%s",
		    replicaname, sortedArrByName[first]->name);
	    system(cmd);
	    path[strlen(path)]='/';	// pretty gross!
	}
	printf("\n\n");
	if (cwd) chdir(cwd);
    }
    
    for (i = first; i < last; i++) {
	resdir_entry *rde = sortedArrByName[i];
	printf("%s%s\n\tFid: (%x.%x) VV:(%d %d %d %d %d %d %d %d)(%x.%x)\n",
	       dirs[rde->replicaid].path, sortedArrByName[first]->name,
	       rde->vno, rde->uniqfier, rde->VV.Versions.Site0,
	       rde->VV.Versions.Site1, rde->VV.Versions.Site2, rde->VV.Versions.Site3,
	       rde->VV.Versions.Site4, rde->VV.Versions.Site5, rde->VV.Versions.Site6,
	       rde->VV.Versions.Site7, rde->VV.StoreId.Host, rde->VV.StoreId.Uniquifier);
    }
    int answers[MAXHOSTS];
    char nnpath[MAXPATHLEN], fixedpath[MAXPATHLEN], lnpath[MAXPATHLEN];
    struct stat sbuf;
    for (i= 0; i < MAXHOSTS; i++) answers[i] = -1;
    CODA_ASSERT((last-first) <= MAXHOSTS);
    for (i = first; i < last; i++) {
	resdir_entry *rde = sortedArrByName[i];
	sprintf(nnpath, "%s%s", dirs[rde->replicaid].path, rde->name);
	if (inf->interactive) {
	    printf("Should %s be removed? ", nnpath);
	    answers[i-first] = Parser_getbool("", 0);
	}
	else { /* Non-interactive mode
		*  -- keep file if link in fixed dir points to the replica */
	    printf("Checking \"%s\"... ", nnpath);
	    sprintf(fixedpath, "%s%s%s", inf->fixed, 
		    ((inf->fixed[(strlen(inf->fixed) - 1)] == '/') ? "" : "/"), rde->name);
	    if ((lstat(fixedpath, &sbuf) < 0) || (!(S_ISLNK(sbuf.st_mode))))
		answers[i-first] = 1; /* remove anything not explicitly saved */
	    else {
		memset(lnpath, 0, MAXPATHLEN);
		if ((readlink(fixedpath, lnpath, MAXPATHLEN - 1) < 0) /* keep data if link is bad */
		    || (strcmp(lnpath, nnpath) == 0)) /* or if it points to the replica */
		    answers[i-first] = 0; 
		else answers[i-first] = 1; 
	    }
	    printf("%s\n", (answers[i-first]) ? "remove" : "save");
	}
    }
    int nobjects = last - first;
    int nyes = 0;
    int nno = 0;
    for (i = 0; i < nobjects; i++) {
	if (answers[i] == 0) nno++;
	else if (answers[i] == 1) nyes++;
    }
    /* check for obvious problems */
    if ((nyes == nobjects) && (inf->interactive))
	    printf("WARNING: ALL REPLICAS OF OBJECT %s WILL BE REMOVED\n", sortedArrByName[first]->name);
    if ((nno == nobjects) && (inf->interactive))
	    printf("WARNING: Nothing will change; NAME/NAME conflict will remain\n");

    if (nno) {
	/* check that only single unique object will exist */
	long goodvnode = -1;
	long goodunique = -1;
	for (i = 0; i < nobjects; i++) {
	    if (!answers[i]) {
		if (goodvnode == -1) {
		    goodvnode = sortedArrByName[i+first]->vno;
		    goodunique = sortedArrByName[i+first]->uniqfier;
		}
		else if (goodvnode != sortedArrByName[i+first]->vno ||
			 goodunique != sortedArrByName[i+first]->uniqfier) {
		    printf("Please try to rename or remove one of the two objects:\n");
		    printf("(%x.%x) and (%x.%x) with name %s\n",
			   goodvnode, goodunique, sortedArrByName[i+first]->vno, 
			   sortedArrByName[i+first]->uniqfier,
			   sortedArrByName[first]->name);
		    return(-1);
		}
	    }
	}
    }

    if (nyes) {
	for (i = 0; i < nobjects; i++) {
	    struct repair rep;
	    if (answers[i]) {
		if (ISDIRVNODE(sortedArrByName[first+i]->vno))
		    rep.opcode = REPAIR_REMOVED;
		else
		    rep.opcode = REPAIR_REMOVEFSL;
		strcpy(rep.name, sortedArrByName[first+i]->name);
		rep.parms[0] = rep.parms[1] = rep.parms[2] = 0;
		InsertListHdr(&rep, opList, sortedArrByName[first+i]->replicaid);
	    }
	}
	return(1);
    }
    return(0);
}

/* dirresolve : returns NNCONFLICTS(-1) if this resolve is definitely not the last needed compare/repair 
   return 0 if the compare implied that the resulting repair will make the directories equal */
int dirresolve (int nreplicas, resreplica *dirs, int (*cbfn)(char *), struct listhdr **opList, 
		char *volmtpt, VolumeId RepVolume, struct repinfo *inf)
{
    int i;

    /* form the array of pointers to sort the dir entries */
    sortedArrByName = (resdir_entry **) malloc(totaldirentries * sizeof(resdir_entry *));
    for (i = 0; i < totaldirentries; i++)
	sortedArrByName[i] = direntriesarr + i;

    qsort(sortedArrByName, totaldirentries,  sizeof(resdir_entry *),
	  (int (*)(const void *, const void *))resdirCompareByName);
	
    nConflicts = 0;
    InitListHdr(nreplicas, dirs, opList);

    /* detect and correct name/name conflicts first */
    int first = 0;
    int next = 1;
    while (first < totaldirentries) {
	while ((next < totaldirentries) && 
	       !(strcmp(sortedArrByName[next]->name, sortedArrByName[first]->name)))
	    next++;
	
	for (i = first+1; i < next; i++) {
	    if ((sortedArrByName[i]->vno != sortedArrByName[first]->vno) || 
		(sortedArrByName[i]->uniqfier != sortedArrByName[first]->uniqfier)) {
		/* name/name conflict exists - process it */
		if (NameNameResolve(first, next, nreplicas, dirs, opList, inf)) {
		    nConflicts++;
		    break;
		}
	    }
	}
	first = next;
    }

    free(sortedArrByName);
    if (nConflicts) 
	return(NNCONFLICTS);
    
    /* group the array by fid and pass them to the predicates */
    sortedArrByFidName = (resdir_entry **) malloc(totaldirentries * sizeof(resdir_entry *));
    for (i = 0; i < totaldirentries; i++)
	sortedArrByFidName[i] = direntriesarr + i;
    qsort(sortedArrByFidName, totaldirentries,  sizeof(resdir_entry *), 
	  (int (*)(const void *, const void *))resdirCompareByFidName);
    int j;
    for (i = 0, j = 1; i < totaldirentries; i += j){
	int conflict, rc;
	for (j = 1; (i + j) < totaldirentries; j ++)
	    if (resdirCompareByFidName(&(sortedArrByFidName[i]), &sortedArrByFidName[i+j]))
	 	break;
	if (sortedArrByFidName[i]->lookedAt) continue;
	rc = GetConflictType(nreplicas, dirs, &(sortedArrByFidName[i]), j, &conflict, volmtpt);
	if (rc){
	    if (inf->interactive)
		printf("**** Couldnt get conflict type for %s ****\n", sortedArrByFidName[i]->name);
	    nConflicts++;
	}
	else 
	    ResolveConflict(nreplicas, dirs, &(sortedArrByFidName[i]), j, conflict, opList, volmtpt, RepVolume);
    }
    free(sortedArrByFidName);
    return 0;
}

int resdirCompareByName (resdir_entry **a, resdir_entry **b)
{
    return(strcmp((*a)->name, (*b)->name));
}

/* this sorts by Fid as the primary index and the name as the secondary index */
int resdirCompareByFidName (resdir_entry **a, resdir_entry **b)
{
    if ((u_long)((*a)->vno) < (u_long)((*b)->vno)) return -1;
    else if ((u_long)((*a)->vno) > (u_long)((*b)->vno)) return 1; 
    if ((u_long)((*a)->uniqfier) < (u_long)((*b)->uniqfier)) return -1;
    else if ((u_long)((*a)->uniqfier) > (u_long)((*b)->uniqfier)) return 1;
    return (strcmp((*a)->name, (*b)->name));
}

/* clean up routine */
void resClean (int nreplicas, resreplica *dirs, struct listhdr *lh)
{
    int i;
    if (dirs){
	for (i = 0; i < nreplicas; i++){
	    if (dirs[i].path) free(dirs[i].path);
	    if (dirs[i].al) FreeAcl(dirs[i].al);
	}
	free(dirs);
    }
    if (lh){
	for (i = 1; i < nreplicas; i++)
	    if (lh[i].repairList)
		free(lh[i].repairList);
	free(lh);
    }
    /* free up the global array of dir entries */
    free(direntriesarr);
}

int GetParent(VenusFid *cfid, VenusFid *dfid, char *volmtpt, char *dpath, char *childname) {
    /* returns fid and absolute path of parent */
    int rc;
    struct ViceIoctl vi;
    char tmp[2048];
    char path[MAXPATHLEN];

    /* first get the path of the child relative to vol root */
    vi.in = (char *)cfid;
    vi.in_size = sizeof(VenusFid);
    vi.out = tmp;
    vi.out_size = sizeof(tmp);
    memset(tmp, 0, sizeof(tmp));

    strcpy(path, "/coda");
    rc = pioctl(path, VIOC_GETPATH, &vi, 0);
    if (rc) {
	//printf("GetParent: Getpath returns error %d, errno = %d\n",
	//rc, errno);
	return(rc);
    }

#if 0 /* this seems to be wrong, replaced it by conditionalizing the
	 strcpy a few lines down. --JH */
    if (!volmtpt) {
	strcpy(dpath, path);
	return(0);
    }
#endif

    /* form the absolute path name of the parent */
    char *lastcomp = rindex(tmp, '/');
    if (lastcomp) 
	*lastcomp = '\0';
    char *firstcomp = index(tmp, '/');

    /* if a volmtpt has not been passed along use "/coda", which already
     * happens to be what path contains. --JH */
    if (volmtpt) strcpy(path, volmtpt);

    if (firstcomp && (firstcomp != lastcomp)) {
	strcat(path, "/");
	strcat(path, firstcomp + 1);
    }
    strcpy(dpath, path);
    strcpy(childname, lastcomp + 1);

    // get fid of parent 
    //ViceVersionVector VV;
    //res_getfid(path, dfid, &VV);
    vi.in = (char *)cfid;
    vi.in_size = sizeof(VenusFid);
    vi.out = tmp;
    vi.out_size = sizeof(tmp);
    strcpy(path, "/coda");
    rc = pioctl(path, VIOC_GETPFID, &vi, 0);
    if (rc) {
	printf("Error %d occured while trying to get fid of %s's parent\n", childname);
	return(rc);
    }
    memcpy(dfid, tmp, sizeof(VenusFid));
    return(0);
}
