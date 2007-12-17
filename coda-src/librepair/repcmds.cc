
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

*/

#include <codaconf.h>
#include "repcmds.h"

int  compareAcl(int, resreplica *);
int  compareOwner(int, resreplica *);
int  compareQuotas(int , char **);
int  compareStatus(int, resreplica *);
int  compareVV(int, char **, struct conflict *);
int  compareFids(ViceFid *a, ViceFid *b);

int  findtype(struct conflict *);
void getremovelists(int, resreplica *, struct listhdr **);
int  getVolrepNames(struct conflict *conf, char ***names, char *msg, int msgsize);
int  isLocal(resreplica *dir);
void printAcl(struct Acl *);

/* Assumes pathname is the path of a conflict
 * Fills in conf with conflict struct created in beginning repair
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int BeginRepair(char *pathname, struct conflict **conf, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF], space[DEF_BUF], cmd[32];
    struct ViceIoctl vioc;
    int rc;

    /* Create a new conflict struct */
    if (repair_newrep(pathname, conf, msgbuf, sizeof(msgbuf)) < 0) {
      strerr(msg, msgsize, "Error beginning repair: %s", msgbuf);
      return(-1);
    }

    if (repair_getfid((*conf)->rodir, &((*conf)->fid), (*conf)->realm,
		      &((*conf)->VV), msgbuf, sizeof(msgbuf))) {
      strerr(msg, msgsize, "Error beginning repair: %s", msgbuf);
      return(-1);
    }

    /* Perform a replica expansion */
    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    memset(space, 0, sizeof(space));
    rc = pioctl((*conf)->rodir, _VICEIOCTL(_VIOC_ENABLEREPAIR), &vioc, 0);
    if (rc < 0) {
        strerr(msg, msgsize, "Error beginning repair: %s", strerror(errno));
	repair_finish(*conf);
	return(-1);
    }

    /* Get the replicas, i.e. just insert into list in conf */
    if ((rc = repair_mountrw(*conf, msgbuf, sizeof(msgbuf))) < 0) {
	strerr(msg, msgsize, "Could not create replica list: %s", msgbuf);
	repair_finish(*conf);
	return(-1);
    }

    /* Begin the repair */
    sprintf(cmd, "%d 1", REP_CMD_BEGIN);
    vioc.in = cmd;
    vioc.in_size = (short)(strlen(cmd) + 1);
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    if ((rc = pioctl((*conf)->rodir, _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0)) < 0) {
        strerr(msg, msgsize, "Error beginning repair: %s", strerror(errno));
	repair_finish(*conf);
	return(-1);
    }

    /* Determine conflict method type */
    sscanf(vioc.out, "%d", &rc);
    if (rc == 0) {
	if (EndRepair(*conf, 0, msgbuf, sizeof(msgbuf)) < 0)
	  strerr(msg, msgsize, "No conflict\nEndRepair failed: %s", msgbuf);
	else
	  strerr(msg, msgsize, "No conflict detected.");
	return(-1);
    }
    else if (rc == 1)
	(*conf)->local = LOCAL_GLOBAL;
    else if (rc == 2)
	(*conf)->local = SERVER_SERVER;
    else if (rc == 3)
	(*conf)->local = MIXED_CONFLICT;
    else { /* (rc < 0) || (rc > 3) */
	strerr(msg, msgsize, "Bogus return code from venus (%d)", rc);
	repair_finish(*conf);
	return(-1);
    }
    if (findtype(*conf) < 0) { /* decides: directory or file? */
      if (EndRepair(*conf, 0, msgbuf, sizeof(msgbuf)) < 0)
	strerr(msg, msgsize, "Could not determine conflict type, %s", msgbuf);
      else
	strerr(msg, msgsize, "Could not determine conflict type");
      return(-1);
    }

    return(0);
}

