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

static char *rcsid = "$Header: ruletypes.cc,v 4.1 97/01/08 21:49:23 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <libc.h>
#ifdef	__linux__
#include <unistd.h>
#endif 
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/wait.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <venusioctl.h>
#include <vcrcommon.h>
#include "asr.h" 

extern "C" void path(char *, char *, char *);
extern int wildmat(char *text, char *pattern);

#ifdef __cplusplus
}
#endif __cplusplus

//C++ include files 
#include <olist.h>
#include <inconsist.h>
#include "ruletypes.h"

objname_t::objname_t(char *name) {
    path(name, dname, fname);
}

objname_t::~objname_t() {
    
}
int objname_t::match(char *idname, char *ifname) {
    // fname and incfname should match 
    // dirname should match or the rule shouldn't have a dir path
    if (wildmat(ifname, fname) && 
	((!strcmp(dname, ".")) || 
	 (!strcmp(dname, idname)))) {
	return(1);
    }
    return(0);
}

// Get the prefix in name that matches the * (if it exists) in the object name 
void objname_t::GetPrefix(char *name, char *prefix) {
    *prefix = '\0';
    if (fname[0] != '*') return;

    char tmpprefix[MAXPATHLEN];
    char tmpfname[MAXPATHLEN];
    strcpy(tmpprefix, name);
    strcpy(tmpfname, &fname[1]);

    // add a special character to find the suffix correctly
    tmpprefix[strlen(tmpprefix) + 1] = '\0';
    tmpprefix[strlen(tmpprefix)] = '$';
    tmpfname[strlen(tmpfname) + 1] = '\0';
    tmpfname[strlen(tmpfname)] = '$';

    // find the common point 
    char *c = strstr(tmpprefix, tmpfname);
    if (c == NULL) {
	DEBUG((stderr, "Wildcard matched but couldn't find common prefix (%s %s)\n",   
	       name, fname));
	exit(-1);
    }
    *c = '\0';
    DEBUG((stdout, "Common suffix is %s\n",  c + 1));
    strcpy(prefix, tmpprefix);
}

void objname_t::print() {
    print(stdout);
}
void objname_t::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void objname_t::print(int fd) {
    char buf[MAXPATHLEN + MAXNAMLEN];
    sprintf(buf, "%s%c%s ", dname, 
	    (strlen(dname)> 0) ? '/' : ' ', 
	    fname);
    write(fd, buf, (int)strlen(buf));
}

depname_t::depname_t(char *name) {
    path(name, dname, fname);
    fid.Volume = fid.Vnode = fid.Unique = 0;
}

depname_t::~depname_t() {
    
}
void depname_t::print() {
    print(stdout);
}
void depname_t::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void depname_t::print(int fd) {
    char buf[MAXPATHLEN + MAXNAMLEN];
    sprintf(buf, "%s%c%s", dname, 
	    (strlen(dname)> 0) ? '/' : ' ', 
	    fname);
    write(fd, buf, (int)strlen(buf));
    sprintf(buf, "(0x%x.%x.%x) ", fid.Volume, fid.Vnode, fid.Unique);
    write(fd, buf, (int) strlen(buf));
}

command_t::command_t(char *name) {
    char *c;
    if (c = rindex(name, '/')) {
	*c = '\0';
	strcpy(cmddname, name);
	strcpy(cmdfname, c + 1);
    }
    else {
	cmddname[0] = '\0';
	strcpy(cmdfname, name);
    }
    fid.Volume = fid.Vnode = fid.Unique = 0;
    argc = 0;
    arglist = NULL;
}

command_t::~command_t() {
    if (arglist) {
	for (int i = 0; i < argc; i++) 
	    free(arglist[i]);
	free(arglist);
    }
}

arg_t *command_t::addarg(char *argname) {
    argc++;
    arg_t **newargl = (arg_t **)malloc(argc * sizeof(arg_t *));
    for (int i = 0; i < argc - 1; i++) 
	newargl[i] = arglist[i];
    newargl[argc - 1] = new arg_t(argname);
    
    if (arglist) free(arglist);
    arglist = newargl;
    DEBUG((stdout, "Debug: Added arg %s\n", argname));
    return(newargl[argc - 1]);
}

void command_t::addreplicaid(char *c) {
    arglist[argc-1]->addreplicaid(c);
}

// expands $*, $<, $>
void command_t::expandname(char *p, char *incdirname, char *incfname) {
    for (int i = 0; i < argc; i++) 
	arglist[i]->expandname(p, incdirname, incfname);
}

