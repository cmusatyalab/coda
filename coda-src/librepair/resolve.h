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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/librepair/resolve.h,v 4.1 1997/01/08 21:50:09 rvb Exp $";
#endif /*_BLURB_*/







/*
 * resolve.h 
 * Created 09/18/89  Puneet Kumar
 */

/* Header file for definitions used by the 
 * resolution subsystem  
 */

#define MAXNAMELEN 255
#define	AVGDIRENTRYSIZE 12
#define	GROWSIZE    32
#define MAXHOSTS    8
#define	ISDIR(vnode) ((vnode) &	1)  /* directory vnodes are odd */
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

/* definition of each directory entry in memory as used 
 * by the resolution subsystem */
typedef struct {
    char    name[MAXNAMELEN + 1];   /* name of the entry */
    long    vno;		    /* vnode number */
    long    uniqfier;		    /* for vice dirs; 0 for unix */
    ViceVersionVector	VV;	    
    int	    MtPt;		    /* Is this child a mount point? */
    int	    replicaid;
    int	    lookedAt;
} resdir_entry;

/* definition of the parent directory replica */
typedef struct {
    int	    entry1;		    /* index of first child in table */
    int	    nentries;		    /* number of children */
    long    replicaid;		    /* id of this replica */
    long    vnode;		    /* fid of the parent directory */
    long    uniqfier;
    char    *path;		    /* path name of the RO mounted copy */
    u_short modebits;		    
    struct  Acl *al;
    short   owner;
}resreplica;

/* globals */
extern resdir_entry	*direntriesarr;
extern int direntriesarrsize;
extern int nextavailindex;
extern resdir_entry	**sortedArrByFidName;	/* for sorting the direntries in fid order*/
extern resdir_entry	**sortedArrByName;	/* for sorting the direntries in name order */
extern int totaldirentries;
extern VolumeId RepVolume;
extern int  nConflicts;


extern void InitListHdr C_ARGS((int , resreplica *, struct listhdr **));
extern int InsertListHdr C_ARGS((struct repair *, struct listhdr **, int ));
extern int InRepairList C_ARGS((struct listhdr *, unsigned , long , long ));
extern int getunixdirreps C_ARGS((int , char **, resreplica **));
extern int dirresolve C_ARGS((int , resreplica *, int (*)(char *), struct listhdr **, char *));
extern void resClean C_ARGS((int, resreplica *, struct listhdr *));
extern int GetParent C_ARGS((ViceFid *, ViceFid *, char *, char *, char *));