/* Clears inconsistencies on repv
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int ClearInc(struct conflict *conf, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF];
    ViceFid confFid, Fid[MAXHOSTS];
    vv_t vv[MAXHOSTS];
    struct ViceIoctl vioc;
    int rc, i, nreplicas;
    struct repinfo inf;
    char **names;

    memset(&inf, 0, sizeof(inf));

    if (conf == NULL) {
	strerr(msg, msgsize, "NULL conflict");
	return(-1);
    }

    /* get replica names for doCompare */
    if ((nreplicas = getVolrepNames(conf, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s\n", msgbuf);
	return(-1);
    }

    /* set the fid to 0.0 so docompare will not check quotas */
    confFid.Volume = conf->fid.Volume;
    confFid.Vnode = confFid.Unique = 0;
    /* do the compare */
    if (!CompareDirs(conf, "/dev/null", &inf, msgbuf, sizeof(msgbuf))) {

      /* XXXX if a get fid is done between two setvv's resolve might get called
	 - therefore get the vv for each replica before doing the setvv */

	for (i = 0; i < nreplicas; i++) {
	    if ((rc = repair_getfid(names[i], &Fid[i], NULL, &vv[i],
				    msgbuf, sizeof(msgbuf))) < 0) {
		strerr(msg, msgsize, "repair_getfid(%s): %s",
		       names[i], msgbuf);
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
	    rc = pioctl(names[i], _VICEIOCTL(_VIOC_SETVV), &vioc, 0);
	    if (rc) {
		strerr(msg, msgsize, "SETVV %s: %s",
		       names[i], strerror(errno));
		goto CLEANUP;
	    }
	}

	for (i = 0; i < nreplicas; i++) 
	    freeif(names[i]);
	free(names);
	return(0);
    }
    else
      strerr(msg, msgsize, "Replicas not identical, can't clear inconsistency: %s", msgbuf);

 CLEANUP:
    if(names) {
      for (i = 0; i < nreplicas; i++)
	freeif(names[i]);
      free(names);
    }
    return(-1);
}

/* Does the compare on conf, and writes to fixfile
 * Returns number of conflicts on success, -1 on error and fills in msg if non-NULL
 * Returns -2 if there are name/name conflicts (in which case the caller
 * should DoRepair to fix them and then do CompareDirs again) */
int CompareDirs(struct conflict *conf, const char *fixfile, struct repinfo *inf,
		char *msg, int msgsize)
{
    char msgbuf[DEF_BUF];
    char **names;
    char realm[MAXHOSTNAMELEN];
    int ret, i, nreps, nConflicts = 0, setmode = 0, setacl = 0, setowner = 0;
    unsigned long j;
    resreplica *dirs = NULL;
    struct replica *rwv;
    struct  listhdr *k = NULL;
    FILE *file;

    if ((conf == NULL) || (inf == NULL)) {
      strerr(msg, msgsize, "NULL %s", (conf ? "repinfo" : "conf"));
      return(-1);
    }

    if (!(conf->dirconf)) {
      strerr(msg, msgsize, "\nCompare can only be performed on directory replicas!\ndirconf=%d", conf->dirconf);
      return(-1);
    }

    if ((nreps = getVolrepNames(conf, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }
 
    /* Open the fixfile */
    if (!strcmp(fixfile, "stdout")) file = stdout;
    else if (!strcmp(fixfile, "stderr")) file = stderr;
    else {
	if ((file = fopen(fixfile, "w")) == NULL) {
	    strerr(msg, msgsize, "Could not open file %s for writing: %s",
		   fixfile, strerror(errno));
	    goto Cleanup;
	}
    }

    if (getunixdirreps(nreps, names, &dirs)) {
	strerr(msg, msgsize, "Could not get replica information");
	goto Cleanup;
    }

    /* Do the resolve! */
    ret = dirresolve(nreps, dirs, NULL, &k, conf->fid.Volume, inf, realm);

    if (compareAcl(nreps, dirs)){
	nConflicts++;
	if ( inf->user && inf->rights ) {
	    printf("Acls will be set to %s %s.\n", inf->user, inf->rights);
	    setacl = 1;
	} 
	else printf("Acls differ: Please repair manually using setacl <user> <rights>\n");
    }

    if (compareOwner(nreps, dirs)) {
	nConflicts++;
	if ( inf->owner ) {
	    printf("owner uid will be set to %s\n", inf->owner);
	    setowner = 1;
	} 
	else printf("Owner differs: Please repair manually using setowner <uid>\n");
    }

    if (compareStatus(nreps, dirs)){
	nConflicts++;
	if ( inf->mode ) {
	    printf("Modebits will be set to %s.\n", inf->mode);
	    setmode = 1;
	} 
	else printf("Modebits differ - a repair should set the bits\n");
    }

    for (i = 0; i < nreps; i++) {
	/* find the server name */
	{
	    rwv = conf->head;
	    while (rwv && compareFids(&k[i].replicaFid, &rwv->fid))
	      rwv = rwv->next;
	    CODA_ASSERT(!compareFids(&k[i].replicaFid, &rwv->fid));
	}
	fprintf(file,"\nreplica %s %08x \n", rwv->srvname, k[i].replicaFid.Volume);
	for (j = 0; j < k[i].repairCount; j++) {
	    repair_printline(&(k[i].repairList[j]), file);
	    nConflicts++;
	}
	if ( setacl )
	    fprintf(file, "\tsetacl %s %s\n", inf->user, inf->rights);
	if ( setmode )
	    fprintf(file, "\tsetmode %s\n", inf->mode);
	if ( setowner )
	    fprintf(file, "\tsetowner %s\n", inf->owner);
    }

    /* Close the fixfile */
    if ((file != stdout) && (file != stderr))
	fclose(file);

    if (compareVV(nreps, names, conf)) {
	if (!nConflicts) {
	    // warn the user if no conflicts were detected otherwise
	    printf("The fix file may be empty but .... \n"
		   "You still need a dorepair because the "
		   "Version state is different\n");
	}
	nConflicts++;
    }

    if ((conf->fid.Vnode == 1) && (conf->fid.Unique == 1) &&
	(compareQuotas(nreps, names))) {
	nConflicts++;
	printf("Your volume quota is different on the various replicas\n");
	printf("You SHOULD talk to one of the system administrators soon\n");
    }

    /* clean up the malloced memory */
    resClean(nreps, dirs, k);
    for (i = 0; i < nreps; i++) freeif(names[i]);
    free(names);

    if (ret == NNCONFLICTS) {
      strerr(msg, msgsize, "Name/name conflicts still exist");
      return(-2);
    }

    return(nConflicts);

   Cleanup:
    resClean(nreps, dirs, k);
    for (i = 0; i < nreps; i++) freeif(names[i]);
    free(names);
    return(-1);
}

/* Does a repair on conf using fixfile at ufixpath
 * Fprints results to res if non-NULL
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int DoRepair(struct conflict *conf, char *ufixpath, FILE *res, char *msg, int msgsize) {
    char space[DEF_BUF], fixpath[MAXPATHLEN];
    VolumeId *vids;
    struct replica *rwv;
    int *rcodes;
    int i, rc;

    if (conf == NULL) {
      strerr(msg, msgsize, "NULL conf");
      return(-1);
    }

    if (conf->dirconf) {   /* directory conflict */
      if (makedff(ufixpath, fixpath, msg, msgsize) < 0)
	return(-1);
    }
    else                   /*  file conflict */
      strncpy(fixpath, ufixpath, sizeof(fixpath));

    /* Do the repair */
    rc = dorep(conf, fixpath, space, sizeof(space));
    if (rc < 0 && errno != ETOOMANYREFS) {
	strerr(msg, msgsize, "VIOC_REPAIR failed on %s: %s",
	       conf->rodir, strerror(errno));
	return(-1);
    }

    /* Print out the results of VIOC_REPAIR pioctl */
    if (res != NULL) {
	vids = (VolumeId *)space;
	rcodes = (int *)&((VolumeId *)space)[MAXHOSTS];
	for (rwv = conf->head; rwv != NULL; rwv = rwv->next) {
	    for (i = 0; ((i < MAXHOSTS) && (vids[i] != rwv->fid.Volume)); i++);
	    if  (i < MAXHOSTS) {
		fprintf(res, "Repair actions performed on %s have %s",
			rwv->srvname, rcodes[i] ? "failed" : "succeeded");
		if (rcodes[i]) {
		    fprintf(res, " (%d).\n%s%s", rcodes[i], 
			    "Possible causes: disconnection, lack of authentication, lack of server space\n",
			    "Fix file contains operations that are in conflict against the server replica.\n");
		}
		else fprintf(res, ".\n");
		vids[i] = 0;
	    }
	    else
	      if(rwv->fid.Volume != conf->fid.Volume)
		fprintf(res, "No return code for actions performed on %s! (vid %08x)\n", rwv->srvname, rwv->fid.Volume);
	}
	for (i = 0; (i < MAXHOSTS); i++)
	    if (vids[i]) fprintf(res, "Return code %d for unexpected vid %08x!\n", rcodes[i], vids[i]);
    }

    /* Clean up */
    if (conf->dirconf)
	unlink(fixpath); /* ignore rc */

    return(0);
}

/* Ends the repair session on repv and frees up memory associated with repv
 * Only commits changes in for a local/global conflict if commit == 1
 * Returns 0 on success, -1 on error and fills in msg if non-NULL
                          (but still frees memory) */
int EndRepair(struct conflict *conf, int commit, char *msg, int msgsize) {
    char space[DEF_BUF], cmd[32];
    struct ViceIoctl vioc;
    int rc;

    if (conf == NULL) {
      strerr(msg, msgsize, "NULL conf");
      return(-1);
    }

    if (conf->local) { /* local/global conflict */
	sprintf(cmd, "%d %d", REP_CMD_END, commit);
	vioc.in = cmd;
	vioc.in_size = (short)(strlen(cmd) + 1);
	vioc.out = space;
	vioc.out_size = sizeof(space);
	if ((rc = pioctl("/coda", _VICEIOCTL(_VIOC_REP_CMD), &vioc, 0)) < 0)
	    strerr(msg, msgsize, "REP_CMD_END failed: %s", strerror(errno));
	else if (strcmp(vioc.out, "repair session completed") != 0) {
	    strerr(msg, msgsize, "%s", vioc.out);
	    rc = -1;
	}
    }

    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = NULL;
    vioc.out_size = 0;
    errno = 0;
    if ((rc = pioctl(conf->rodir, _VICEIOCTL(_VIOC_DISABLEREPAIR), &vioc, 0)) < 0)
      strerr(msg, msgsize, "DISABLEREPAIR %s: %s", conf->rodir, strerror(errno));

    repair_finish(conf); /* free()'s everything */
    return(rc);
}

/* Removes inconsistencies on repv (first does a repair, then clears the inc)
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int RemoveInc(struct conflict *conf, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF], tmppath[MAXPATHLEN];
    ViceFid fixfid;
    char fixrealm[MAXHOSTNAMELEN];
    vv_t fixvv;
    char *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL, **names;
    int nreplicas, rc, i, fd = 0;
    struct listhdr *repairlist = NULL;
    resreplica *dirs = NULL;

    user = rights = owner = mode = NULL;

    if (conf == NULL) {
      strerr(msg, msgsize, "NULL conf");
      return(-1);
    }

    if ((nreplicas = getVolrepNames(conf, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }

    if (conf->dirconf) { /* directory conflict */

	/* get the dirent's and create list of children to be removed */
	if (getunixdirreps(nreplicas, names, &dirs)) {
	    strerr(msg, msgsize, "Could not get needed replica information");
	    rc = -1;
	    goto Error;	    
	}
	getremovelists(nreplicas, dirs, &repairlist);

	/* convert list to internal format */
	strcpy(tmppath, "/tmp/REPAIR.XXXXXX");
	fd = mkstemp(tmppath);
	if (fd < 0) {
	  rc = -1;
	  strerr(msg, msgsize, "Couldn't make tmp file %s", tmppath);
	  goto Error;
	}

	/* write out internal rep */
	rc = repair_putdfile(tmppath, nreplicas, repairlist);
	if (rc) {
	    strerr(msg, msgsize, "Couldn't put repair list into file %s",
		   tmppath);
	    if(fd > 2)
	      close(fd);
	    unlink(tmppath); /* Clean up */
	    goto Error;
	}

	rc = dorep(conf, tmppath, NULL, 0); /* do the repair */
	if (rc < 0 && errno != ETOOMANYREFS) {
	    strerr(msg, msgsize, "REPAIR %s: %s", conf->rodir,
		   strerror(errno));
	    if(fd > 2)
	      close(fd);
	    unlink(tmppath); /* Clean up */
	    goto Error;
	}

	if(fd > 2)
	  close(fd);
	unlink(tmppath); /* Clean up */

	/* object is still inconsistent -- try to clear it */
	if ((rc = ClearInc(conf, msgbuf, sizeof(msgbuf))) < 0) {
	    strerr(msg, msgsize, "Error clearing inconsistency: %s", msgbuf);
	    goto Error;
	}
    }
    else { /* file conflict */
	rc = repair_getfid(names[0], &fixfid, fixrealm, &fixvv,
			   msgbuf, sizeof(msgbuf));
	if (rc) {
	    strerr(msg, msgsize, "repair_getfid(%s): %s", names[0], msgbuf);
	    goto Error;
	}

	sprintf(tmppath, "@%08x.%08x.%08x@%s",
		fixfid.Volume, fixfid.Vnode, fixfid.Unique, fixrealm);

	rc = dorep(conf, tmppath, NULL, 0); /* do the repair */
	if ((rc < 0) && (errno != ETOOMANYREFS))
	    strerr(msg, msgsize, "REPAIR %s: %s",
		   conf->rodir, strerror(errno));
    }

 Error:

    /* clean up malloced memory */
    if (conf->dirconf) /* directory conflict */
	resClean(nreplicas, dirs, repairlist);

    for (i = 0; i < nreplicas; i++)
	freeif(names[i]);

    free(names);
    return(rc);
}