// expand $#, [i] and [all]
void command_t::expandreplicas(int n, char **repnames) {
    // first count the number of args finally after expanding the [all] case
    int countargs = argc;
    for (int i = 0; i < argc; i++) 
	if (arglist[i]->expandall()) 
	    countargs += (n - 1);

    // expand the [all] case
    if (countargs != argc) {
	arg_t **newarglist = (arg_t **)malloc(countargs * sizeof(arg_t *));
	int index = 0;

	for (i = 0; i < argc; i++) {
	    if (arglist[i]->expandall()) {
		for (int j = 0; j < n; j++) {
		    char buf[MAXPATHLEN];
		    arglist[i]->appendname(buf, repnames[j]);
		    newarglist[index] = new arg_t(buf);
		    index++;
		}
		delete arglist[i];
		arglist[i] = NULL;
	    }
	    else {
		newarglist[index] = arglist[i];
		arglist[i] = NULL;
		index++;
	    }
	}
	assert(index == countargs);
	argc = countargs;
	free(arglist);
	arglist = newarglist;
    }

    // expand the [i] and $#
    for (i = 0; i < argc; i++) 
	arglist[i]->expandreplicas(n, repnames);

}

int command_t::execute() {
    char name[MAXPATHLEN];
    sprintf(name, "%s/%s", cmddname, cmdfname);
    char **argv = (char **)malloc((argc + 2) * sizeof(char *));
    argv[0] = name;
    
    for (int i = 0, index = 1; i < argc; i++, index++) 
	argv[index] = &(arglist[i]->name[0]);
    argv[index] = NULL;
    
    int rc = fork();
    if (rc == -1) {
	fprintf(stderr, "Error during fork for command %s\n", 
		name);
	return(-1);
    }
    else if (rc) {
	// parent process 
	union wait cstatus;
	int cpid = rc;
#ifdef	__NetBSD__
	for (rc = wait(&cstatus.w_status); (rc != -1) && (rc != cpid); rc = wait(&cstatus.w_status))
#else
	for (rc = wait(&cstatus); (rc != -1) && (rc != cpid); rc = wait(&cstatus))
#endif
	    fprintf(stderr, "Waiting for %d to finish ...\n", cpid);
	if (cstatus.w_coredump) {
	    fprintf(stderr, "%s dumped core\n", name);
	    return(-1);
	}
	return(cstatus.w_retcode);
    }
    else {
	// child process 
	DEBUG((stdout, "Going to execute command %s ", name));
	if (debug) {
	    for (int i = 0; i < argc; i++) 
		DEBUG((stdout, "%s ", argv[i + 1]));
	    DEBUG((stdout, "\n"));
	}
	if (execv(name, argv)) {
	    fprintf(stderr, "error during execing \n");
	    return(-1);
	}
    }
}
void command_t::print() {
    print(stdout);
}

void command_t::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void command_t::print(int fd) {
    char buf[MAXPATHLEN + MAXNAMLEN];	//XXX arg length should be < 1280
    sprintf(buf, "%s%c%s ", cmddname, 
	    (strlen(cmddname) > 0) ? '/' : ' ',
	    cmdfname);
    write(fd, buf, (int) strlen(buf));
    
    for (int i = 0; i < argc; i++) 
	arglist[i]->print(fd);
    
    sprintf(buf, "\n");
    write(fd, buf, (int) strlen(buf));
}

rule_t::rule_t() {
    DEBUG((stdout, "Initializing rule\n"));
    prefix[0] = '\0';
    for (int i = 0; i < VSG_MEMBERS; i++) 
	repnames[i] = NULL;
    nreplicas = 0;
    incfid.Volume = incfid.Vnode = incfid.Unique = 0;
    idname[0] = '\0';
    ifname[0] = '\0';
}

rule_t::~rule_t() {
    objname_t *o;
    depname_t *d;
    command_t *c;
    
    DEBUG((stdout, "Rule being destroyed\n"));
    while (o = (objname_t *)objlist.get()) {
	DEBUG((stdout, "Destroying object..."));
	delete o;
    }
    DEBUG((stdout, "\nDeleted object list\n"));
    
    while (d = (depname_t *)deplist.get()) {
	DEBUG((stdout, "Destroying dependency..."));
	delete d;
    }
    DEBUG((stdout, "\nDeleted dependency list \n"));
    
    while (c = (command_t *)cmdlist.get()) {
	DEBUG((stdout, "Destroying command..."));
	delete c;
    }
    DEBUG((stdout, "\nDeleted command list\n"));
    for (int i = 0; i < VSG_MEMBERS; i++) 
	if (repnames[i]) free(repnames[i]);
}

void rule_t::addobject(char *name) {
    objname_t *oname = new objname_t(name);
    objlist.insert(oname);
}

void rule_t::adddep(char *name) {
    depname_t	*dname = new depname_t(name);
    deplist.insert(dname);
}

