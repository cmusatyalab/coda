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

char compDirDefault[MAXPATHLEN];
char compOutputFile[MAXPATHLEN]; /* filename for output of last docompare */
char beginRepDefault[MAXPATHLEN];
char doRepDefault[MAXPATHLEN];
struct stat compOutputStatBuf;	 /* file information for the repair commands file */
struct stat doInputStatBuf;

extern int IsCreatedEarlier(struct listhdr **, int, long, long);

void SetDefaultPaths();
int  compareVV(int, char **, struct repvol *);
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
int  dorep(struct repvol *repv, char *path);
int  getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize);
int  GetTokens();
void help(int argc, char **argv);

int findtype(struct repvol *);

/* Assumes pathname is the path of a conflict
 * Fills in repv with repvol created in beginning repair
 * Returns 0 on success, -1 on error and fills in msg if non-null */
int BeginRepair(char *pathname, struct repvol **repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], space[DEF_BUF], cmd[32];
    VolumeId vid;
    struct ViceIoctl vioc;
    int rc;

    /* Create a new rep vol entry */
    if (repair_newrep(pathname, repv, space, sizeof(space)) < 0) {
	strerr(msg, msgsize, "Could not allocate new repvol: %s", space);
	return(-1);
    }

    /* Obtain names of rw replicas */
    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    memset(space, 0, sizeof(space));
    rc = pioctl((*repv)->rodir, VIOC_ENABLEREPAIR, &vioc, 0);
    if (rc < 0) {
	if (errno == EWOULDBLOCK)
	    strerr(msg, msgsize, "Repair in progress on volume at \"%s\"", (*repv)->mnt);
	else 
	    strerr(msg, msgsize, "ENABLEREPAIR %s: %s", (*repv)->mnt, strerror(errno));
	repair_finish(*repv);
	return(-1);
    }

    /* Mount the rw replicas, i.e. just insert into list in repv */
    if ((rc = repair_mountrw(*repv, (VolumeId *)space, MAXHOSTS, msgbuf, sizeof(msgbuf))) < 0) {
	strerr(msg, msgsize, "Could not allocate replica list");    
	repair_finish(*repv);
	return(-1);
    }

    /* Begin the repair */
    sprintf(cmd, "%d 1", REP_CMD_BEGIN);
    vioc.in = cmd;
    vioc.in_size = (short)(strlen(cmd) + 1);
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    if ((rc = pioctl((*repv)->rodir, VIOC_REP_CMD, &vioc, 0)) < 0) {
	strerr(msg, msgsize, "REP_CMD_BEGIN failed: %s", strerror(errno));
	repair_finish(*repv);
	return(-1);
    }

    /* Determine conflict type */
    sscanf(vioc.out, "%d", &rc);
    if (rc == 0)
	(*repv)->local = 1; /* LOCAL_GLOBAL */
    else if (rc == 1) {
	(*repv)->local = 1;
	if (EndRepair(*repv, 0, msgbuf, sizeof(msgbuf)) < 0)
	    strerr(msg, msgsize, "Local-global repair session already in progress, %s", msgbuf);
	else strerr(msg, msgsize, "Local-global repair session already in progress");
	return(-1);
    }
    else if (rc == 2)
	(*repv)->local = 0; /* SERVER_SERVER */
    else { /* (rc < 0) || (rc > 2) */
	strerr(msg, msgsize, "Bogus return code from venus (%d)", rc);
	repair_finish(*repv);
	return(-1);
    }
    if (findtype(*repv) < 0) {
	if (EndRepair(*repv, 0, msgbuf, sizeof(msgbuf)) < 0)
	    strerr(msg, msgsize, "Could not determine conflict type, %s", msgbuf);
	else strerr(msg, msgsize, "Could not determine conflict type");
	return(-1);
    }

    return(0);
}

