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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/asr/ruletypes.h,v 4.2 1998/01/10 18:36:56 braam Exp $";
#endif /*_BLURB_*/





#ifndef _RULETYPES_H_
#define _RULETYPES_H_
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef	__linux__
#include <stdlib.h>        
#else
#include <libc.h>        
#endif
#include <sys/param.h>
#include <unistd.h>
#ifdef __BSD44__
#include <sys/dir.h>
#endif
#include <strings.h>
#include <vcrcommon.h>

#ifdef __cplusplus
}
#endif __cplusplus

//C++ include files 
#include <olist.h>
#include <inconsist.h>

class objname_t : public olink {
    char	dname[CFS_MAXPATHLEN];	/* dir name cannot have any * in it */
    char 	fname[CFS_MAXNAMLEN];	/* file name can have *s */
  public:
    objname_t(char *);
    ~objname_t();
    int match(char *, char*);
    void GetPrefix(char *name, char *prefix);
    void print();
    void print(FILE *);
    void print(int);
};

class  depname_t : public olink {
    char 	dname[CFS_MAXPATHLEN];
    char	fname[CFS_MAXNAMLEN];
    ViceFid 	fid; 			/* initialized only by application */
  public:
    depname_t(char *);
    ~depname_t();
    void print();
    void print(FILE *);
    void print(int);
};

#define NOREPLICAID	-1
#define ALLREPLICAS	9
class arg_t {
friend class command_t;
    char name[CFS_MAXPATHLEN];
    int	replicaid;
  public:
    arg_t(char *);
    ~arg_t();
    void addreplicaid(char *);
    void expandname(char *, char *, char *);
    int expandall();			// returns true if [all] was in the arg
    void appendname(char *, char *);
    void expandreplicas(int, char **);
    void print();
    void print(FILE *);
    void print(int);
};

class command_t : public olink {
    char	cmddname[CFS_MAXPATHLEN];
    char	cmdfname[CFS_MAXNAMLEN];
    ViceFid	fid;
    int 	argc;
    arg_t	**arglist;
  public:
    command_t(char *);
    ~command_t();
    arg_t *addarg(char *);
    void addreplicaid(char *);
    void expandname(char *, char *dirname, char *fname);
    void expandreplicas(int, char **replicanames);
    int execute();
    void print();
    void print(FILE *);
    void print(int);
};

class rule_t : public olink {
    olist 	objlist;
    olist	deplist;
    olist	cmdlist;
    // the following are filled only after this rule is matched for an inc object
    char	prefix[CFS_MAXNAMLEN];	// common prefix from obj name (eg. *.c)
    char	*repnames[VSG_MEMBERS];	// canonical ordering of rep names
    int		nreplicas;		// number of replicas of the inc object
    ViceFid	incfid;			// fid of inc object causing the asr invocation
    char 	idname[CFS_MAXPATHLEN];	// name of inc object 
    char 	ifname[CFS_MAXNAMLEN];
    int GetReplicaNames();		// gets name of individual replicas
    //int GetIFid();			// gets fid of inc. object 
  public:
    rule_t();
    ~rule_t();
    void addobject(char *);
    void adddep(char *);
    void addcmd(command_t *);
    int match(char *, char *);
    void GetRepInfo(char *, char *);
    void expand();
    int enablerepair();
    int execute();
    void disablerepair();
    void print();
    void print(FILE *);
    void print(int);
};

// exportable routines 
extern void expandstring(char *, char *, char *);
#endif _RULETYPES_H_