void rule_t::addcmd(command_t *cmd) {
    cmdlist.append(cmd);
}

int rule_t::match(char *dname, char *fname) {
    olist_iterator next(objlist);
    objname_t *o;
    while (o = (objname_t *)next()) {
	if (o->match(dname, fname)) {
	    DEBUG((stdout, "Found matching object\n"));
	    o->GetPrefix(fname, prefix);
	    GetRepInfo(dname, fname);
	    if (debug) o->print();
	    return(1);
	}
    }
    return(0);
}

int rule_t::GetReplicaNames() {
    // we should have a pioctl for venus that returns the names of the children
    // for now we get them by doing a begin repair and looking at the children
    struct ViceIoctl vioc;
    char space[2048];
    int rc;

    // expose the replicas 
    char name[MAXPATHLEN];
    char *namep = &name[0];
    sprintf(name, "%s/%s", idname, ifname);

    vioc.out_size = (short) sizeof(space);
    vioc.in_size = 0;
    vioc.out = space;
    bzero(space, (int) sizeof(space));
    rc = pioctl(namep, VIOC_ENABLEREPAIR, &vioc, 0);
    if (rc < 0) {
	fprintf(stderr, "Error %d(%d) trying to get replicas of %s\n", 
		rc, errno, name);
	return(rc);
    }
    else {
	// get the replicas 
	DIR *dirp = opendir(name);
	if (dirp) {
	    struct direct *dp;
	    for (dp = readdir(dirp); 
		 ((dp != NULL) && (nreplicas < VSG_MEMBERS)); 
		 dp = readdir(dirp)) {
		if ((!strcmp(dp->d_name, ".")) || (!strcmp(dp->d_name, ".."))) 
		    continue;
		else { 
		    repnames[nreplicas] = (char *)malloc(strlen(dp->d_name) + 1);
		    strcpy(repnames[nreplicas], dp->d_name);
		    nreplicas++;
		}
	    }
	    closedir(dirp);
	}
	else 
	    DEBUG((stdout, "Couldn't open directory %s\n", name));
	pioctl(name, VIOC_DISABLEREPAIR, &vioc, 0);
    }
    return(0);
}

int rule_t::enablerepair() {
    char space[2048];
    struct ViceIoctl vioc;
    char name[MAXPATHLEN];

    sprintf(name, "%s/%s", idname, ifname);
    vioc.out_size = (short) sizeof(space);
    vioc.in_size = 0;
    vioc.out = space;
    bzero(space, (int) sizeof(space));
    int rc = pioctl(name, VIOC_ENABLEREPAIR, &vioc, 0);
    if (rc < 0) 
	fprintf(stderr, "Error %d(%d) trying to enable repair for %s\n", 
		rc, errno, name);
    return(rc);
}

void rule_t::disablerepair() {
    struct ViceIoctl vioc;
    int rc;
    char name[MAXPATHLEN]; 
    
    sprintf(name, "%s", idname);
    bzero(&vioc, (int) sizeof(vioc));
    rc = pioctl(name, VIOC_DISABLEREPAIR, &vioc, 0);
    if (rc < 0) 
	fprintf(stderr, "Error(%d) during disablerepair of %s", errno, name);
}

void rule_t::GetRepInfo(char *dname, char *fname) {

    strcpy(idname, dname);
    strcpy(ifname, fname);
    char name[MAXPATHLEN];

    // make sure object is inconsistent
    {
	int rc;
	char symval[MAXPATHLEN];
	struct stat statbuf;
	sprintf(name, "%s/%s", idname, ifname);
	rc = stat(name, &statbuf);
	if ((rc == 0) || (errno != ENOENT)) return;
	
	// is it a sym link
	symval[0] = 0;
	rc = readlink(name, symval, MAXPATHLEN);
	if (rc < 0) return;
	
	// it's a sym link, alright 
	if (symval[0] == '@') {
	    sscanf(symval, "@%x.%x.%x",
		   &incfid.Volume, &incfid.Vnode, &incfid.Unique);
	}
    }
    
    // Get replica names
    GetReplicaNames();
}

// expand all the macros ($*, $<, $>, [], $#
void rule_t::expand() {
    olist_iterator next(cmdlist);
    command_t *c;
    while (c = (command_t *) next()) {
	c->expandname(prefix,idname, ifname);	// expands the $*, $> and $<
	c->expandreplicas(nreplicas, repnames);	// expands the [] and $#
    }
}

int rule_t::execute() {
    int rc = 0;
    olist_iterator next(cmdlist);
    command_t *c;
    while (c = (command_t *) next()) 
	if (rc = c->execute()) {
	    fprintf(stderr, "The following command failed - discontinuing\n");
	    c->print(stderr);
	    return(rc);
	}
    DEBUG((stdout, "All commands succeeded\n"));
    return(0);
}
void rule_t::print() {
    print(stdout);
}

