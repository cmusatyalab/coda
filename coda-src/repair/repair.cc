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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/repair/repair.cc,v 4.4 1997/10/23 19:24:25 braam Exp $";
#endif /*_BLURB_*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <parser.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/stat.h>
#include <setjmp.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <inodeops.h>
#include <rpc2.h>
#include <signal.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <venusioctl.h>
#include <auth2.h>
#include <repio.h>
#include <resolve.h>
#include <inconsist.h>
#include "repair.h"

extern int IsCreatedEarlier C_ARGS((struct listhdr **, int, long, long));

#define HELPDIR "/coda/project/coda/doc/cihelp/repair"
#define ISDIR(vnode) ((vnode) & 1)  /* directory vnodesare odd */

#ifdef REPAIR_STATS
#define DOREPAIRDATADIR "/to/be/defined/by/anyone/needs/to/collect/repair/stats"
#endif REPAIR_STATS

int session = NOT_IN_SESSION;
int  repair_DebugFlag;
char beginRepDefault[MAXPATHLEN];
char doRepDefault[MAXPATHLEN];
char compDirDefault[MAXPATHLEN];
char compOutputFile[MAXPATHLEN];// filename for output of last docompare
struct stat compOutputStatBuf;	// file information for the repair
				// commands file
struct stat doInputStatBuf;

PRIVATE jmp_buf NormalJmpBuf;  /* to exit from ci loops gracefully */

PRIVATE void	SetDefaultPaths();
PRIVATE int	compareVV(int, char **, struct repvol *);
PRIVATE void	GetArgs(int argc, char *argv[]);
PRIVATE	int	getcompareargs(int, char **, char *, char *);
PRIVATE	int	getlistargs(char *, char *);
PRIVATE int	getremoveargs(int, char **, char *);
PRIVATE void	getremovelists(int, resreplica *, struct listhdr **);
PRIVATE int	getrepairargs(int, char **, char *, char *, char *);
PRIVATE int	makedff(char *extfile, char *intfile);
PRIVATE int	doCompare(int, struct repvol *, char **, char *, char *, ViceFid *);
PRIVATE int	compareStatus(int, resreplica *);
PRIVATE int	compareQuotas(int , char **);
PRIVATE int 	compareOwner(int, resreplica *);
PRIVATE void	printAcl(struct Acl *);
PRIVATE int	compareAcl(int, resreplica *);
PRIVATE int 	GetReplicaNames(char **, int , char *);
PRIVATE int 	GetTokens();
PRIVATE void 	INT(int, int, struct sigcontext *);

#define INITHELPMSG 	\
"This repair tool can be used to manually repair server/server \n\
or local/global conflicts on files and directories. \n\
You will first need to do a \"beginrepair\" to start a repair\n\
session where messages about the nature of the conflict and\n\
the commands that should be used to repair the conflict will\n\
be displayed. Help message on individual commands can also be\n\
obtained by using the \"help\" facility. Finally, you can use the\n\
\"endrepair\" or \"quit\" to terminate the current repair session.\n"

/*
  BEGIN_HTML
  <a name="ci"><strong> ci-command interface </strong></a>
  END_HTML
*/

/* Relax, command parser allows abbreviations of command names */
command_t list[] = {
  {"beginrepair", beginRepair, 0, ""},  	/* <reppathname> */
  {"endrepair", endRepair, 0, ""},  		/* no args */
  {"dorepair", doRepair, 0, ""},     		/* <reppathname> <fixfilename> */
  {"comparedirs", compareDirs, 0, ""},	 	/* <reppathname>, <fixfile> */
  {"clearinc", clearInc, 0, ""},	 	/* <reppathname> */
  {"removeinc", removeInc, 0, ""}, 	 	/* <reppathname> */
  {"quit", quit, 0, ""},         		/* no args */
  {"checklocal", checkLocal, 0, ""},	 	/* no args */
  {"listlocal", listLocal, 0, ""},	 	/* no args */
  {"preservelocal", preserveLocal, 0, ""},	/* no args */
  {"preservealllocal", preserveAllLocal, 0, ""},/* no args */
  {"discardlocal", discardLocal, 0, ""},	/* no args */
  {"discardalllocal", discardAllLocal, 0, ""},	/* no args */
  {"setglobalview", setGlobalView, 0, ""},	/* no args */
  {"setmixedview", setMixedView, 0, ""},	/* no args */
  {"setlocalview", setLocalView, 0, ""},	/* no args */
  { 0, 0, 0, ""},
};

main(int argc, char *argv[])
    {
    int rc;

    /* parse args */
    GetArgs(argc, argv);

    SetDefaultPaths();

    /* initialize normal mode setjmp */
    rc = setjmp(NormalJmpBuf);
    if (rc){exit(0);}

    signal(SIGINT, (void (*)(int))INT);	/* catch SIGINT */

    /* check if help is available and accessible */
    if (access(HELPDIR, R_OK|X_OK) < 0)	{
	printf("The help directory \"%s\" is not accessible\n", HELPDIR);
    }

    /* print a message indicating basic repair methodology */
    printf(INITHELPMSG);
    /* warn user if no tokens are found */
    if (GetTokens()) {
	printf("\n\n\nWARNING: YOU DON'T HAVE TOKENS.  "
	       "YOU MIGHT WANT TO AUTHENTICATE FIRST\n\n");
    }
    /* Sit in command loop */
    Parser_init("*", list);
    Parser_commands();
    }