int isLocal(resreplica *dir) {
    int i;

    /* local dirs get entry vnodes and storeID's of 0xffff */
    for (i = dir->entry1; i < (dir->entry1 + dir->nentries); i++) {
	if (~(direntriesarr[i].fid.Vnode) || ~(direntriesarr[i].VV.StoreId.Host))
	    return(0);
    }
    if (~(dir->fid.Vnode)) return(0);   /* and vnode is 0xffff */
    if (dir->al != NULL) return(0); /* and NULL acl's */
    return(1);
}

int compareAcl(int nreplicas, resreplica *dirs) {
    int init, i, j;
    struct Acl *al0 = NULL, *ali;

    for (init = 0; init < nreplicas; init++) {
	if (!(isLocal(&(dirs[init]))))
	    al0 = dirs[init].al;
    }
    CODA_ASSERT(al0);

    for (i = init + 1; i < nreplicas; i++){

	if (isLocal(&(dirs[i])))
	    continue;

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

int compareOwner(int nreplicas, resreplica *dirs)
{
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].owner != dirs[0].owner)
	    return -1;
    }
    return 0;
}

int compareQuotas(int nreplicas, char **names)
{
    char piobuf[DEF_BUF];
    struct ViceIoctl vio;
    vio.in = NULL;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = sizeof(piobuf);

    /* Do the pioctl */
    int rc = pioctl(names[0], _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
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
	rc = pioctl(names[i], _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
	if (rc < 0) {fflush(stdout); perror(names[i]); return(1);}
	/* Get pointers to output fields */
	vs = (VolumeStatus *)piobuf;
	if ((vs->MinQuota !=  minquota0) || (vs->MaxQuota != maxquota0))
	    return 1;
    }
    return 0;
}

int compareStatus(int nreplicas, resreplica *dirs) {
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].modebits != dirs[0].modebits)
	    return -1;
    }
    return 0;
}

