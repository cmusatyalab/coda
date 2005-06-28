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

#ifndef _RESOLVE_H_
#define _RESOLVE_H_

/*
 * resolve.h 
 * Created 09/18/89  Puneet Kumar
 */

/* Header file for definitions used by the 
 * resolution subsystem  
 */
#include <vcrcommon.h>

#ifdef MAXNAMELEN
#undef MAXNAMELEN
#endif
#define MAXNAMELEN 255
#define	AVGDIRENTRYSIZE 12
#define	GROWSIZE    32
#define MAXHOSTS    8
#define	ISDIRVNODE(vnode) ((vnode) &	1)  /* directory vnodes are odd */
#define NNCONFLICTS -1

struct Acl {
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    char name[MAXNAMELEN];
    long rights;
};

struct repinfo {
    char *user, *rights;      /* ACL sets   (what to set ACL's to if conflicting) */
    char *owner;              /* Owner sets (what to set owner to...) */
    char *mode;               /* Mode sets */
    char *fixed;              /* Location of 'fixed' file/directory if non-interactive */
    char interactive;         /* flag indicating whether repair is interactive */
};

/* definition of each directory entry in memory as used 
 * by the resolution subsystem */
typedef struct {
    char    name[MAXNAMELEN + 1];   /* name of the entry */
    ViceFid fid;
    ViceVersionVector	VV;
    int	    MtPt;		    /* Is this child a mount point? */
    int	    replicaid;              /* XXX: do we need this? -Adam */
    int	    lookedAt;
} resdir_entry;

/* definition of the parent directory replica */
typedef struct {
    int	    entry1;		    /* index of first child in table */
    int	    nentries;		    /* number of children */
    ViceFid fid;
  //    long    replicaid;		    /* volume id of this replica */
  //    VnodeId  vnode;		    /* fid of the parent directory */
  //    Unique_t uniqfier;
    char    *path;		    /* path name of the RO mounted copy */
    u_short modebits;
    struct  Acl *al;
    short   owner;
} resreplica;

/* globals */
extern resdir_entry	*direntriesarr;
extern int direntriesarrsize;
extern int nextavailindex;
extern resdir_entry	**sortedArrByFidName;	/* for sorting the direntries in fid order*/
extern resdir_entry	**sortedArrByName;	/* for sorting the direntries in name order */
extern int totaldirentries;
extern int  nConflicts;


extern void InitListHdr (int , resreplica *, struct listhdr **);
extern int InsertListHdr (struct repair *, struct listhdr **, int );
extern int InRepairList (struct listhdr *, unsigned, VnodeId, Unique_t);
extern int IsCreatedEarlier(struct listhdr **, int, VnodeId, Unique_t);
extern int getunixdirreps (int , char **, resreplica **);
extern int dirresolve (int, resreplica *, int (*)(char *), struct listhdr **,
		       VolumeId, struct repinfo *, char *realm);
extern void resClean (int, resreplica *, struct listhdr *);
extern int GetParent (char *realm, ViceFid *, ViceFid *, char *, char *);

#endif
