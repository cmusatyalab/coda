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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/vol/RCS/vlist.h,v 4.1 1997/01/08 21:52:19 rvb Exp $";
#endif /*_BLURB_*/






/*
 * Vlist.h -- Created October 1989
 * Author Puneet Kumar
 */


#ifndef _VICE_VLIST_H_
#define _VICE_VLIST_H_ 1

struct vle;

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <dlist.h>
#include <vice.h>
#include "rvmdir.h"
#include "cvnode.h"

extern int VLECmp(vle *, vle *);
extern vle *FindVLE(dlist&, ViceFid *);
extern vle *AddVLE(dlist&, ViceFid *);


/* The data structure we want here is a binary search tree, but we use a list instead. -JJK */
struct vle : public dlink {
    ViceFid fid;
    Vnode *vptr;
    olist sl;			    /* list of spooled log records - for res logs in vm only */
    olist rsl;			    /* list of spooled log records - for res logs in rvm */
    union {
	struct {
	    ViceStoreId	sid;	    /* sid of LAST data store (used to avoid multiple bulk transfers) */
	    Inode sinode;	    /* inode to dec on success */
	    Inode finode;	    /* inode to dec on failure */
	    Inode tinode;	    /* inode to trunc on success */
	    unsigned tlength;	    /* length to trunc t_inode to (on success) */
	} file;
	struct {
	    DirInode *cinode;	    /* cloned inode (in RVM)  */
	    int needsres;  	    /* does directory need to be resolved at end of reintegration? */
	    int purgelog;  	    /* should directory log be purged */
	    int trunclog;  	    /* should log be truncated */
	    unsigned rupdate : 1;   /* was directory updated during reintegration */
	    unsigned rstale : 1;    /* reintegration: is client's directory version info stale? */
	} dir;
    } u;
#define	f_sid	    u.file.sid
#define	f_sinode    u.file.sinode
#define	f_finode    u.file.finode
#define	f_tinode    u.file.tinode
#define	f_tlength   u.file.tlength
#define	d_cinode    u.dir.cinode
#define	d_needsres  u.dir.needsres
#define d_needslogpurge u.dir.purgelog
#define d_needslogtrunc u.dir.trunclog
#define d_reintupdate u.dir.rupdate
#define d_reintstale u.dir.rstale

    vle(ViceFid *Fid) {
	fid = *Fid;
	vptr = 0;

	/* HC chokes on the correct code (below), so we simply bzero. -JJK */
	bzero(&u, (int)sizeof(u));
/*
	if (!ISDIR(fid)) {
	    f_sid = NullSid;
	    f_sinode = 0;
	    f_finode = 0;
	    f_tinode = 0;
	    f_tlength = 0;
	} else {
	    d_cinode = 0;
	    d_needsres = 0;
	    d_needslogpurge = 0;
	    d_needslogtrunc = 0;
	    d_reintupdate = 0;
	    d_reintstale = 0;
	}
*/
    };

/*
    ~vle() {
	assert(vptr == 0);
    };
*/
};

#endif	not _VICE_VLIST_H_