/*
  BEGIN_HTML
  <a name="dorepair"><strong> perform the repair actions as specified by the repair-file </strong></a>
  END_HTML
 */

void doRepair(int largc, char **largv)
    /* args: <reppathname> <fixfilename> */
    {
    enum {FIXDIR, FIXFILE} fixtype;
    VolumeId vid;
    struct repvol *repv;
    struct rwvol *rwv;
    struct repvol *saverepv;
    struct stat statbuf;
    struct ViceIoctl vioc;
    char space[2048];
    int i, rc;
    char uconflictpath[MAXPATHLEN], ufixpath[MAXPATHLEN];
    char realfixpath[MAXPATHLEN];
    char reppath[MAXPATHLEN], tmppath[MAXPATHLEN];
    char prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    long vids[MAXHOSTS];
    long rcodes[MAXHOSTS];
    

    switch (session) {
    case LOCAL_GLOBAL:
	printf("\"dorepair\" can only be used to repair a server/server conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    /* Obtain parameters and confirmation from user */
    rc = getrepairargs(largc, largv, uconflictpath, ufixpath, realfixpath);
    if (rc < 0) return;

    /* Is this the leftmost element in conflict? */
    rc = repair_isleftmost(uconflictpath, reppath); 
    if (rc < 0) return;

    /* Is the volume locked for repair? */
    rc = repair_getmnt(reppath, prefix, suffix, &vid);
    if (rc < 0) return;
    if (repair_findrep(vid, &repv) < 0)
	{printf("You must do \"beginrepair\" first\n"); return;}
    saverepv = repv;
    /* Is it a directory?
       Assumption: rw replicas are all of the same type.
       This must be true if this is a leftmost conflict
    */
    assert(repv->rwhead);          /* better have one rw replica at least! */
    rwv = repv->rwhead;
    while (rwv != 0) {
	sprintf(tmppath, "%s/%s", reppath, rwv->compname);
	rc = lstat(tmppath, &statbuf);

	/* If it's a symlink, that rep must not be available, skip over it. */
	if ((statbuf.st_mode & S_IFMT) == S_IFLNK) rc = -1;

	if (rc >= 0) break;
	else 
	    rwv = rwv->next;
    }
    if (rc < 0) {
	myperror(" lstat", tmppath, errno);  
	printf("No replicas accessible\n");
	printf("Possible causes: disconnection or lack of authentication\n");
	return;
    }
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR) fixtype = FIXDIR;
    else fixtype = FIXFILE;

    DEBUG(("fixtype = %d\n", fixtype));
    if (fixtype == FIXDIR) rc = makedff(realfixpath, tmppath);     /* Create internal form of fix file */
    if (rc < 0) { 
	printf("Error in the fix file\n");
	return;
    }
    DEBUG(("ufixpath = \"%s\"  tmppath = \"%s\"\n", ufixpath,
	   tmppath));

#ifdef	REPAIR_STATS
    /* 
     * The following code for collecting repair stats is ifdef'ed out.
     * If anyone needs to resume stats collection, please redefine DOREPAIRDATADIR.
     */
    // collect statistics about the repair
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
	else 
	    collectstats = 0;
    }
#endif REPAIR_STATS

    /* Do the repair */
    if (fixtype == FIXDIR)
        {
	vioc.in_size = (short)(1+strlen(tmppath));
	vioc.in = tmppath;
	}
    else
	{
	vioc.in_size = (short)(1 + strlen(ufixpath));
	vioc.in  = ufixpath;
	}
    vioc.out_size = (short)sizeof(space);
    vioc.out = space;
    bzero(space, (short)sizeof(space));
    rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
    if (rc < 0 && errno != ETOOMANYREFS) 
	myperror(" REPAIR", reppath, errno);
    {	
	long *l;
	l = (long *) space;
	for (i = 0; i < MAXHOSTS; i++)
	    vids[i] = l[i];
	for (i = 0; i < MAXHOSTS; i++)
	    rcodes[i] = l[i+MAXHOSTS];
	for (i = 0; i < MAXHOSTS; i++)
	    if (vids[i]) {
		/* find server name */
		rwv = saverepv->rwhead;
		while(rwv && vids[i] != rwv->vid)
		    rwv = rwv->next;
		assert(vids[i] == rwv->vid);
		printf("repair actions performed on %s have %s ",
		       rwv->srvname, rcodes[i] ? "failed" : "succeeded");
		if (rcodes[i]) {
		    printf("(%d)\n", rcodes[i]); 
		    printf("Possible causes: disconnection, lack of authentication, lack of server space\n");	
		    printf("fix file contains operations that are in conflict against the server replica .... \n");
		} else {
		    printf("\n");
		}
	    }
    }

#ifdef REPAIR_STATS
    if (collectstats) {
	/* 
	 * The following code for collecting repair stats is ifdef'ed out.
	 * If anyone needs to resume stats collection, please redefine DOREPAIRDATADIR.
	 */

	// open the statistics file 
	char statfilename[MAXPATHLEN];
	uid_t uid = getuid();
	sprintf(statfilename, "%s%u.XXXXXX", DOREPAIRDATADIR, uid);
	int statfd = mkstemp(statfilename);
	if (statfd>0) {
	    FILE *fp = fdopen(statfd, "w");
	    assert(fp);
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
	    // print the return codes
	    for (i = 0; i < MAXHOSTS; i++)
		if (vids[i]) {
		    /* find server name */
		    rwv = saverepv->rwhead;
		    while(rwv && vids[i] != rwv->vid)
			rwv = rwv->next;
		    assert(vids[i] == rwv->vid);
		    fprintf(fp, "%s %s\n", 
			    rwv->srvname, rcodes[i] ? "failed" : "succeeded");
		}
	    fclose(fp);
	    close(statfd);
	}
    }
#endif REPAIR_STATS

    /* Clean up */
    if (fixtype == FIXDIR) unlink(tmppath); /* ignore rc */
    return;
    }

