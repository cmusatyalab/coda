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

#include "repair.h"

int interactive = 1, session = NOT_IN_SESSION;
char compDirDefault[MAXPATHLEN];
char compOutputFile[MAXPATHLEN]; /* filename for output of last docompare */
char beginRepDefault[MAXPATHLEN];
char doRepDefault[MAXPATHLEN];
int repair_DebugFlag;
struct stat compOutputStatBuf;	 /* file information for the repair commands file */
struct stat doInputStatBuf;

extern int IsCreatedEarlier(struct listhdr **, int, long, long);

void SetDefaultPaths();
int  compareVV(int, char **, struct repvol *);
void GetArgs(int argc, char *argv[]);
int  getcompareargs(int, char **, char *, char *, char **, char **, char **, char **);
int  getremoveargs(int, char **, char *);
void getremovelists(int, resreplica *, struct listhdr **);
int  getrepairargs(int, char **, char *, char *, char *);
int  makedff(char *extfile, char *intfile);
int  doCompare(int, struct repvol *, char **, char *, char *, ViceFid *, char *, char *, char *, char *);
int  compareStatus(int, resreplica *);
int  compareQuotas(int , char **);
int  compareOwner(int, resreplica *);
void printAcl(struct Acl *);

int  compareAcl(int, resreplica *);
int  GetReplicaNames(char **, int , char *);
int  GetTokens();
void help(int argc, char **argv);
void INT(int, int, struct sigcontext *);