int ClearInc(struct repvol *repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF];
    ViceFid confFid, Fid[MAXHOSTS];
    vv_t vv[MAXHOSTS];
    struct ViceIoctl vioc;
    int rc, i, nreplicas;
    char **names, *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL;

    /* initialize the array of ropaths */
    if (repv == NULL) {
	strerr(msg, msgsize, "NULL repv");
	return(-1);
    }

    if ((nreplicas = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s\n", msgbuf);
	return(-1);
    }

    /* set the fid to 0.0 so docompare will not check quotas */
    confFid.Volume = repv->vid;
    confFid.Vnode = confFid.Unique = 0;
    /* do the compare */
    if (!doCompare(nreplicas, repv, names, "/dev/null", 
		   repv->mnt, &confFid, user, rights, owner, mode)) {
	/* XXXX if a get fid is done between two setvv's resolve might get called 
	   - therefore get the vv for each replica  before doing the setvv */
	for (i = 0; i < nreplicas; i++) {
	    if ((rc = repair_getfid(names[i], &Fid[i], &vv[i])) < 0) {
		strerr(msg, msgsize, "Error in repair_getfid(%s)", names[i]);
		goto CLEANUP;
	    }
	}

	if ((Fid[0].Vnode == 1) && (Fid[0].Unique == 1) 
	    && compareQuotas(nreplicas, names)) {
	    strerr(msg, msgsize, "Volume Quotas are different");
	    goto CLEANUP;
	}

	for (i = 0; i < nreplicas; i++) {
	    ClearIncon(vv[i]);
	    vioc.in = (char *)&vv[i];
	    vioc.in_size = sizeof(vv_t);
	    vioc.out = NULL;
	    vioc.out_size = 0;
	    if (rc = pioctl(names[i], VIOC_SETVV, &vioc, 0)) {
		strerr(msg, msgsize, "SETVV %s: %s", names[i], strerror(errno));
		goto CLEANUP;
	    }
	}

	for (i = 0; i < nreplicas; i++) 
	    freeif(names[i]);
	free(names);
	return(0);
    }
    else strerr(msg, msgsize, "Replicas not identical, can't clear inconsistency");

 CLEANUP:
    for (i = 0; i < nreplicas; i++) 
	freeif(names[i]);
    free(names);
    return(-1);
}

/* Discards all local mutations to volume under repair
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int DiscardAllLocal(struct repvol *repv, char *msg, int msgsize) {
    char space[DEF_BUF];
    char buf[BUFSIZ];
    struct ViceIoctl vioc;
    int rc;

    if ((repv == NULL) || !(repv->local)) { 
	strerr(msg, msgsize, "Invalid repv");
	return(-1);
    }

    sprintf(buf, "%d", REP_CMD_DISCARD_ALL);
    vioc.in = buf;
    vioc.in_size = (short)strlen(buf) + 1;
    vioc.out = space;
    vioc.out_size = sizeof(space);
    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) strerr(msg, msgsize, "%s", space);
    return(rc);
}

/* Assumes rw replicas are all of the same type
 *    (must be true if this is a leftmost conflict)
 * Fills in dirconf field in repv
 * Returns 0 on success, -1 on failure */