int compareVV(int nreplicas, char **names, struct conflict *conf)
{
    char msgbuf[DEF_BUF];
    vv_t vv[MAXHOSTS];
    vv_t *vvp[MAXHOSTS];
    int nhosts = 0;
    int i;
    int HowMany = 0;

    for (i = 0; i < MAXHOSTS; i++) 
	vvp[i] = NULL;

    for (i = 0; i < nreplicas; i++) {
      char replicaname[MAXPATHLEN], *lastslash = NULL;

      /* Puneet's way to get replica name */
      strcpy(replicaname, names[i]);
      lastslash = strrchr(replicaname, '/'); // the trailing /
      if(lastslash == NULL)
	continue;
      *lastslash = '\0';
      lastslash = strrchr(replicaname, '/'); // the / before the replica name
      if (lastslash == NULL)
	continue;
      lastslash++;

      if(!strcmp(lastslash, "_localcache")) /* local VV's don't mean anything */
	continue;

      if (repair_getfid(names[i], NULL, NULL, &vv[nhosts], msgbuf, sizeof(msgbuf)))
	printf("Couldn't get vv for %s: %s\n", names[i], msgbuf);
      else
	nhosts++;
    }
    for (i = 0; i < nhosts; i++)
	vvp[i] = &vv[i];
    if (VV_Check_IgnoreInc(&HowMany, vvp, 1) != 1)
	return(1);
    return(0);
}