/* args: <reppathname> <fixfilename> */
void rep_DoRepair(int largc, char **largv) {
  enum {FIXDIR, FIXFILE} fixtype;
  VolumeId vid;
  struct repvol *repv;
  struct volrep *rwv;
  struct repvol *saverepv;
  struct stat statbuf;
  struct ViceIoctl vioc;
  char space[DEF_BUF];
  int i, rc;
  char uconflictpath[MAXPATHLEN], ufixpath[MAXPATHLEN];
  char realfixpath[MAXPATHLEN];
  char reppath[MAXPATHLEN], tmppath[MAXPATHLEN];
  char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
  VolumeId vids[MAXHOSTS];
  long rcodes[MAXHOSTS];

  switch (session) {
    case LOCAL_GLOBAL:
      printf("\"dorepair\" can only be used to repair a server/server conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  /* Obtain parameters and confirmation from user */
  rc = getrepairargs(largc, largv, uconflictpath, ufixpath, realfixpath);
  if (rc < 0) return;

  /* Is this the leftmost element in conflict? */
  rc = repair_isleftmost(uconflictpath, reppath, MAXPATHLEN); 
  if (rc < 0) return;

  /* Is the volume locked for repair? */
  rc = repair_getmnt(reppath, prefix, suffix, &vid);
  if (rc < 0) return;
  if ((repv = repair_findrep(vid)) == NULL) {
    printf("You must do \"beginrepair\" first\n"); 
    return;
  }
  saverepv = repv;
  /* Is it a directory?
     Assumption: rw replicas are all of the same type.
     This must be true if this is a leftmost conflict
  */
  CODA_ASSERT(repv->rwhead); /* better have one rw replica at least! */
  rwv = repv->rwhead;
  while (rwv != 0) {
    sprintf(tmppath, "%s/%s", reppath, rwv->compname);
    rc = lstat(tmppath, &statbuf);

    /* If it's a symlink, that rep must not be available, skip over it. */
    if ((statbuf.st_mode & S_IFMT) == S_IFLNK) rc = -1;

    if (rc >= 0) break;
    else rwv = rwv->next;
  }
  if (rc < 0) {
    repair_perror(" lstat", tmppath, errno);  
    printf("No replicas accessible\n");
    printf("Possible causes: disconnection or lack of authentication\n");
    return;
  }
  if ((statbuf.st_mode & S_IFMT) == S_IFDIR) fixtype = FIXDIR;
  else fixtype = FIXFILE;

  if (fixtype == FIXDIR) rc = makedff(realfixpath, tmppath);     /* Create internal form of fix file */
  if (rc < 0) { 
    printf("Error in the fix file\n");
    return;
  }

  /* The following code for collecting repair stats is ifdef'ed out.
   * If anyone needs to resume stats collection, please redefine DOREPAIRDATADIR. */
#ifdef REPAIR_STATS
  /* collect statistics about the repair */
  int collectstats = 1;
  int repfilemodified = 0;
  int changesdifflevel = -1;
  if (fixtype == FIXDIR) {
    if (compOutputFile[0]) {
      if (stat(realfixpath, &doInputStatBuf)) {
	printf("Couldn't stat file %s\n", realfixpath);
	collectstats = 0;
      }
      else {
	if ((compOutputStatBuf.st_ino != doInputStatBuf.st_ino) ||
	    (compOutputStatBuf.st_size != doInputStatBuf.st_size) ||
	    (compOutputStatBuf.st_mtime != doInputStatBuf.st_mtime))
	  repfilemodified = 1;
	if (repfilemodified) {
	  printf("The repair file was modified.\nHow difficult was it to change?");
	  while ((changesdifflevel < 0) || (changesdifflevel > 5)) {
	    printf("(0 Easy, 5 Difficult)");
	    scanf("%d", &changesdifflevel);
	  }
	}
      }
    }
    else collectstats = 0;
  }
#endif REPAIR_STATS

  /* Do the repair */
  if (fixtype == FIXDIR) {
    vioc.in_size = (short)(1+strlen(tmppath));
    vioc.in = tmppath;
  }
  else {
    vioc.in_size = (short)(1 + strlen(ufixpath));
    vioc.in  = ufixpath;
  }
  vioc.out_size = (short)sizeof(space);
  vioc.out = space;
  memset(space, 0, (short)sizeof(space));
  rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
  if (rc < 0 && errno != ETOOMANYREFS) 
    repair_perror(" REPAIR", reppath, errno);
  {	
    long *l;
    l = (long *) space;
    for (i = 0; i < MAXHOSTS; i++)
      vids[i] = l[i];
    for (i = 0; i < MAXHOSTS; i++)
      rcodes[i] = l[i+MAXHOSTS];
    for (i = 0; i < MAXHOSTS; i++) {
      if (vids[i]) {
	/* find server name */
	rwv = saverepv->rwhead;
	while(rwv && vids[i] != rwv->vid)
	  rwv = rwv->next;
	CODA_ASSERT(vids[i] == rwv->vid);
	printf("repair actions performed on %s have %s ",
	       rwv->srvname, rcodes[i] ? "failed" : "succeeded");
	if (rcodes[i]) {
	  printf("(%ld)\n", rcodes[i]); 
	  printf("Possible causes: disconnection, lack of authentication, lack of server space\n");	
	  printf("fix file contains operations that are in conflict against the server replica .... \n");
	} 
	else printf("\n");
      }
    }
  }

  /* The following code for collecting repair stats is ifdef'ed out.
   * If anyone needs to resume stats collection, please redefine DOREPAIRDATADIR.
   */
#ifdef REPAIR_STATS
  if (collectstats) {
    /* open the statistics file */
    char statfilename[MAXPATHLEN];
    uid_t uid = getuid();
    sprintf(statfilename, "%s%u.XXXXXX", DOREPAIRDATADIR, uid);
    int statfd = mkstemp(statfilename);
    if (statfd>0) {
      FILE *fp = fdopen(statfd, "w");
      CODA_ASSERT(fp);
      if (fixtype == FIXDIR) {
	fprintf(fp, "Kind of Repair: Directory\n");
	fprintf(fp, "Repair file changed: %d\n", repfilemodified);
	fprintf(fp, "Level of difficulty for changes %d\n", changesdifflevel);
	fprintf(fp, "The repair file was as follows:\n");
	FILE *repfp = fopen(realfixpath, "r");
	if (repfp) {
	  char s[MAXPATHLEN];
	  while (fgets(s, MAXPATHLEN, repfp))
	    fprintf(fp, "%s", s);
	  fclose(repfp);
	}
	else fprintf(fp, "Couldn't open the repair file\n");
      }
      else {
	fprintf(fp, "Kind of Repair: File\n");
	fprintf(fp, "Path of repair file: %s\n", ufixpath);
      }

      fprintf(fp, "Return codes from the repair were:\n");
      /* print the return codes */
      for (i = 0; i < MAXHOSTS; i++) {
	if (vids[i]) {
	  /* find server name */
	  rwv = saverepv->rwhead;
	  while(rwv && vids[i] != rwv->vid)
	    rwv = rwv->next;
	  CODA_ASSERT(vids[i] == rwv->vid);
	  fprintf(fp, "%s %s\n", 
		  rwv->srvname, rcodes[i] ? "failed" : "succeeded");
	}
      }
      fclose(fp);
      close(statfd);
    }
  }
#endif REPAIR_STATS

  /* Clean up */
  if (fixtype == FIXDIR) unlink(tmppath); /* ignore rc */
}

/* args: <reppathname> <fixfilename> */
void rep_CompareDirs(int largc, char **largv) {
  int nreplicas;
  char **names;
  int rc, i;
  int sizeOfPath;
  VolumeId vid;
  struct repvol *repv;
  char reppath[MAXPATHLEN], filepath[MAXPATHLEN];
  char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
  char tmppath[MAXPATHLEN];
  char uconflictpath[MAXPATHLEN];
  ViceFid confFid;
  vv_t confvv;
  struct stat buf;
  char *user;
  char *rights;
  char *owner;
  char *mode;

  user = rights = owner = mode = NULL;

  switch (session) {
    case LOCAL_GLOBAL:
      printf("\"compardirs\" can only be used to repair a server/server conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  /* Obtain parameters from user */
  rc = getcompareargs(largc, largv, uconflictpath, filepath, &user, &rights, &owner, &mode);
  if (rc < 0) return;

  /* Is this the leftmost element in conflict? */
  rc = repair_isleftmost(uconflictpath, reppath, MAXPATHLEN); 
  if (rc < 0) return;

  /* Is the volume locked for repair */
  rc = repair_getmnt(reppath, prefix, suffix, &vid);

  if (rc < 0) return;
  if ((repv = repair_findrep(vid)) == NULL) {
    printf("You must do \"beginrepair\" first\n"); 
    return;
  }
    
  CODA_ASSERT(repv->rwhead);   /* better have atleast one rw replica */

  if (repair_getfid(uconflictpath, &confFid, &confvv)) {
    printf("%s isn't in /coda... Are you sure this object is in conflict\n", uconflictpath);
    return;
  }
  if (!ISDIR(confFid.Vnode)) {
    printf("You can only compare directory replicas\n");
    return;
  }
  /* initialize the array of ropaths */
  nreplicas = repair_countRWReplicas(repv);
  sizeOfPath = (int) (strlen(repv->rodir) + 15 + strlen(suffix));
  names = (char **)malloc(nreplicas * sizeof(char *));
  nreplicas = GetReplicaNames(names, nreplicas, reppath);
  if (nreplicas > 0) {
    rc = lstat(names[0], &buf);
    if (rc < 0)  goto Exit;
    if ((buf.st_mode & S_IFMT) != S_IFDIR) {
      printf("Compare can only be performed on directories\n");
      return;
    }
  }
  else {
    printf("Couldn't get replicas to do compare\n");
    return;
  }
    
 redocompare:
  /* do the compare */
  rc = doCompare(nreplicas, repv, names, filepath, prefix, &confFid, user, rights, owner, mode);
  if (!rc) printf("No conflicts detected \n");
  else if (rc > 0) printf("Operations to resolve conflicts are in %s\n", filepath);	
  else if (rc == NNCONFLICTS) {
    if (Parser_getbool("Do you want to repair the name/name conflicts", 1)) {
      /* repair the name/name conflicts */
      struct ViceIoctl vioc;
      char space[DEF_BUF];
      rc = makedff(filepath, tmppath);
      vioc.in_size = (short) (1+strlen(tmppath));
      vioc.in = tmppath;
      vioc.out_size = (short) sizeof(space);
      vioc.out = space;
      memset(space, 0, (int)sizeof(space));
      rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
      if (!rc) {
	/* name/name conflicts were repaired 
	   if the only thing wrong was the n/n conflict, then the object isn\'t inconsistent anymore.
	   try to stat the individual replica - if it fails then just exit */
	struct stat buf;
	if (stat(names[0], &buf)) {
	  /* object doesn't exist anymore */
	  printf("The directory was repaired and is no longer inconsistent\n");
	  printf("You can quit now :-)\n");
	  goto Exit;
	}
	else /* the object is still there so the dir is still inconsistent */
	  goto redocompare;
      }
      else  {
	printf("Couldn't remove name/name conflicts successfully");
	printf(" - Try another compare\n");
      }
    }
  }
 Exit:
  /* free the malloced space */
  for ( i = 0; i < nreplicas; i++)
    free(names[i]);
  free(names);
}

int allowclear = 0;
void rep_ClearInc(int largc, char **largv) {
  int rc, i;
  char reppath[MAXPATHLEN], uconflictpath[MAXPATHLEN];
  char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
  VolumeId vid;
  struct repvol *repv;
  int sizeOfPath;
  char **names;
  int nreplicas;
  char *user;
  char *rights;
  char *owner;
  char *mode;

  user = rights = owner = mode = NULL;

  if (!allowclear) {
    printf("Clear Inconsistency: This command is obsolete.");
    printf("You don't need to use this anymore\n");
    return;
  }

  if (largc == 1)
    Parser_getstr("Pathname of Object that should be made consistent?", doRepDefault, uconflictpath, MAXPATHLEN);
  else {
    if (largc != 2) {
      printf("clearinc <pathname>\n");
      return;
    }
    strncpy(uconflictpath, largv[1], MAXPATHLEN);
  }

  /* Is this the leftmost element in conflict? */
  rc = repair_isleftmost(uconflictpath, reppath, MAXPATHLEN); 
  if (rc < 0) return;

  /* Is the volume locked for repair */
  rc = repair_getmnt(reppath, prefix, suffix, &vid);
  if (rc < 0) return;
  if ((repv = repair_findrep(vid)) == NULL) {
    printf("You must do \"beginRepair\" first\n"); 
    return;
  }

  CODA_ASSERT(repv->rwhead);   /* better have atleast one rw replica */

  /* initialize the array of ropaths */
  nreplicas = repair_countRWReplicas(repv);
  sizeOfPath = (int) (strlen(repv->rodir) + 15 + strlen(suffix));
  names = (char **)malloc(nreplicas * sizeof(char *));
  nreplicas = GetReplicaNames(names, nreplicas, reppath);
  if (nreplicas <= 0) {
    printf("Clear Inconsistency: Couldn't get enough replicas to compare\n");
    return;
  }
  ViceFid confFid;
  confFid.Volume = vid;
  confFid.Vnode = 0;	// set the fid to 0.0 so docompare will not check quotas
  confFid.Unique = 0; 
  /* do the compare */
  if (!doCompare(nreplicas, repv, names, "/dev/null", prefix, &confFid, user, rights, owner, mode)) {
    ViceFid Fid[MAXHOSTS];
    vv_t vv[MAXHOSTS];
    struct ViceIoctl vioc;

    /* now for each replica get the vv */
    /* XXX - if a get fid is done between two setvv's resolve might 
       get called - therefore get all the vv's before doing the setvv */
    for (i = 0; i < nreplicas; i++){
      if ((rc = repair_getfid(names[i], &Fid[i], &vv[i])) < 0){
	printf("Error in repair_getfid(%s)\n", names[i]);
	return;
      }
    }
    if ((Fid[0].Vnode == 1) && (Fid[0].Unique == 1)) {
      printf("Comparing Volume Quotas...\n");
      if (compareQuotas(nreplicas, names)) 
	printf("Warning: Volume Quotas are different\n");
      else 
	printf("No Conflicts detected \n");		
    }
    else 
      printf("No Conflicts detected \n");

    printf("Clearing inconsistency flags ... \n");
    for (i = 0; i < nreplicas; i++) {
      ClearIncon(vv[i]);
      vioc.in_size = (short)sizeof(vv_t);
      vioc.in = (char *)&vv[i];
      vioc.out_size = 0;
      vioc.out = 0;
      rc = pioctl(names[i], VIOC_SETVV, &vioc, 0);
      if (rc){
	repair_perror("SETVV", names[i], errno);
	return;
      }
    }
  }
  else
    printf("Since replicas are not yet identical, inconsistency still remains\n");

  /* clean up */
  for ( i = 0; i < nreplicas; i++)
    free(names[i]);
  free(names);
}

void rep_RemoveInc(int largc, char **largv) {
  int rc, dirconf;
  struct repvol *repv;
  char pathname[MAXPATHLEN];

  switch (session) {
    case NOT_IN_SESSION:
      printf("please use \"beginrepair\" first to determine the nature of the conflict\n");
      return;
      break;
    case LOCAL_GLOBAL:
      printf("\"removeinc\" can only be used to repair a server/server conflict\n");
      return;
      break;
    case SERVER_SERVER:
      printf("\"removeinc\" will terminate the current repair session\n");
      break;
  }

  /* Obtain parameters */
  if (getremoveargs(largc, largv, pathname) < 0) {
    printf("Could not determine pathname in conflict\n");
    return;
  }

  /* remove the inconsistency */
  rc = RemoveInc(pathname, &repv, &dirconf);

  /* end the repair session */
  if (EndRepair(repv, session, 0) < 0) {
    printf("Error ending repair session.\n");
    exit(2);
  }
  else session = NOT_IN_SESSION;

  if (!rc) {
    /* no error - try to remove the object */
    if (((dirconf) ? rmdir(pathname) : unlink(pathname)) < 0)
      printf("Could not remove %s\n", pathname);
  }
}

/* removes inconsistencies:  first does a repair, then clears the inc */
int RemoveInc(char *uconflictpath, struct repvol **retv, int *dirconf) {
  int nreplicas, sizeOfPath, rc, i, j;
  char reppath[MAXPATHLEN], tmppath[MAXPATHLEN];
  char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
  VolumeId vid;
  struct repvol *repv;
  struct volrep  *rwv;
  struct listhdr *repairlist;
  resreplica *dirs;
  struct stat buf;
  FILE *file;
  enum {FIXDIR, FIXFILE, UNKNOWN} fixtype;
  char *user, *rights, *owner, *mode, **names;

  user = rights = owner = mode = NULL;

  rc = 0;
  fixtype = UNKNOWN;

  /* Is this the leftmost element in conflict? */
  rc = repair_isleftmost(uconflictpath, reppath, MAXPATHLEN); 
  if (rc < 0) return(-1);

  /* Is the volume locked for repair */
  rc = repair_getmnt(reppath, prefix, suffix, &vid);
  if (rc < 0) return(-1);
  if ((repv = repair_findrep(vid)) == NULL) {
    printf("You must do \"beginrepair\" first\n"); 
    return(-1);
  }
    
  CODA_ASSERT(repv->rwhead);   /* better have atleast one rw replica */

  /* get repair type */
  {
    rwv = repv->rwhead;
    while (rwv != 0) {
      sprintf(tmppath, "%s/%s", repv->rodir, rwv->compname);
      rc = lstat(tmppath, &buf);  
      if (rc >= 0) break;
      else rwv = rwv->next;
    }
    if (rc < 0) {
      repair_perror(" lstat", tmppath, errno);  
      printf("No replicas accessible\n");
      return(-1);
    }
    if ((buf.st_mode & S_IFMT) == S_IFDIR) fixtype = FIXDIR;
    else fixtype = FIXFILE;
  }
    
  /* initialize the array of ropaths */
  nreplicas = repair_countRWReplicas(repv);
  sizeOfPath = (int) (strlen(repv->rodir) + 15 + strlen(suffix));
  names = (char **)malloc(nreplicas * sizeof(char *));
  nreplicas = GetReplicaNames(names, nreplicas, reppath);

  if (nreplicas <= 0) {
    printf("No replica is accessible!\n");
    printf("Possible causes: disconnection or lack of authentication\n");
    rc = -1;
    goto Error;
  }

  if (fixtype == FIXFILE) {
    ViceFid fixfid;
    vv_t fixvv;
    struct ViceIoctl vioc;
    char space[DEF_BUF];

    rc = repair_getfid(names[0], &fixfid, &fixvv);
    if (rc) {
      printf("Could not get fid for %s\n", names[0]);
      printf("Possible causes: disconnection or lack of authentication\n");
      goto Error;
    }

    sprintf(tmppath, "@%lx.%lx.%lx", fixfid.Volume, fixfid.Vnode, fixfid.Unique);

    /* do the repair */
    vioc.in_size = (short) (1+strlen(tmppath));
    vioc.in = tmppath;
    vioc.out_size = (int)sizeof(space);
    vioc.out = space;
    memset(space, 0, (int)sizeof(space));
    rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
    if (rc < 0 && errno != ETOOMANYREFS) 
      repair_perror(" REPAIR", reppath, errno);
  }
  else {
    /* get the directory entries and create list of children to be removed */
    if (getunixdirreps(nreplicas, names, &dirs)) {
      printf("removeinc failed because of failure in getting needed replica information\n");
      rc = -1;
      goto Error;	    
    }
    getremovelists(nreplicas, dirs, &repairlist);

    if (repair_DebugFlag) {
      file = stdout;
      for (i = 0; i < nreplicas; i++){
	/* find the server name */
	{
	  rwv = repv->rwhead;
	  while (rwv && repairlist[i].replicaId != rwv->vid)
	    rwv = rwv->next;
	  CODA_ASSERT(repairlist[i].replicaId == rwv->vid);
	}
	fprintf(file,"\nreplica %s %lx \n", rwv->srvname, 
		repairlist[i].replicaId);
	for (j = 0; j < repairlist[i].repairCount; j++)
	  repair_printline(&(repairlist[i].repairList[j]), file);
      }
    }

    /* convert list to internal format */
    strcpy(tmppath, "/tmp/REPAIR.XXXXXX");
    mktemp(tmppath);

    /* write out internal rep */
    rc = repair_putdfile(tmppath, nreplicas, repairlist);
    if (rc) {
      printf("Coudn't put repair list into file %s\n", tmppath);
      goto Error;
    }

    /* Do the repair */
    {
      struct ViceIoctl vioc;
      char space[DEF_BUF];

      vioc.in_size = (short)(1+strlen(tmppath));
      vioc.in = tmppath;
      vioc.out_size = (short)sizeof(space);
      vioc.out = space;
      memset(space, 0, (int)sizeof(space));
      rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
      if (rc < 0 && errno != ETOOMANYREFS) 
	repair_perror(" REPAIR", reppath, errno);
      /* Clean up */
      unlink(tmppath); 
    }

    /* clear the inconsistency if needed and possible */
    {
      ViceFid confFid; 
      ViceVersionVector confvv;
      if ((!repair_getfid(uconflictpath, &confFid, &confvv)) && 
	  ((confvv.StoreId.Host == -1) && (confvv.StoreId.Uniquifier == -1))) {
	// object is still inconsistent ... try to clear inconsistency
	// confFid is 0.0 so that doCompare will not check quotas
	confFid.Volume = confFid.Vnode = confFid.Unique = 0;
	if (!doCompare(nreplicas, repv, names, "/dev/null", 
		       prefix, &confFid, user, rights, owner, mode)) {
	  ViceFid Fid[MAXHOSTS];
	  vv_t vv[MAXHOSTS];
	  struct ViceIoctl vioc;
	  printf("No Conflicts exist in directory\n");
	  printf("Clearing Inconsistency flags.....\n");

	  /* now for each replica get the vv and  clear the inconsistency  */
	  for (i = 0; i < nreplicas; i++){
	    if ((rc = repair_getfid(names[i], &Fid[i], &vv[i])) < 0){
	      printf("Error in repair_getfid(%s)\n", names[i]);
	      goto Error;
	    }
	  }
	  for (i = 0; i < nreplicas; i++) {
	    ClearIncon(vv[i]);
	    vioc.in_size = (short) sizeof(vv_t);
	    vioc.in = (char *)&vv[i];
	    vioc.out_size = 0;
	    vioc.out = 0;
	    rc = pioctl(names[i], VIOC_SETVV, &vioc, 0);
	    if (rc){
	      repair_perror("SETVV", names[i], errno);
	      goto Error;
	    }
	  }
	}
	else {
	  printf("Replicas are still not equal - cannot remove\n");
	  goto Error;
	}
      }
    }
  }
    
 Error:
  /* clean up malloced memory */
  if (fixtype == FIXDIR) 
    resClean(nreplicas, dirs, repairlist);

  /* free the malloced space */
  for ( i = 0; i < nreplicas; i++)
    free(names[i]);
  free(names);

  if (retv != NULL) *retv = repv;
  if (dirconf != NULL) *dirconf = (fixtype == FIXDIR);
  return(rc);
}

void rep_EndRepair(int largc, char **largv) {
  int commit;

  switch (session) {
    case NOT_IN_SESSION:
      printf("There is no repair session to end\n");
      fflush(stdout);
      return;
      break;
    case LOCAL_GLOBAL:
      if ((!interactive) || (Parser_getbool("Commit the local-global repair session?", 1)))
	commit = 1;
      else commit = 0;

      if (EndRepair(NULL, session, commit) < 0) {
	printf("Error ending repair session.\n");
	exit(2);
      }
      break;
    case SERVER_SERVER: 
      if (EndRepair(RepVolHead, session, 0) < 0) {
	printf("Error ending repair session.\n");
	exit(2);
      }
      break;
    default:
      printf("Unknown session type\n");
      exit(1);
      break;
  }
  session = NOT_IN_SESSION;
  printf("Repair session completed.\n");
  fflush(stdout);
}

int EndRepair(struct repvol *rvhead, int type, int commit) {
  struct ViceIoctl vioc;
  struct repvol *repv;
  char space[DEF_BUF], cmd[32];
  int rc, die = 0;

  if (type == LOCAL_GLOBAL) {
    vioc.out = space;
    vioc.out_size = sizeof(space);
    sprintf(cmd, "%d %d", REP_CMD_END, commit);
    vioc.in = cmd;
    vioc.in_size = (short)(strlen(cmd) + 1);
    errno = 0;
    if ((rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0)) < 0) {
      perror("VIOC_REP_CMD(REP_CMD_END)");
      return(-1);
    }
    if (strcmp(vioc.out, "repair session completed") != 0)
      printf("%s\n", vioc.out);
  }
  else if (type == SERVER_SERVER) {
    vioc.out = 0;
    vioc.out_size = 0;
    vioc.in_size = 0;
    if (rvhead != RepVolHead) {
      repair_unlinkrep(rvhead);
      errno = 0;
      if ((rc = pioctl(rvhead->mnt, VIOC_DISABLEREPAIR, &vioc, 0)) < 0) {
	repair_perror(" DISABLEREPAIR", rvhead->mnt, errno);
	die = 1;
      }
      repair_finish(rvhead);
    }
    else while (RepVolHead) { /* new element at head on each iteration */
      repv = RepVolHead;
      repair_unlinkrep(repv);
      errno = 0;
      if ((rc = pioctl(repv->mnt, VIOC_DISABLEREPAIR, &vioc, 0)) < 0) {
	repair_perror(" DISABLEREPAIR", repv->mnt, errno);
	die = 1;
      }
      repair_finish(repv);
    }
    if (die) return(-1);
  }
  else {
    printf("Unknown session type (%d)\n", type);
    exit(2);
  }
  return(0);
}

void rep_Exit(int largc, char **largv) {
  /* terminate the current session, if there is one */
  if (session != NOT_IN_SESSION) 
    EndRepair(RepVolHead, session, 1);

  /* exit the repair tool */
  Parser_exit(largc, largv);
}


/* user passed path (realpath) is what the user gives as the fixfile.
 * if the object is in coda then fixpath contains the @fid 
 * representation of the object
 */
int getrepairargs(int largc, char **largv, char *conflictpath, char *fixpath, char *realpath) {
  ViceFid fixfid;
  vv_t fixvv;

  if (largc == 1) {
    strncpy(conflictpath, doRepDefault, MAXPATHLEN);
    strncpy(fixpath, compDirDefault, MAXPATHLEN);
  } 
  else if ( largc == 3 ) {
    strncpy(conflictpath, largv[1], MAXPATHLEN);
    strncpy(fixpath, largv[2], MAXPATHLEN);
  }
  else {
    printf("%s {object fixfile }\n", largv[0]);
    return(-1);
  }
  if (!repair_getfid(fixpath, &fixfid, &fixvv)) {
    printf("%s is in Coda and cannot be used as the fix file\n", fixpath);
    return(-1); 
  }
  strncpy(realpath, fixpath, MAXPATHLEN);
  return 0;

#if 0    
  char msg[2*MAXPATHLEN+100];

  if (largc == 1) {
    Parser_getstr("Pathname of object in conflict?", 
		  (*doRepDefault == '\0') ? beginRepDefault : doRepDefault,
		  conflictpath, MAXPATHLEN);
    Parser_getstr("Pathname of fix file?", 
		  (*compDirDefault == '\0') ? "/tmp/fix" : compDirDefault, fixpath,
		  MAXPATHLEN);
  } 
  else {
    if (largc != 3) {
      printf("%s <object> <fixfile>\n", largv[0]);
      return(-1);
    }
    strncpy(conflictpath, largv[1], MAXPATHLEN);
    strncpy(fixpath, largv[2], MAXPATHLEN);
  }

  strcpy(doRepDefault, conflictpath);
  strcpy(realpath, fixpath);
  strcpy(compDirDefault, fixpath);
  if (!repair_getfid(fixpath, &fixfid, &fixvv)) {
    printf("%s is in Coda and cannot be used as the fix file\n", fixpath);
    return(-1); 
    /*	sprintf(fixpath, "@%lx.%lx.%lx",  
	fixfid.Volume, fixfid.Vnode, fixfid.Unique); */
  }

  sprintf(msg, "OK to repair \"%s\" by fixfile \"%s\"?", conflictpath, fixpath);
  if (!Parser_getbool(msg, 0)) return(-1);
  else return(0);    
  return 0;
#endif
}

int getcompareargs(int largc, char **largv, char *reppath, char *filepath,
			  char **user, char **rights, char **owner, char **mode) {
  int j;
  if (largc == 1 ) 
    goto exit;

  strncpy(filepath, largv[1], MAXPATHLEN);

  *user = NULL;
  *rights = NULL;
  *owner = NULL;
  *mode = NULL;

  for ( j = 2; j < largc ; j++ ) {
    if ( strcmp(largv[j], "-acl") == 0 ) {
      if ( largc < j+3 ) 
	goto exit;
      *user = largv[j+1];
      *rights = largv[j+2];
      j = j + 2;
    }
    if ( strcmp(largv[j], "-owner") == 0 ) {
      if ( largc < j+2 ) 
	goto exit;
      *owner = largv[j+1];
      j = j+1;
    }
    if ( strcmp(largv[j], "-mode") == 0) {
      if ( largc < j+2 ) 
	goto exit;
      *mode = largv[j+1];
      j = j+1;
    }
  }

  while (*filepath && (strncmp(filepath, "/coda", 5) == 0)) {
    if (*filepath)
      printf("Please use a fix file not in /coda \n");
    Parser_getstr("Fix file?", "", filepath, MAXPATHLEN);
  }

  strncpy(reppath, doRepDefault, MAXPATHLEN);
  strncpy(compDirDefault, filepath, MAXPATHLEN);
  return 0;

exit:
  printf("%s  <fixfile> { -acl user rights } { -owner uid} {-mode mode}\n", largv[0]);
  return(-1);
}

int getremoveargs(int largc, char **largv, char *uconfpath) {
  if (largc == 1)
    Parser_getstr("Pathname of Object in conflict?", doRepDefault, uconfpath, MAXPATHLEN);
  else {
    if (largc != 2) {
      printf("%s <object>\n", largv[0]);
      return(-1);
    }
    strncpy(uconfpath, largv[1], MAXPATHLEN);
  }

  return(0);
}


int doCompare(int nreplicas, struct repvol *repv, char **names, char *filepath, 
	 char *volmtpt, ViceFid *incfid, char *user, char *rights, char *owner, char *mode) {
  resreplica *dirs;
  struct  listhdr *k;
  int i;
  unsigned long j;
  FILE *file;
  struct volrep *rwv;
  int setmode = 0, setacl = 0, setowner = 0;

  switch (session) {
    case LOCAL_GLOBAL:
      printf("\"doCompare\" can only be used for server/server conflict\n");
      return -1;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return -1;
      break;
  }

  if (nreplicas == 1) {
    printf("DoCompare: Since there is only one replica, nothing to compare\n");
    return(0);
  }
  compOutputFile[0] = '\0';
  if (!strcmp(filepath, "stdout"))
    file = stdout;
  else {
    file = fopen(filepath, "w");
    if (!file) {
      printf("Couldn't open file %s for writing\n", filepath);
      return(-2);
    }
    if (strcmp(filepath, "/dev/null"))
      sprintf(compOutputFile, "%s", filepath);
  }
  /* set the global RepVolume to the volume we are repairing */
  RepVolume = repv->vid;
    
  if (getunixdirreps(nreplicas, names, &dirs)) {
    printf("compare failed because of failure in getting needed replica information\n");
    return -1;
  }
  int rc = dirresolve(nreplicas, dirs, NULL, &k, volmtpt);

  if (compareAcl(nreplicas, dirs)){
    nConflicts ++;
    if ( user && rights ) {
      printf("Acls will be set to %s %s.\n", user, rights);
      setacl = 1;
    } 
    else printf("Acls differ: Please repair manually using setacl <user> <rights>\n");
  }
  if (compareStatus(nreplicas, dirs)){
    nConflicts++;
    if ( mode ) {
      printf("Modebits will be set to %s.\n", mode);
      setmode = 1;
    } 
    else printf("Modebits differ - a repair should set the bits\n");
  }
  if (compareOwner(nreplicas, dirs)) {
    nConflicts++;
    if ( owner ) {
      printf("owner uid will be set to %s\n", owner);
      setowner = 1;
    } 
    else printf("Owner differs: Please repair manually using setowner <uid>\n");
  }

  for (i = 0; i < nreplicas; i++){
    /* find the server name */
    {
      rwv = repv->rwhead;
      while (rwv && k[i].replicaId != rwv->vid)
	rwv = rwv->next;
      CODA_ASSERT(k[i].replicaId == rwv->vid);
    }
    fprintf(file,"\nreplica %s %lx \n", rwv->srvname, k[i].replicaId);
    for (j = 0; j < k[i].repairCount; j++)
      repair_printline(&(k[i].repairList[j]), file);
    if ( setacl )
      fprintf(file, "\tsetacl %s %s\n", user, rights);
    if ( setmode )
      fprintf(file, "\tsetmode %s\n", mode);
    if ( setowner )
      fprintf(file, "\tsetowner %s\n", owner);
  }

  if (file != stdout) {
    fclose(file);
    if (compOutputFile[0]) {
      // save the information about the compare file produced
      if (stat(compOutputFile, &compOutputStatBuf)) {
	printf("Stat of %s failed\n", compOutputFile);
	compOutputFile[0] = '\0';
      }
    }
  }

  if (compareVV(nreplicas, names, repv)) {
    if (!nConflicts) {
      // warn the user if no conflicts were detected otherwise
      printf("The fix file may be empty but .... \n"
	     "You still need a dorepair because the "
	     "Version state is different\n");
    }
    nConflicts++;
  }
  if ((incfid->Vnode == 1) && (incfid->Unique == 1) &&
      (compareQuotas(nreplicas, names))) {
    nConflicts++;
    printf("Your volume quota is different on the various replicas\n");
    printf("You SHOULD talk to one of the system administrators soon\n");
  }
  /* clean up the malloced memory */
  resClean(nreplicas, dirs, k);
  if (rc == NNCONFLICTS) return(NNCONFLICTS);
  return(nConflicts);
}

int compareStatus(int nreplicas, resreplica *dirs) {
  int i;
  for (i = 1; i < nreplicas; i++) {
    if (dirs[i].modebits != dirs[0].modebits)
      return -1;
  }
  return 0;
}

int compareOwner(int nreplicas, resreplica *dirs) {
  int i;
  for (i = 1; i < nreplicas; i++) {
    if (dirs[i].owner != dirs[0].owner)
      return -1;
  }
  return 0;
}

int compareQuotas(int nreplicas, char **names) {
  if (nreplicas <= 1) {
    printf("Comparing Quotas: Not enough replicas to compare\n");
    return 0;
  }
  char piobuf[DEF_BUF];    
  struct ViceIoctl vio;
  vio.in = 0;
  vio.in_size = 0;
  vio.out = piobuf;
  vio.out_size = sizeof(piobuf);

  /* Do the pioctl */
  int rc = pioctl(names[0], VIOCGETVOLSTAT, &vio, 1);
  if (rc <0) {fflush(stdout); perror(names[0]); return(1);}
  /* Get pointers to output fields */
  VolumeStatus *vs = (VolumeStatus *)piobuf;
  int minquota0 = (int) vs->MinQuota;
  int maxquota0 = (int) vs->MaxQuota;
  for (int i = 1; i < nreplicas; i++) {
    vio.in = 0;
    vio.in_size = 0;
    vio.out_size = DEF_BUF;
    vio.out = piobuf;

    /* Do the pioctl */
    rc = pioctl(names[i], VIOCGETVOLSTAT, &vio, 1);
    if (rc <0) {fflush(stdout); perror(names[i]); return(1);}
    /* Get pointers to output fields */
    vs = (VolumeStatus *)piobuf;
    if ((vs->MinQuota !=  minquota0) || (vs->MaxQuota != maxquota0)) 
      return 1;
  }
  return 0;
}

void printAcl(struct Acl *acl) {
  int i;
    
  printf("There are %d plus entries\n", acl->nplus);
  for (i = 0; i < acl->nplus; i++)
    printf("%s \t %ld\n", ((acl->pluslist)[i]).name, ((acl->pluslist)[i]).rights);
  printf("There are %d negative entries\n", acl->nminus);
  for (i = 0; i < acl->nminus; i++)
    printf("%s \t %ld\n", ((acl->minuslist)[i]).name, ((acl->minuslist)[i]).rights);
  printf("End of Access List\n");
}

int compareAcl(int nreplicas, resreplica *dirs) {
  int i, j;
  struct Acl *al0, *ali;
  al0 = dirs[0].al;

  for (i = 1; i < nreplicas; i++){
    ali = dirs[i].al;

    if (ali->nplus != al0->nplus || ali->nminus != al0->nminus)
      return -1;
    for (j = 0; j < al0->nplus; j++){
      if (strcmp((al0->pluslist)[j].name, (ali->pluslist)[j].name))
	return -1;
      if ((al0->pluslist)[j].rights != (ali->pluslist)[j].rights)
	return -1;
    }
    for (j = 0; j < al0->nminus; j++){
      if (strcmp((al0->minuslist)[j].name, (ali->minuslist)[j].name))
	return -1;
      if ((al0->minuslist)[j].rights != (ali->minuslist)[j].rights)
	return -1;
    }
  }
  return 0;
}

int compareVV(int nreplicas, char **names, struct repvol *repv) {
  vv_t vv[MAXHOSTS];
  vv_t *vvp[MAXHOSTS];
  ViceFid fid;
  int nhosts = 0;
  int i;
  int HowMany = 0;

  for (i = 0; i < MAXHOSTS; i++) 
    vvp[i] = NULL;

  for (i = 0; i < nreplicas; i++) {
    if (repair_getfid(names[i], &fid, &vv[nhosts]))
      printf("Couldn't get vv for %s\n", names[i]);
    else
      nhosts++;
  }
  for (i = 0; i < nhosts; i++)
    vvp[i] = &vv[i];
  if (VV_Check_IgnoreInc(&HowMany, vvp, 1) != 1)
    return(1);
  return(0);
}

void getremovelists(int nreplicas, resreplica *dirs, struct listhdr **repairlist) {
  struct repair rep;
  resdir_entry *rde;
  int i, j;

  InitListHdr(nreplicas, dirs, repairlist);
  /* for each replica create the list of entries to be removed */
  for ( i = 0 ; i < nreplicas; i++) {
    for ( j = 0; j < dirs[i].nentries; j++) {
      rde = &(direntriesarr[dirs[i].entry1 + j]);
      if (ISDIR(rde->vno)) 
	rep.opcode = REPAIR_REMOVED;
      else 
	rep.opcode = REPAIR_REMOVEFSL;
      strcpy(&(rep.name[0]), &(rde->name[0]));
      rep.parms[0] = 0;
      rep.parms[1] = 0;
      rep.parms[2] = 0;
      InsertListHdr(&rep, repairlist, i);
    }
  }
}

/* extfile: external (ASCII) rep
   intfile: internal (binary) rep
   Returns 0 on success, -1 on failures
*/
int makedff(char *extfile, char *intfile /* OUT */) {
  struct listhdr *hl;
  int hlc, rc;

  /* parse input file and obtain internal rep  */
  rc = repair_parsefile(extfile, &hlc, &hl);
  if (rc < 0) return(-1);

  /* generate temp file name */
  strcpy(intfile, "/tmp/REPAIR.XXXXXX");
  mktemp(intfile);

  /* write out internal rep */
  rc = repair_putdfile(intfile, hlc, hl);
  if (rc) return (-1);
  // repair_printfile(intfile);
  
  return(0);
}

void GetArgs(int argc, char *argv[]) {
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      repair_DebugFlag = 1;
      continue;
    }
    if (strcmp(argv[i], "-allowclear") == 0) {
      allowclear = 1;
      continue;
    }
    goto BadArgs;
  }
  return;

BadArgs:
  printf("Usage: repair [-d]\n");
  exit(-1);
}


void SetDefaultPaths() {
  char buf[MAXPATHLEN];
  char *repairrc = getenv("REPAIRRC");
  char *home;
  FILE *reprc;
  int ec;
  char arg1, arg2[MAXPATHLEN];  

  beginRepDefault[0] = '\0';
  doRepDefault[0] = '\0';

  if (repairrc == NULL){
    home = getenv("HOME");
    CODA_ASSERT(home != NULL);
    strcpy(buf, home);
    strcat(buf, "/.repairrc");
    repairrc = buf;
  } 
  reprc = fopen(repairrc, "r");
  if (reprc){
    while((ec = fscanf(reprc, "%c\t%s\n", &arg1, arg2)) != EOF){
      if (ec != 2){
	printf("Error in file %s \n", repairrc);
	exit(-1);
      }
      switch(arg1) {
        case 'b':
	  strcpy(beginRepDefault, arg2);
	  break;
        case 'c':
	  strcpy(compDirDefault, arg2);
	  break;
        case 'd':
	  strcpy(doRepDefault, arg2);
	  break;
        default:
	  printf("Unknown option %c in %s file\n", arg1, repairrc);
	  exit(-1);
      }
    }
    fclose(reprc);
  }
}

int GetReplicaNames(char **names, int maxnames, char *ReplicatedName) {
  struct dirent *de;
  struct stat buf;
  int i;
  DIR *d = opendir(ReplicatedName);
  if (!d) {
    printf("GetReplicaNames: couldn't open %s\n", ReplicatedName);
    return(-1);
  }
  int SizeRepName = (int) (strlen(ReplicatedName) + 1);
  i = 0; 
  de = readdir(d);
  for (; (i < maxnames) && (de != NULL); de = readdir(d)) {
    if ((!strcmp(de->d_name, ".")) || 
	(!strcmp(de->d_name, "..")))
      continue;

    /* XXXX -- 5 is for safety */
    names[i] = (char *)malloc(strlen(de->d_name) + SizeRepName + 5); 
    CODA_ASSERT(names[i]);
    sprintf(names[i], "%s/%s" , ReplicatedName, de->d_name);
    if (stat(names[i], &buf)) {
      perror(names[i]);
      printf("replica %s not accessible, NOT USING FOR COMPARE\n", names[i]);
      free(names[i]);
      names[i] = 0;
      continue;
    }
    else { /* append a "/" to the name only if it is a directory */
      if ((buf.st_mode & S_IFMT) == S_IFDIR) 
	strcat(names[i], "/");
      i++;
    }
  }
  closedir(d);
  return (i);
}

/* return zero if user has valid tokens */
int GetTokens() {
  ClearToken clear;
  EncryptedSecretToken secret;
  return (U_GetLocalTokens(&clear, secret));
}

void rep_BeginRepair(int largc, char **largv) {
  char userpath[MAXPATHLEN];
  struct repvol *repv;
  int rc;

  switch (session) {
    case SERVER_SERVER:
      printf("Server-server repair session already in progress.\n");
      return;
      break;
    case LOCAL_GLOBAL:
      printf("Local-global repair session already in progress.\n");
      return;
      break;
  }
  if (largc == 1)
    Parser_getstr("Pathname of object in conflict?", beginRepDefault, userpath, MAXPATHLEN);
  else if (largc == 2)
    strncpy(userpath, largv[1], MAXPATHLEN);
  else {
    printf("beginrepair <reppathname>\n");
    return;
  }
  strcpy(beginRepDefault, userpath);
  strcpy(doRepDefault, userpath);

  if ((rc = BeginRepair(userpath, &repv)) < 0) {
    printf("beginrepair failed.\n");
    fflush(stdout);
    return;
  }
  
  switch (rc) {
    case 0: 
      session = LOCAL_GLOBAL;
      repv->local = 1;
      printf("Local-global repair session started.\n");
      printf("Available Commands:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
	     "checklocal", "listlocal", "preservelocal", "preservealllocal", "discardlocal",
	     "discardalllocal", "setglobalview", "setmixedview", "setlocalview");
      printf("A list of local mutations is available in the .cml file in the coda spool directory\n");
      break;
    case 1: 
      session = LOCAL_GLOBAL;
      repv->local = 0;
      printf("Local-global repair session already in progress!\n");
      EndRepair(repv, session, 0);
      break;
    case 2:
      session = SERVER_SERVER;
      repv->local = 0;
      printf("Server-server repair session started.\n");
      printf("Available commands:\n\t%s\n\t%s\n\t%s\n", 
	     "comparedirs", "removeinc", "dorepair");
      break;
    default:
      printf("Bogus return code from venus (%d)\n", rc);
      exit(1);
      break;
  }
  fflush(stdout);
}

int BeginRepair(char *userpath, struct repvol **repv) {
  VolumeId vid;
  VolumeId *tv;
  struct ViceIoctl vioc;
  char space[DEF_BUF], cmd[32];
  char reppath[MAXPATHLEN], prefix[MAXPATHLEN], suffix[MAXPATHLEN];
  int rc;

  if ((rc = repair_isleftmost(userpath, reppath, MAXPATHLEN)) < 0)
    return(rc);
  if ((rc = repair_getmnt(reppath, prefix, suffix, &vid)) < 0)
    return(rc);

  /* See if this volume is already being repaired */
  if ((*repv = repair_findrep(vid)) != NULL) {
    printf("Repair already in progess on volume %d.\n", vid);
    return(-1);
  }

  /* Create a new rep vol entry */
  if (repair_newrep(vid, prefix, repv) < 0) {
    printf("Could not allocate new repvol.\n");
    return(-1);
  }

  /* Obtain names of rw replicas */
  vioc.out_size = (short)sizeof(space);
  vioc.in_size = 0;
  vioc.out = space;
  memset(space, 0, (int)sizeof(space));
  rc = pioctl(reppath, VIOC_ENABLEREPAIR, &vioc, 0);
  if (rc < 0) {
    if (errno == EWOULDBLOCK)
      printf("Repair in progress on volume at \"%s\"\n", prefix);
    else 
      repair_perror(" ENABLEREPAIR", prefix, errno); /* other error */
    repair_finish(*repv);
    return(-1);
  }
  sprintf((*repv)->rodir, "%s", reppath);

  /* Mount the rw replicas - or just insert into list of replicas */
  tv = (VolumeId *)space; /* output of ENABLEREPAIR pioctl */
  if ((rc = repair_mountrw(*repv, tv, MAXHOSTS)) < 0) {
    repair_finish(*repv); 
    return(-1);
  }
  /* Link in new volume */
  if (repair_linkrep(*repv) < 0) {
    printf("Could not link in new repvol.\n");
    repair_finish(*repv); 
    return(-1);    
  }
    
  sprintf(cmd, "%d 1", REP_CMD_BEGIN);
  vioc.in = cmd;
  vioc.in_size = (short)(strlen(cmd) + 1);
  vioc.out = space;
  vioc.out_size = (short)sizeof(space);
  if ((rc = pioctl(reppath, VIOC_REP_CMD, &vioc, 0)) < 0) {
    perror("VIOC_REP_CMD(REP_CMD_BEGIN)");
    if (repair_unlinkrep(*repv) < 0) {
      printf("Could not unlink repvol.\n");
      exit(1);
    }
    repair_finish(*repv); 
    return(-1);    
  }

  sscanf(vioc.out, "%d", &rc);
  return(rc);
}


void rep_CheckLocal(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[DEF_BUF];

  switch (session) {
    case SERVER_SERVER:
      printf("\"checkLocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  vioc.in = buf;
  sprintf(buf, "%d", REP_CMD_CHECK);
  vioc.in_size = (short) strlen(vioc.in) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_CHECK)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_ListLocal(int largc, char **largv) {
  int fd;
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[DEF_BUF];
  char filename[MAXPATHLEN];

  switch (session) {
    case SERVER_SERVER:
      printf("\"listLocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  strcpy(filename, "/tmp/listlocal.XXXXXX");
  mktemp(filename);
  vioc.in = buf;
  sprintf(buf, "%d %s", REP_CMD_LIST, filename);
  vioc.in_size = (short) strlen(vioc.in) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_LIST)");
  printf("%s\n", vioc.out);
  fflush(stdout);
  if (rc == 0) {
    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) perror(filename);
    else {
      while (read(fd, buf, DEF_BUF) > 0)
	write(1, buf, strlen(buf));
      close(fd);
    }	
  }
  unlink(filename);
}

void rep_PreserveLocal(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  struct repvol *repv;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  switch (session) {
    case SERVER_SERVER:
      printf("\"preservelocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_PRESERVE);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMU_PRESERVE)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_PreserveAllLocal(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  switch (session) {
    case SERVER_SERVER:
      printf("\"preservealllocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  /* Release volume-level locks */
  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_PRESERVE_ALL);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMU_PRESERVE_ALL)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_DiscardLocal(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];
    
  switch (session) {
    case SERVER_SERVER:
      printf("\"discardlocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_DISCARD);
  vioc.in = buf;    
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_DISCARD)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_DiscardAllLocal(int largc, char **largv) {
  char space[DEF_BUF];

  switch (session) {
    case SERVER_SERVER:
      printf("\"discardalllocal\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }
  if (DiscardAllLocal(space) < 0)
    printf("%s\ndiscardalllocal failed\n", space);
  else
    printf("%s\n", space);
  fflush(stdout);
}

/* returns string result of pioctl in argument if non-null */
int DiscardAllLocal(char *ret) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  vioc.out = (ret == NULL) ? space : ret;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_DISCARD_ALL);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) {
    perror("VIOC_REP_CMD(REP_CMD_DISCARD_ALL)");
    return(-1);
  }
  return(0);
}

void rep_SetLocalView(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  switch (session) {
    case SERVER_SERVER:
      printf("\"setlocalview\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_LOCAL_VIEW);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_LOCAL_VIEW)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_SetGlobalView(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  switch (session) {
    case SERVER_SERVER:
      printf("\"setglobalview\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }
    
  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_GLOBAL_VIEW);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_GLOBAL_VIEW)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void rep_SetMixedView(int largc, char **largv) {
  struct ViceIoctl vioc;
  int rc;
  char space[DEF_BUF];
  char buf[BUFSIZ];

  switch (session) {
    case SERVER_SERVER:
      printf("\"setmixedview\" can only be used to repair a local/global conflict\n");
      return;
      break;
    case NOT_IN_SESSION:
      printf("You must do \"beginrepair\" first\n");
      return;
      break;
  }

  vioc.out = space;
  vioc.out_size = DEF_BUF;
  sprintf(buf, "%d", REP_CMD_MIXED_VIEW);
  vioc.in = buf;
  vioc.in_size = (short) strlen(buf) + 1;

  rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
  if (rc < 0) perror("VIOC_REP_CMD(REP_CMD_MIXED_VIEW)");
  printf("%s\n", vioc.out);
  fflush(stdout);
}

void INT(int, int, struct sigcontext *) {
  /* force an end to the current repair session when ^C is hit */
  printf("abnormal exit of repair tool\n");
  fflush(stdout);
  rep_Exit(0, NULL);
}