int findtype(struct repvol *repv) {
    char tmppath[MAXPATHLEN];
    struct stat sbuf;
    struct volrep *tmp;

    tmp = repv->rwhead;
    if (tmp == NULL) return(-1); /* need at least 1 replica */

    /* do the first one manually */
    sprintf(tmppath, "%s/%s", repv->rodir, tmp->compname);
    if (lstat(tmppath, &sbuf) < 0) return(-1);
    repv->dirconf = ((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
    tmp = tmp->next;

    while (tmp != NULL) {
	sprintf(tmppath, "%s/%s", repv->rodir, tmp->compname);
	if (lstat(tmppath, &sbuf) < 0) return(-1);
	if (repv->dirconf != (((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0))
	    return(-1);
	tmp = tmp->next;
    }
    return(0);
}

/* Ends the repair session on repv and frees up memory associated with repv
 * Only commits changes in for a local/global conflict if commit == 1
 * Returns 0 on success, -1 on error and fills in msg if non-NULL 
                          (but still frees memory) */
int EndRepair(struct repvol *repv, int commit, char *msg, int msgsize) {
    char space[DEF_BUF], cmd[32];
    struct ViceIoctl vioc;
    int rc;

    if (repv->local) { /* local/global conflict */
	sprintf(cmd, "%d %d", REP_CMD_END, commit);
	vioc.in = cmd;
	vioc.in_size = (short)(strlen(cmd) + 1);
	vioc.out = space;
	vioc.out_size = sizeof(space);
	if ((rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0)) < 0)
	    strerr(msg, msgsize, "REP_CMD_END failed: %s", strerror(errno));
	else if (strcmp(vioc.out, "repair session completed") != 0) {
	    strerr(msg, msgsize, "%s", vioc.out);
	    rc = -1;
	}
    }
    else { /* server/server conflict */
	vioc.in = NULL;
	vioc.in_size = 0;
	vioc.out = NULL;
	vioc.out_size = 0;
	errno = 0;
	if ((rc = pioctl(repv->mnt, VIOC_DISABLEREPAIR, &vioc, 0)) < 0)
	    strerr(msg, msgsize, "DISABLEREPAIR %s: %s", repv->mnt, strerror(errno));
    }
    repair_finish(repv);
    return(rc);
}

/* Removes inconsistencies one volume under repair
 *   (first does a repair, then clears the inc)
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int RemoveInc(struct repvol *repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], tmppath[MAXPATHLEN];
    struct stat buf;
    ViceFid fixfid, confFid;
    ViceVersionVector confvv;
    vv_t fixvv;
    char *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL, **names;
    int nreplicas, rc, i, j;
    struct volrep  *rwv;
    struct listhdr *repairlist;
    resreplica *dirs;

    user = rights = owner = mode = NULL;

    if ((nreplicas = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }

    if (repv->dirconf) { /* directory conflict */
	/* get the directory entries and create list of children to be removed */
	if (getunixdirreps(nreplicas, names, &dirs)) {
	    strerr(msg, msgsize, "Could not get needed replica information");
	    rc = -1;
	    goto Error;	    
	}
	getremovelists(nreplicas, dirs, &repairlist);

	/* convert list to internal format */
	strcpy(tmppath, "/tmp/REPAIR.XXXXXX");
	mktemp(tmppath);

	/* write out internal rep */
	rc = repair_putdfile(tmppath, nreplicas, repairlist);
	if (rc) {
	    strerr(msg, msgsize, "Coudn't put repair list into file %s", tmppath);
	    goto Error;
	}

	rc = dorep(repv, tmppath); /* do the repair */
	if (rc < 0 && errno != ETOOMANYREFS) {
	    strerr(msg, msgsize, "REPAIR %s: %s", repv->rodir, strerror(errno));
	    unlink(tmppath); /* Clean up */
	    goto Error;
	}    
	unlink(tmppath); /* Clean up */

	/* clear the inconsistency if needed and possible */ 
	if ((!repair_getfid(repv->rodir, &confFid, &confvv))
	    && (confvv.StoreId.Host == -1)
	    && (confvv.StoreId.Uniquifier == -1)
	    /* object is still inconsistent -- try to clear it */
	    && ((rc = ClearInc(repv, msgbuf, sizeof(msgbuf))) < 0))
	    strerr(msg, msgsize, "Error clearing inconsistency: %s", msgbuf);
    }
    else { /* file conflict */
	if (rc = repair_getfid(names[0], &fixfid, &fixvv)) {
	    strerr(msg, msgsize, "Could not get fid for %s", names[0]);
	    goto Error;
	}

	sprintf(tmppath, "@%lx.%lx.%lx", fixfid.Volume, fixfid.Vnode, fixfid.Unique);

	rc = dorep(repv, tmppath); /* do the repair */
	if ((rc < 0) && (errno != ETOOMANYREFS))
	    strerr(msg, msgsize, "REPAIR %s: %s", repv->rodir, strerror(errno));
    }

 Error:
    /* clean up malloced memory */
    if (repv->dirconf) /* directory conflict */
	resClean(nreplicas, dirs, repairlist);
    for (i = 0; i < nreplicas; i++)
	freeif(names[i]);
    free(names);

    return(rc);
}

/* args: <reppathname> <fixfilename> */
void rep_DoRepair(int largc, char **largv) {
    enum {FIXDIR, FIXFILE} fixtype;
    VolumeId vid;
    struct repvol *repv;
    struct volrep *rwv;
    struct repvol *saverepv;
    struct stat statbuf;
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
    /*
      if ((repv = repair_findrep(vid)) == NULL) {
      printf("You must do \"beginrepair\" first\n"); 
      return;
      }
    */
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
    if (fixtype == FIXDIR) rc = dorep(repv, tmppath);
    else rc = dorep(repv, ufixpath);
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
    VolumeId vid;
    struct repvol *repv;
    char reppath[MAXPATHLEN], filepath[MAXPATHLEN];
    char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    char tmppath[MAXPATHLEN], msgbuf[DEF_BUF];
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
    /*
      if ((repv = repair_findrep(vid)) == NULL) {
      printf("You must do \"beginrepair\" first\n"); 
      return;
      }
    */
    
    CODA_ASSERT(repv->rwhead);   /* better have atleast one rw replica */

    if (repair_getfid(uconflictpath, &confFid, &confvv)) {
	printf("%s isn't in /coda... Are you sure this object is in conflict\n", uconflictpath);
	return;
    }
    if (!ISDIR(confFid.Vnode) || !(repv->dirconf)) {
	printf("Compare can only be performed on directory replicas\n");
	return;
    }
    /* initialize the array of ropaths */
    if ((nreplicas = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	printf("Error getting replica names: %s", msgbuf);
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
	    rc = dorep(repv, tmppath);
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
    vio.in = NULL;
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
	vio.in = NULL;
	vio.in_size = 0;
	vio.out = piobuf;
	vio.out_size = sizeof(piobuf);

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

int dorep(struct repvol *repv, char *path) {
    char space[DEF_BUF];
    struct ViceIoctl vioc;
    int rc;

    vioc.in = path;
    vioc.in_size = strlen(path) + 1;
    vioc.out = space;
    vioc.out_size = sizeof(space);
    memset(space, 0, sizeof(space));
    rc = pioctl(repv->rodir, VIOC_REPAIR, &vioc, 0);
    return(rc);
}

int getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize) {
    struct stat buf;
    DIR *d;
    struct dirent *de;
    struct volrep *rwv;
    int i, nreps, len;

    /* count and allocate replicas */
    for (nreps = 0, rwv = repv->rwhead ; rwv != NULL; rwv = rwv->next, nreps++);
    if (nreps == 0) {
	strerr(msg, msgsize, "No accessible replicas");
	return(-1);
    }
    if ((*names = (char **)malloc(nreps * sizeof(char *))) == NULL) {
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }
    for (i = 0; i < nreps; (*names)[i++] = NULL); /* initialize all to NULL */

    if ((d = opendir(repv->rodir)) == NULL) {
	strerr(msg, msgsize, "Could not open directory %s\n", repv->rodir);
	free(*names);
	return(-1);
    }

    /* space for path prefix, slash, and trailing null, and last slash (if directory) */
    len = strlen(repv->rodir) + 3;

    /* search the directory and get the names */
    for (i = 0, de = readdir(d); (i < nreps) && (de != NULL); de = readdir(d)) {
	if ((!strcmp(de->d_name, ".")) || (!strcmp(de->d_name, "..")))
	    continue;
	/* allocate new name */
	(*names)[i] = (char *)malloc((strlen(de->d_name) + len) * sizeof(char));
	if ((*names)[i] == NULL) {
	    for (i = 0; i < nreps; nreps++) freeif((*names)[i]);
	    free(*names);
	    strerr(msg, msgsize, "Malloc failed");
	    return(-1);      
	}

	if (repv->dirconf) sprintf((*names)[i], "%s/%s/", repv->rodir, de->d_name);
	else sprintf((*names)[i], "%s/%s" , repv->rodir, de->d_name);

	i++;
    }

    if (closedir(d) < 0) {
	for (i = 0; i < nreps; nreps++) freeif((*names)[i]);
	free(*names);
	strerr(msg, msgsize, "closedir failed: %s", strerror(errno));
	return(-1);      
    }

    return (nreps);
}

/* return zero if user has valid tokens */
int GetTokens() {
    ClearToken clear;
    EncryptedSecretToken secret;
    return (U_GetLocalTokens(&clear, secret));
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

    sprintf(buf, "%d", REP_CMD_CHECK);
    vioc.in = buf;
    vioc.in_size = strlen(vioc.in) + 1;
    vioc.out = space;
    vioc.out_size = DEF_BUF;

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
