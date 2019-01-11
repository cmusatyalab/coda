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

#ifndef _RULETYPES_H_
#define _RULETYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include "coda_string.h"
#include <vcrcommon.h>

#ifdef __cplusplus
}
#endif

//C++ include files
#include <olist.h>
#include <inconsist.h>

class objname_t : public olink {
    char dname[CODA_MAXPATHLEN]; /* dir name cannot have any * in it */
    char fname[CODA_MAXNAMLEN]; /* file name can have *s */
public:
    objname_t(char *);
    ~objname_t();
    int match(char *, char *);
    void GetPrefix(char *name, char *prefix);
    void print();
    void print(FILE *);
    void print(int);
};

class depname_t : public olink {
    char dname[CODA_MAXPATHLEN];
    char fname[CODA_MAXNAMLEN];
    ViceFid fid; /* initialized only by application */
public:
    depname_t(char *);
    ~depname_t();
    void print();
    void print(FILE *);
    void print(int);
};

#define NOREPLICAID -1
#define ALLREPLICAS 9
class arg_t {
    friend class command_t;
    char name[CODA_MAXPATHLEN];
    int replicaid;

public:
    arg_t(char *);
    ~arg_t();
    void addreplicaid(char *);
    void expandname(char *, char *, char *);
    int expandall(); // returns true if [all] was in the arg
    void appendname(char *, char *);
    void expandreplicas(int, char **);
    void print();
    void print(FILE *);
    void print(int);
};

class command_t : public olink {
    char cmddname[CODA_MAXPATHLEN];
    char cmdfname[CODA_MAXNAMLEN];
    ViceFid fid;
    int argc;
    arg_t **arglist;

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
    olist objlist;
    olist deplist;
    olist cmdlist;
    // the following are filled only after this rule is matched for an inc object
    char prefix[CODA_MAXNAMLEN]; // common prefix from obj name (eg. *.c)
    char *repnames[VSG_MEMBERS]; // canonical ordering of rep names
    int nreplicas; // number of replicas of the inc object
    ViceFid incfid; // fid of inc object causing the asr invocation
    char increalm[MAXHOSTNAMELEN]; // realm of inc object
    char idname[CODA_MAXPATHLEN]; // name of inc object
    char ifname[CODA_MAXNAMLEN];
    int GetReplicaNames(); // gets name of individual replicas
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
extern void expandstring(char *, const char *, char *);
#endif /* _RULETYPES_H_ */