/* Basically just nice padding for the VIOC_REPAIR pioctl.
 * Calls the pioctl, and passes back results in buf if non-NULL
 * Returns the pioctl return code */
int dorep(struct conflict *conf, char *fixpath, char *buf, int len) {
    char space[DEF_BUF];
    struct ViceIoctl vioc;
    int rc;

    if (buf == NULL) { 
	buf = space;
	len = sizeof(space);
    }
    vioc.in = fixpath;
    vioc.in_size = strlen(fixpath) + 1;
    vioc.out = buf;
    vioc.out_size = len;
    memset(buf, 0, len);
    rc = pioctl(conf->rodir, _VICEIOCTL(_VIOC_REPAIR), &vioc, 0);
    return(rc);
}

/* Assumes rw replicas are all of the same type
 *    (must be true if this is a leftmost conflict)
 * Fills in dirconf field in repv
 * Returns 0 on success, -1 on failure */
int findtype(struct conflict *conf) {
    char tmppath[MAXPATHLEN];
    struct stat sbuf;
    struct replica *tmp;

    tmp = conf->head;
    if (tmp == NULL) return(-1); /* need at least 1 replica */

    /* do the first one manually */
    sprintf(tmppath, "%s/%s", conf->rodir, tmp->compname);
    if (lstat(tmppath, &sbuf) < 0) return(-1);
    conf->dirconf = ((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
    tmp = tmp->next;

    while (tmp != NULL) {
	sprintf(tmppath, "%s/%s", conf->rodir, tmp->compname);
	if (lstat(tmppath, &sbuf) < 0) return(-1);
	if (conf->dirconf != (((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0))
	    return(-1);
	tmp = tmp->next;
    }
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
	    if (ISDIRVNODE(rde->fid.Vnode))
		rep.opcode = REPAIR_REMOVED;
	    else
		rep.opcode = REPAIR_REMOVEFSL;
	    strcpy(&(rep.name[0]), &(rde->name[0]));
	    rep.parms[0] = 0;
	    rep.parms[1] = 0;
	    rep.parms[2] = 0;
	    InsertListHdr(nreplicas, &rep, repairlist, i);
	}
    }
}

/* Allocates and returns an array of replica names (complete paths) in ***names
 * Returns number of replicas in array on success, -1 on failure (after cleaning up) */
int getVolrepNames(struct conflict *conf, char ***names, char *msg, int msgsize) {
    struct replica *rwv;
    int i, nreps;

    /* count and allocate replicas */
    for (nreps = 0, rwv = conf->head; rwv != NULL; rwv = rwv->next, nreps++);
    if (nreps == 0) {
	strerr(msg, msgsize, "No accessible replicas");
	return(-1);
    }
    if ((*names = (char **)malloc(nreps * sizeof(char *))) == NULL) {
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }

    for (i = 0; i < nreps; (*names)[i++] = NULL); /* initialize all to NULL */

    for (i = 0, rwv = conf->head; rwv != NULL; rwv = rwv->next, i++) {
	/* 3 is for middle slash, trailing slash (if directory) and closing '\0' */
	(*names)[i] = (char *)malloc((strlen(rwv->compname) + strlen(conf->rodir) + 3) * sizeof(char));
	if ((*names)[i] == NULL) {
	    for (i = 0; i < nreps; nreps++) freeif((*names)[i]);
	    free(*names);
	    strerr(msg, msgsize, "Malloc failed");
	    return(-1);
	}

	if (conf->dirconf) sprintf((*names)[i], "%s/%s/", conf->rodir, rwv->compname);
	else sprintf((*names)[i], "%s/%s" , conf->rodir, rwv->compname);
    }

    return (nreps);
}

int compareFids(ViceFid *a, ViceFid *b) {

  if(!a || !b)
    return -1;

  if(a->Volume != b->Volume)
    return 1;

  if(a->Vnode != b->Vnode)
    return 1;

  if(a->Unique != b->Unique)
    return 1;

  return 0;
}

/*  extfile: external (ASCII) rep
 *  intfile: internal (binary) rep
 *  Returns 0 on success, -1 on failure */
int makedff(char *extfile, char *intfile, char *msg, int msgsize) {
    struct listhdr *hl;
    int hlc, rc;

    /* parse input file and obtain internal rep  */
    rc = repair_parsefile(extfile, &hlc, &hl);
    if (rc < 0) {
        strerr(msg, msgsize, "Error in fix file");
        return(-1);
    }

    /* generate temp file name */
    strcpy(intfile, "/tmp/REPAIR.XXXXXX");
    rc = mkstemp(intfile);
    if(rc < 0) {
        strerr(msg, msgsize, "Error creating tmp file %s", intfile);
        return(-1);
    }

    /* write out internal rep */
    rc = repair_putdfile(intfile, hlc, hl);
    if (rc) {
        strerr(msg, msgsize, "Error in fix file");
        return(-1);
    }
    /* repair_printfile(intfile); */

    free(hl);
    return(0);
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