void rule_t::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}

void rule_t::print(int fd) {
    // print object names 
    olist_iterator onext(objlist);
    objname_t *o;
    while (o = (objname_t *)onext()) 
	o->print(fd);
    
    char buf[2 * MAXPATHLEN];
    sprintf(buf, ":");
    write(fd, buf, (int) strlen(buf));
    
    // print dependency names
    olist_iterator dnext(deplist);
    depname_t *d;
    while (d = (depname_t *)dnext()) 
	d->print(fd);
    
    sprintf(buf, "\n");
    write(fd, buf, (int) strlen(buf));
    
    // print commands
    olist_iterator cnext(cmdlist);
    command_t *c;
    while (c = (command_t *)cnext()) 
	c->print();
    
    sprintf(buf, "Prefix is %s\n\n\n", prefix);
    write (fd, buf, (int) strlen(buf));
    
    if (incfid.Volume) {
	sprintf(buf, "Inc object is %s/%s (0x%x.%x.%x) with %d replicas\n",
		idname, ifname, incfid.Volume, incfid.Vnode, incfid.Unique, nreplicas);
	write(fd, buf, (int) strlen(buf));
	sprintf(buf, "Replica names are %s %s %s %s %s %s %s %s \n",
		repnames[0], repnames[1], repnames[2], repnames[3], 
		repnames[4], repnames[5], repnames[6], repnames[7]);
	write(fd, buf, (int) strlen(buf));
    }
}

arg_t::arg_t(char *c) {
    assert((strlen(c) < MAXPATHLEN));
    strcpy(name, c);
    replicaid = NOREPLICAID;
}

arg_t::~arg_t() {
    
}

void arg_t::addreplicaid(char *c) {
    if (!strcmp(c, "*"))
	replicaid = ALLREPLICAS;
    else {
	replicaid = atoi(c);
	if (replicaid >= ALLREPLICAS) {
	    fprintf(stderr, "Error - replicaid must be < 8\n");
	    replicaid = NOREPLICAID;
	}
    }
}

void arg_t::expandname(char *p, char *incdirname, char *incfname) {
    expandstring(name, "$*", p);
    expandstring(name, "$<", incdirname);
    expandstring(name, "$>", incfname);
}

// expand the [i] and $# in the replica names 
void arg_t::expandreplicas(int n, char **replicanames) {
    DEBUG((stdout, "DEBUG: expanding replicas names for arg_t (0x%x)\n", this));
    char buf[4];
    sprintf(buf, "%d", n);
    expandstring(name, "$#", buf);
    if (!n || !replicanames) return;
    if ((replicaid != -1) && (replicaid != ALLREPLICAS) 
	&& (replicanames[replicaid])) {
	// concat the name of the [i]th replica 
	strcat(name, "/");
	strcat(name, replicanames[replicaid]);
	replicaid = -1;
	DEBUG((stdout,  "Setting replicaid of arg_t(0x%x) to -1\n", this));
    }
}
int arg_t::expandall() {
    return((replicaid == ALLREPLICAS) ? 1 : 0);
}

// append "/c" to the name and put it in buf
void arg_t::appendname(char *buf, char *c) {
    sprintf(buf, "%s/%s", name, c);
}

void arg_t::print() {
    print(stdout);
}
void arg_t::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}
void arg_t::print(int fd) {
    char buf[MAXPATHLEN];
    
    if (replicaid == ALLREPLICAS)
	sprintf(buf, "%s[all] ", name);
    else if (replicaid == NOREPLICAID)
	sprintf(buf, "%s ", name);
    else 
	sprintf(buf, "%s[%d] ", name, replicaid);
    
    write(fd, buf, (int) strlen(buf));
}

// In string s, replace all instances of pattern by newpattern
// assume final string fits in MAXPATHLEN and length(s) = MAXPATHLEN
void expandstring(char *s, char *pattern, char *newpattern) {
    char buff[MAXPATHLEN];
    char *tmps, *buffp;
    int patternlength = (int) strlen(pattern);
    int newpatternlength = (int) strlen(newpattern);

    if (patternlength == 0) return;
    int length = (int) strlen(s);
    char *end = s + length;
    tmps = s;
    buffp = &buff[0];
    while (tmps < end) {
	char *c = strstr(tmps, pattern);
	if (c) {
	    strncpy(buffp, tmps, c - tmps);
	    buffp += c - tmps;
	    tmps = c + patternlength;
	    strcpy(buffp, newpattern);
	    buffp += newpatternlength;
	}
	else {
	    strcpy(buffp, tmps);
	    break;
	}
    }
    strcpy(s, buff);
}