/*
  BEGIN_HTML
  <a name="comparedirs"><strong> compare accessible replicas of the directory being repaired </strong></a>
  END_HTML
*/

void compareDirs(int largc, char **largv)
   /* args: <reppathname> <fixfilename> */
{
    int	    nreplicas;
    char    **names;
    int rc, i;
    int	sizeOfPath;
    VolumeId vid;
    struct  repvol *repv;
    char    reppath[MAXPATHLEN], filepath[MAXPATHLEN];
    char    prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    char    tmppath[MAXPATHLEN];
    char    uconflictpath[MAXPATHLEN];
    ViceFid confFid;
    vv_t    confvv;
    struct stat buf;

    switch (session) {
    case LOCAL_GLOBAL:
	printf("\"compardirs\" can only be used to repair a server/server conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    /* Obtain parameters from user */
    rc = getcompareargs(largc, largv, uconflictpath, filepath);
    if (rc < 0) return;

    /* Is this the leftmost element in conflict? */
    rc = repair_isleftmost(uconflictpath, reppath); 
    if (rc < 0) return;

    /* Is the volume locked for repair */
    rc = repair_getmnt(reppath, prefix, suffix, &vid);

    if (rc < 0) return;
    if (repair_findrep(vid, &repv) < 0){
	printf("You must do \"beginrepair\" first\n"); 
	return;
    }
    
    assert(repv->rwhead);   /* better have atleast one rw replica */

    if (repair_getfid(uconflictpath, &confFid, &confvv)) {
	printf("%s isn't in /coda... Are you sure this object is in conflict\n", 
	       uconflictpath);
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
    rc = doCompare(nreplicas, repv, names, filepath, prefix, &confFid);
    if (!rc)
	printf("No conflicts detected \n");
    else if (rc > 0)
	printf("Operations to resolve conflicts are in %s\n", 
	       filepath);	
    
    else if (rc == NNCONFLICTS) 
	if (Parser_getbool("Do you want to repair the name/name conflicts", 1)) {
	    /* repair the name/name conflicts */
	    struct ViceIoctl vioc;
	    char space[2048];
	    rc = makedff(filepath, tmppath);
	    vioc.in_size = (short) (1+strlen(tmppath));
	    vioc.in = tmppath;
	    vioc.out_size = (short) sizeof(space);
	    vioc.out = space;
	    bzero(space, (int)sizeof(space));
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
    
  Exit:
    /* free the malloced space */
    for ( i = 0; i < nreplicas; i++)
	free(names[i]);
    free(names);
}

int allowclear = 0;
void clearInc(int largc, char **largv)
{
    int	    rc, i;
    char    *p;
    char    reppath[MAXPATHLEN], uconflictpath[MAXPATHLEN];
    char    prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    VolumeId	vid;
    struct repvol *repv;
    int	    sizeOfPath;
    char    **names;
    int	    nreplicas;
    struct stat buf;

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
    rc = repair_isleftmost(uconflictpath, reppath); 
    if (rc < 0) return;
    
    /* Is the volume locked for repair */
    rc = repair_getmnt(reppath, prefix, suffix, &vid);
    if (rc < 0) return;
    if (repair_findrep(vid, &repv) < 0){
	printf("You must do \"beginRepair\" first\n"); 
	return;
    }

    assert(repv->rwhead);   /* better have atleast one rw replica */

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
    if (!doCompare(nreplicas, repv, names, "/dev/null", prefix, &confFid)){
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
		myperror("SETVV", names[i], errno);
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
    
    return;
}


/*
  BEGIN_HTML
  <a name="removeinc"><strong> remove the inconsistent object and terminate the current repair session </strong></a> 
  END_HTML
*/

void removeInc(int largc, char **largv)
     /* removes inconsistent objects;
	first does  a repair, clears the inc 
	and then removes the object */
{
    int	    nreplicas;
    char    **names;
    int rc, i, j;
    int	sizeOfPath;
    char    reppath[MAXPATHLEN], uconflictpath[MAXPATHLEN];
    char    prefix[MAXPATHLEN], suffix[MAXPATHLEN];
    char    tmppath[MAXPATHLEN];
    VolumeId vid;
    struct  repvol *repv;
    struct  rwvol  *rwv;
    struct listhdr *repairlist;
    resreplica *dirs;
    struct stat buf;
    FILE *file;
    enum {FIXDIR, FIXFILE, UNKNOWN} fixtype;

    switch (session) {
    case NOT_IN_SESSION:
	printf("please use \"beginrepair\" first to determine the nature of the conflict\n");
	return;
    case LOCAL_GLOBAL:
	printf("\"removeinc\" can only be used to repair a server/server conflict\n");
	return;
    case SERVER_SERVER:
	printf("\"removeinc\" will terminate the current repair session\n");
    }

    rc = 0;
    fixtype = UNKNOWN;

    /* Obtain parameters */
    rc = getremoveargs(largc, largv, uconflictpath);
    if (rc < 0) return;

#if	0
    /* do a begin repair first */
    if (beginRepair(uconflictpath) != 0) {
	printf("Couldn\'t do begin repair on %s\n",
	       uconflictpath);
	return;
    }
#endif	0

    /* Is this the leftmost element in conflict? */
    rc = repair_isleftmost(uconflictpath, reppath); 
    if (rc < 0) return;

    /* Is the volume locked for repair */
    rc = repair_getmnt(reppath, prefix, suffix, &vid);

    if (rc < 0) return;
    if (repair_findrep(vid, &repv) < 0){
	printf("You must do \"beginrepair\" first\n"); 
	return;
    }
    
    assert(repv->rwhead);   /* better have atleast one rw replica */

    /* get repair type */
    {
	rwv = repv->rwhead;
	while (rwv != 0) {
	    sprintf(tmppath, "%s/%s", repv->rodir, rwv->compname);
	    rc = lstat(tmppath, &buf);  
	    if (rc >= 0) break;
	    else 
		rwv = rwv->next;
	}
	if (rc < 0) {
	    myperror(" lstat", tmppath, errno);  
	    printf("NO replicas accessible\n");
	    return;
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
	char space[2048];
	
	if (rc = repair_getfid(names[0], &fixfid, &fixvv)) {
	    printf("Could not get fid for %s\n", names[0]);
	    printf("Possible causes: disconnection or lack of authentication\n");
	    goto Error;
	}

	sprintf(tmppath, "@%x.%x.%x", fixfid.Volume, 
		fixfid.Vnode, fixfid.Unique);

	/* do the repair */
	vioc.in_size = (short) (1+strlen(tmppath));
	vioc.in = tmppath;
	vioc.out_size = (int)sizeof(space);
	vioc.out = space;
	bzero(space, (int)sizeof(space));
	rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
	if (rc < 0 && errno != ETOOMANYREFS) 
	    myperror(" REPAIR", reppath, errno);
    }
    else {
	/* get the directory entries and create list of 
	    children to be removed */
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
		    assert(repairlist[i].replicaId == rwv->vid);
		}
		fprintf(file,"\nreplica %s %x \n", rwv->srvname, 
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
	    printf("Coudn't put repair list into file %s\n",
		   tmppath);
	    goto Error;
	}
	
	/* Do the repair */
	{
	    struct ViceIoctl vioc;
	    char space[2048];
	    
	    vioc.in_size = (short)(1+strlen(tmppath));
	    vioc.in = tmppath;
	    vioc.out_size = (short)sizeof(space);
	    vioc.out = space;
	    bzero(space, (int)sizeof(space));
	    rc = pioctl(reppath, VIOC_REPAIR, &vioc, 0);
	    if (rc < 0 && errno != ETOOMANYREFS) 
		myperror(" REPAIR", reppath, errno);
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
		    if (!doCompare(nreplicas, repv, names, "/dev/null", prefix, &confFid)){
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
				myperror("SETVV", names[i], errno);
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
    
    /* disable repair after we are done */
    {
	struct ViceIoctl vioc;
	int retcode;
	vioc.out_size = 0;
	vioc.in_size = 0;
	vioc.out = 0;
	repair_unlinkrep(repv);
	retcode = pioctl(repv->mnt, VIOC_DISABLEREPAIR, &vioc, 0);
	if (retcode < 0) myperror(" DISABLEREPAIR", repv->mnt, errno);
	repair_finish(repv);
    }
    if (!rc) {
	/* no error - try to remove the object */
	if (fixtype == FIXDIR) {
	    if (rmdir(uconflictpath)) {
		printf("Could not remove %s\n", 
		       uconflictpath);
		return;
	    }
	}
	else {
	    if (unlink(uconflictpath)) {
		printf("Could not remove %s\n", 
		       uconflictpath);
		return;
	    }
	}
    }
    session = NOT_IN_SESSION;
    return;
}

/*
  BEGIN_HTML
  <a name="endrepair"><strong> end the current repair session </strong></a>
  END_HTML
*/

void endRepair(int largc, char **largv) 
{
    /* only terminate the current repair session */

    switch (session) {
    case NOT_IN_SESSION:
	{
	    printf("There is no repair session to end\n");
	    break;
	}
    case LOCAL_GLOBAL:
	{
	    struct ViceIoctl vioc;
	    int rc;
	    int commit;

	    if (Parser_getbool("commit the local/global repair session?", 1)) {
		commit = 1;
	    } else {
		commit = 0;
	    }

	    char space[2048];
	    char choice[10];
	    vioc.out = space;
	    vioc.out_size = 2048;
	    sprintf(choice, "%d %d", REP_CMD_END, commit);
	    vioc.in = choice;
	    vioc.in_size = (short) (strlen(choice) + 1);
	    
	    errno = 0;
	    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
	    if (rc < 0) {
		perror("VIOC_REP_CMD(REP_CMD_END)");
	    }
	    if (strcmp(vioc.out, "repair session completed"))
	      printf("%s\n", vioc.out);

	    session = NOT_IN_SESSION;
	    break;
	}
    case SERVER_SERVER: 
	{
	    struct ViceIoctl vioc;
	    int rc;
	    struct repvol *repv;

	    /* Release volume-level locks, for server/server session */
	    vioc.out = 0;
	    vioc.out_size = 0;
	    vioc.in_size = 0;
	    
	    /* Release all volume-level locks, and clean up */
	    while (RepVolHead)
	      {/* new element at head on each iteration */
		  repv = RepVolHead;
		  repair_unlinkrep(repv);
		  errno = 0;
		  rc = pioctl(repv->mnt, VIOC_DISABLEREPAIR, &vioc, 0);
		  if (rc < 0) myperror(" DISABLEREPAIR", repv->mnt, errno);
		  repair_finish(repv);
	      }

	    session = NOT_IN_SESSION;
	}
    }
}

/*
  BEGIN_HTML
  <a name="quit"><strong> quit the repair tool </strong></a>
  END_HTML
*/


void quit(int largc, char **largv)
{
    /* terminate the current session, if there is one */
    if (session != NOT_IN_SESSION) 
      endRepair(0, (char **)0);

    /* exit the repair tool */
    Parser_exit(largc, largv);
}


/* 
  user passed path (realpath) is what the user gives as the fixfile.
  if the object is in coda then fixpath contains the @fid representation of the object
 */
PRIVATE int getrepairargs(int largc, char **largv, char *conflictpath, char *fixpath /* OUT */,
			  char *realpath)
    {
    char *p;
    char msg[2*MAXPATHLEN+100];
    ViceFid fixfid;
    vv_t fixvv;
    
    if (largc == 1) {
      Parser_getstr("Pathname of object in conflict?", 
	       (*doRepDefault == '\0') ? beginRepDefault : doRepDefault,
	       conflictpath, MAXPATHLEN);
      Parser_getstr("Pathname of fix file?", 
	       (*compDirDefault == '\0') ? "" : compDirDefault, fixpath,
	       MAXPATHLEN);
    } else {
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
    if (!repair_getfid(fixpath, &fixfid, &fixvv))
	{
/* 	printf("%s is in Coda and cannot be used as the fix file\n", fixpath);
	return(-1);
*/  
	sprintf(fixpath, "@%x.%x.%x", fixfid.Volume, fixfid.Vnode, fixfid.Unique);
	}
	
    sprintf(msg, "OK to repair \"%s\" by fixfile \"%s\"?", conflictpath, fixpath);
    if (!Parser_getbool(msg, 0)) return(-1);
    else return(0);    
    }


PRIVATE	int getcompareargs(int largc, char **largv, char *reppath, char *filepath)
{
    if (largc == 1) {
      Parser_getstr("Pathname of Object in conflict?", 
	       doRepDefault, reppath, MAXPATHLEN);
      *filepath = 0;
    } else {
      if (largc != 3) {
	printf("%s <object> <fixfile>\n", largv[0]);
	return(-1);
      }
      strncpy(reppath, largv[1], MAXPATHLEN);
      strncpy(filepath, largv[2], MAXPATHLEN);
    }
    while (*filepath && IsInCoda(filepath)) {
      if (*filepath)
	printf("Please use a fixfile not in /coda \n");
      Parser_getstr("Pathname of repair file produced?", 
	       (*compDirDefault == '\0') ? "" : compDirDefault, 
	       filepath, MAXPATHLEN);
    }

    strcpy(compDirDefault, filepath);
    return 0;
}

PRIVATE int getremoveargs(int largc, char **largv, char *uconfpath)
{
    if (largc == 1)
      Parser_getstr("Pathname of Object in conflict?", 
	     doRepDefault, uconfpath, MAXPATHLEN);
    else {
      if (largc != 2) {
	printf("%s <object>\n", largv[0]);
	return(-1);
      }
      strncpy(uconfpath, largv[1], MAXPATHLEN);
    }

    return(0);
}

/* woferry - this is not being used, but still uses strarg
PRIVATE	int getlistargs(char *args, char *reppath)
{
    char *p;
    
    p = args;
    *reppath = 0;
    while (*reppath == 0)
	strarg(&p, " ", "Pathname of Object in conflict?", 
	       doRepDefault, reppath);

    return 0;
}
*/

PRIVATE int doCompare(int nreplicas, struct repvol *repv, char **names, 
		      char *filepath, char *volmtpt, ViceFid *incfid)
{
    resreplica *dirs;
    struct  listhdr *k;
    int	i, j;
    FILE *file;
    struct rwvol *rwv;

    switch (session) {
    case LOCAL_GLOBAL:
	printf("\"doCompare\" can only be used for server/server conflict\n");
	return -1;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return -1;
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

    for (i = 0; i < nreplicas; i++){
	/* find the server name */
	{
	    rwv = repv->rwhead;
	    while (rwv && k[i].replicaId != rwv->vid)
		rwv = rwv->next;
	    assert(k[i].replicaId == rwv->vid);
	}
	fprintf(file,"\nreplica %s %x \n", rwv->srvname, k[i].replicaId);
	for (j = 0; j < k[i].repairCount; j++)
	    repair_printline(&(k[i].repairList[j]), file);
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
    if (compareAcl(nreplicas, dirs)){
	nConflicts ++;
	printf("Acls differ: Please repair manually using setacl <user> <rights>\n");
    }
    if (compareStatus(nreplicas, dirs)){
	nConflicts++;
	printf("Modebits differ - a repair should set the bits\n");
    }
    if (compareOwner(nreplicas, dirs)) {
	nConflicts++;
	printf("Owner differs: Please repair manually using setowner <uid>\n");
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

PRIVATE int compareStatus(int nreplicas, resreplica *dirs)
{
    int	i;
    for (i = 1; i < nreplicas; i++)
	if (dirs[i].modebits != dirs[0].modebits)
	    return -1;
    return 0;
}
PRIVATE int compareOwner(int nreplicas, resreplica *dirs) {
    int i;
    for (i = 1; i < nreplicas; i++)
	if (dirs[i].owner != dirs[0].owner)
	    return -1;
    return 0;
}
PRIVATE int compareQuotas(int nreplicas, char **names)
{
    if (nreplicas <= 1) {
	printf("Comparing Quotas: Not enough replicas to compare\n");
	return 0;
    }
    char piobuf[2048];    
    struct ViceIoctl vio;
    vio.in = 0;
    vio.in_size = 0;
    vio.out_size = 2048;
    vio.out = piobuf;

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
	vio.out_size = 2048;
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

PRIVATE void printAcl(struct Acl *acl)
{
    int i;
    
    printf("There are %d plus entries\n", acl->nplus);
    for (i = 0; i < acl->nplus; i++)
	printf("%s \t %d\n", ((acl->pluslist)[i]).name, ((acl->pluslist)[i]).rights);
    printf("There are %d negative entries\n", acl->nminus);
    for (i = 0; i < acl->nminus; i++)
	printf("%s \t %d\n", ((acl->minuslist)[i]).name, ((acl->minuslist)[i]).rights);
    printf("End of Access List\n");
}

PRIVATE int compareAcl(int nreplicas, resreplica *dirs)
{
    int	i, j;
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
PRIVATE int compareVV(int nreplicas, char **names, struct repvol *repv) 
{
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

PRIVATE void getremovelists(int nreplicas, resreplica *dirs, struct listhdr **repairlist)
{
    struct repair rep;
    resdir_entry *rde;
    int i, j;

    InitListHdr(nreplicas, dirs, repairlist);
    /* for each replica create the list of entries to be removed */
    for ( i = 0 ; i < nreplicas; i++) 
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

PRIVATE int makedff(char *extfile, char *intfile /* OUT */)
    /* extfile: external (ASCII) rep
       intfile: internal (binary) rep
       Returns 0 on success, -1 on failures
    */
    {
    struct listhdr *hl;
    int hlc, rc;

    DEBUG(("makedff(%s,...)\n", extfile));

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


    /* done! */
    return(0);
    }

PRIVATE void GetArgs(int argc, char *argv[])
    {
    int i;

    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-d") == 0)
	    {
	    repair_DebugFlag = 1;
	    continue;
	    }
	if (strcmp(argv[i], "-nsr") == 0)
	    {
		printf("Old-style repair is no longer supported\n");
		goto BadArgs;
	    }
	if (strcmp(argv[i], "-allowclear") == 0)
	    {
		allowclear = 1;
		continue;
	    }
        if (strcmp(argv[i], "-allowfilerepair") == 0) 
	    {
		printf("the -allowfilerepair flag is no longer supported\n");
		goto BadArgs;
	    }
	goto BadArgs;
	}
    return;

BadArgs:
    printf("Usage: repair [-d]\n");
    exit(-1);
    }


PRIVATE void SetDefaultPaths()
{
    char buf[MAXPATHLEN];
    char *repairrc = getenv("REPAIRRC");
    char *home;
    FILE *reprc;
    int ec;
    char arg1, arg2[MAXPATHLEN];  
    char *display;

    beginRepDefault[0] = '\0';
    doRepDefault[0] = '\0';

    if (repairrc == NULL){
	home = getenv("HOME");
	assert(home != NULL);
	strcpy(buf, home);
	strcat(buf, "/.repairrc");
	repairrc = buf;
    } 
    if (reprc = fopen(repairrc, "r")){
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
		printf("Unknown option %c in %s file\n", 
		       arg1, repairrc);
		exit(-1);
	    }
	}
	fclose(reprc);
    }
}

PRIVATE int GetReplicaNames(char **names, int maxnames, char
			     *ReplicatedName) {
    struct direct *de;
    struct stat buf;
    int i;
    DIR *d = opendir(ReplicatedName);
    if (!d) {
	printf("GetReplicaNames: coudln't open %s\n", ReplicatedName);
	return(-1);
    }
    int SizeRepName = (int) (strlen(ReplicatedName) + 1);
    i = 0; 
    de = readdir(d);
    for (; (i < maxnames) && (de != NULL); de = readdir(d)) {
	if ((!strcmp(de->d_name, ".")) || 
	    (!strcmp(de->d_name, "..")))
	    continue;

        /* 5 is for safety */
	names[i] = (char *)malloc(strlen(de->d_name) + SizeRepName + 5); 
	assert(names[i]);
	sprintf(names[i], "%s/%s" , ReplicatedName, de->d_name);
	if (stat(names[i], &buf)) {
	    perror(names[i]);
	    printf("replica %s not accessible, NOT USING FOR COMPARE\n", 
		   names[i]);
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

// return zero if user has valid tokens
PRIVATE int GetTokens() {
    ClearToken clear;
    EncryptedSecretToken secret;
    return (U_GetLocalTokens(&clear, secret));
}

/*
  BEGIN_HTML
  <a name="beginrepair"><strong> begin a new repair session </strong></a>
  END_HTML
*/
void beginRepair(int largc, char **largv)
    /* args:  <reppathname> */
    {
    VolumeId vid;
    int rc;
    char *p;
    VolumeId *tv;
    struct ViceIoctl vioc;
    struct repvol *repv;
    char space[2048]; /* XXX */
    char userpath[MAXPATHLEN], reppath[MAXPATHLEN];
    char prefix[MAXPATHLEN], suffix[MAXPATHLEN];

    switch (session) {
    case SERVER_SERVER:
	printf("there is already a server/server repair session going on\n");
	return;
    case LOCAL_GLOBAL:
	printf("there is already a local/global repair session going on\n");
	return;
    }

    if (largc == 1)
      Parser_getstr("Pathname of object in conflict?", beginRepDefault,
		    userpath, MAXPATHLEN);
    else {
      if (largc != 2) {
	printf("beginrepair <reppathname>\n");
	return;
      }
      strncpy(userpath, largv[1], MAXPATHLEN);
    }

    strcpy(beginRepDefault, userpath);
    strcpy(doRepDefault, userpath);
    rc = repair_isleftmost(userpath, reppath);
    if (rc < 0) return;

    rc = repair_getmnt(reppath, prefix, suffix, &vid);
    if (rc < 0) return;
    /* See if this volume is already being repaired */
    repair_findrep(vid, &repv);
    if (repv) return;

    /* Create a new rep vol entry */
    repair_newrep(vid, prefix, &repv);

    /* Obtain names of rw replicas */
    vioc.out_size = (short)sizeof(space);
    vioc.in_size = 0;
    vioc.out = space;
    bzero(space, (int)sizeof(space));
    rc = pioctl(prefix, VIOC_ENABLEREPAIR, &vioc, 0);
    if (rc < 0)
	{
	if (errno == EWOULDBLOCK)
	    {
	    printf("Repair in progress on volume at \"%s\"\n", prefix);
	    /* Print out Workstation/IP addr here --- Satya */
	    }
	else myperror(" ENABLEREPAIR", prefix, errno); /* some other error */
	repair_finish(repv);
	return;
	}
    sprintf(repv->rodir, "%s", reppath);

    /* Mount the rw replicas - or just insert into list of replicas */
    tv = (VolumeId *)space; /* output of ENABLEREPAIR pioctl */
    rc = repair_mountrw(repv, tv, MAXHOSTS);
    if (rc) {repair_finish(repv); return;}
    /* Link in new volume */
    repair_linkrep(repv);
    
    char cmd[10];
    sprintf(cmd, "%d 1", REP_CMD_BEGIN);
    vioc.in = cmd;
    vioc.in_size = (short) (strlen(cmd) + 1);
    vioc.out_size = (short)sizeof(space);
    vioc.out = space;
    rc = pioctl(reppath, VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_BEGIN)");
    }

    sscanf(vioc.out, "%d", &rc);
    switch (rc) {
    case 0: 
	printf("a local-global-conflict repair session started\n");
	printf("the conflict is cuased by a reintegration failure\n");
	printf("use the following commands to repair the conflict:\n");
	printf("\tchecklocal\n");
	printf("\tlistlocal\n");
	printf("\tpreservelocal\n");
	printf("\tpreservealllocal\n");
	printf("\tdiscardlocal\n");
	printf("\tdiscardalllocal\n");
	printf("\tsetglobalview\n");
	printf("\tsetmixedview\n");
	printf("\tsetlocalview\n");
	printf("a list of local mutations is availabe in the .cml file in the coda spool directory\n");
	session = LOCAL_GLOBAL;
	repv->local = 1;
	break;
    case 1: 
	printf("a local-global-conflict repair session already in progress\n");
	repv->local = 0;
	break;
    case 2:
	printf("a server-server-conflict repair session started\n");
	printf("use the following commands to repair the conflict:\n");
	printf("\tcomparedirs\n");
	printf("\tremoveinc\n");
	printf("\tdorepair\n");
	session = SERVER_SERVER;
	repv->local = 0;
	break;
    default:
	printf("bogus return code from venus (%d)\n", rc);
    }
    fflush(stdout);
    return;
}


/*
  BEGIN_HTML
  <a name="checklocal"><strong> check whether the current local  mutation operation being iterated by the current repair session </strong></a> 
  END_HTML
*/
void checkLocal(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[2048];

    switch (session) {
    case SERVER_SERVER:
	printf("\"checkLocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    vioc.in = buf;
    sprintf(buf, "%d", REP_CMD_CHECK);
    vioc.in_size = (short) strlen(vioc.in) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_CHECK)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="listlocal"><strong> list the local mutation operations associated with the object being repaired by the current session </strong></a>
  END_HTML
*/
void listLocal(int largc, char **largv)
    /* args: <no args> */
    {
    int fd;
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[2048];
    char filename[MAXPATHLEN];

    switch (session) {
    case SERVER_SERVER:
	printf("\"listLocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    strcpy(filename, "/tmp/listlocal.XXXXXX");
    mktemp(filename);
    vioc.in = buf;
    sprintf(buf, "%d %s", REP_CMD_LIST, filename);
    vioc.in_size = (short) strlen(vioc.in) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_LIST)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
    if (rc == 0) {
	fd = open(filename, O_RDONLY, 0);
	if (fd < 0) {
	    perror(filename);
	} else {
	    while (read(fd, buf, 2048) > 0)
	      write(1, buf, strlen(buf));
	    close(fd);
	}	
    }
    unlink(filename);
}

/*
  BEGIN_HTML
  <a name="preservelocal"><strong> preserve the effect of the current mutation operation being repaired </strong></a>
  END_HTML
*/
void preserveLocal(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];
    

    switch (session) {
    case SERVER_SERVER:
	printf("\"preservelocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_PRESERVE);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMU_PRESERVE)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="preservealllocal"><strong> preserve the effect of all the mutation operations associated with the object currently being repaired </strong></a>
  END_HTML
*/
void preserveAllLocal(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];
    

    switch (session) {
    case SERVER_SERVER:
	printf("\"preservealllocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    /* Release volume-level locks */
    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_PRESERVE_ALL);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMU_PRESERVE_ALL)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="discardlocal"><strong> discard the effect of the current
  mutation operation being repaired </strong></a>
END_HTML
*/
void discardLocal(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];
    
    switch (session) {
    case SERVER_SERVER:
	printf("\"discardlocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_DISCARD);
    vioc.in = buf;    
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_DISCARD)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="discardalllocal"><strong> discard all the mutation operations associated with the object currently being repaired </strong></a>
  END_HTML
*/
void discardAllLocal(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];
    
    switch (session) {
    case SERVER_SERVER:
	printf("\"discardalllocal\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_DISCARD_ALL);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_DISCARD_ALL)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="setlocalview"><strong> set the local object view for the object being repaired so that only its local client state is visible </strong></a> 
  END_HTML
*/
void setLocalView(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];

    switch (session) {
    case SERVER_SERVER:
	printf("\"setlocalview\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_LOCAL_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_LOCAL_VIEW)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="setglobalview"><strong> set the global object view for the 
  object being repaired so that only its global server state is visible </strong></a>
  END_HTML
*/
void setGlobalView(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];

    switch (session) {
    case SERVER_SERVER:
	printf("\"setglobalview\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }
    
    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_GLOBAL_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_GLOBAL_VIEW)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

/*
  BEGIN_HTML
  <a name="setmixedview"><strong> set the mixed object view for the
  object being repaired so that both its local client state and its
  global server state are visible </strong></a>
  END_HTML
*/
void setMixedView(int largc, char **largv)
    /* args: <no args> */
    {
    struct ViceIoctl vioc;
    int rc;
    struct repvol *repv;
    char space[2048];
    char buf[BUFSIZ];

    switch (session) {
    case SERVER_SERVER:
	printf("\"setmixedview\" can only be used to repair a local/global conflict\n");
	return;
    case NOT_IN_SESSION:
	printf("You must do \"beginrepair\" first\n");
	return;
    }

    vioc.out = space;
    vioc.out_size = 2048;
    sprintf(buf, "%d", REP_CMD_MIXED_VIEW);
    vioc.in = buf;
    vioc.in_size = (short) strlen(buf) + 1;

    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) {
	perror("VIOC_REP_CMD(REP_CMD_MIXED_VIEW)");
    }
    printf("%s\n", vioc.out);
    fflush(stdout);
}

PRIVATE void INT(int, int, struct sigcontext *) {
    /* force an end to the current repair session when ^C is hit */
    printf("abnormal exit of repair tool\n");
    fflush(stdout);
    quit(0, (char **)0);
}
