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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/simops.h,v 4.2 1997/10/23 19:24:36 braam Exp $";
#endif /*_BLURB_*/







#ifndef _SIM_OPS_H_
#define _SIM_OPS_H_ 1
#include <voltypes.h>
typedef struct
{
	VolumeId	Volume;
	VnodeId		Vnode;
	Unique		Unique;
}	ViceFid;

typedef struct
{
	unsigned long	Host;
	unsigned long	Uniquifier;
}	ViceStoreId;

typedef struct
{
    int	Site0;
    int	Site1;
    int	Site2;
    int	Site3;
    int	Site4;
    int	Site5;
    int	Site6;
    int	Site7;
} ViceVersionArray;

typedef struct
{
    ViceVersionArray	Versions;
    ViceStoreId	StoreId;
    long Flags;
} ViceVersionVector;

struct common_log {
    unsigned long	serverid;
    ViceStoreId 	storeid;
    unsigned int  	opcode;
    VnodeId		dvnode;
    Unique		dunique;
    long 		nextptr;
    long 		prevptr;
};

struct rm_log {
    struct common_log cl;
    char 		*name;
    VnodeId 		cvnode;
    Unique 		cunique;
    ViceVersionVector 	cvv;
};

struct rmdir_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique 		cunique;
    int 		head;	/* head of log for child being deleted */
    int			count;	/* number of log entries for child */
    ViceStoreId 	childLCP; /* childs lcp in log with other reps */
    ViceStoreId		csid;	/* storeid of directory when deleted */
};

struct create_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct mkdir_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct store_log {
    struct common_log	cl;
};

struct rename_log {
    struct common_log	cl;
    unsigned long srctgt;
    struct {
	char 	*oldname;
	VnodeId	cvnode;
	Unique	cunique;
	ViceVersionVector cvv;
    } rename_src;
    VnodeId OtherDirV;
    Unique OtherDirU;
    struct {
	char	*newname;
	int 	tgtexisted;
	VnodeId	TgtVnode;	/* vnode of target obj if it existed */
	Unique	TgtUnique;	/* uniquifier of target obj */
	union {
	    ViceVersionVector TgtGhostVV;
	    struct {
		int	head;
		int 	count;
	    } TgtGhostLog;
	} TgtGhost;
    } rename_tgt;
};

struct symlink_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

struct hardlink_log {
    struct common_log	cl;
    char		*name;
    VnodeId 		cvnode;
    Unique		cunique;
};

#endif not _SIM_OPS_H_
