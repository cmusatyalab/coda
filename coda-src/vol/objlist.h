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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/vlist.h,v 4.4 1998/08/26 21:22:28 braam Exp $";
#endif /*_BLURB_*/


#ifndef _VICE_OBJLIST_H_
#define _VICE_OBJLIST_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <codadir.h>
#include <srv.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus
#include "cvnode.h"


#define	f_sid	        u.file.sid
#define	f_sinode        u.file.sinode
#define	f_finode        u.file.finode
#define	f_tinode        u.file.tinode
#define	f_tlength       u.file.tlength
#define d_inodemod      u.dir.inodemod
#define	d_cinode        u.dir.cinode
#define	d_needsres      u.dir.needsres
#define d_needslogpurge u.dir.purgelog
#define d_needslogtrunc u.dir.trunclog
#define d_reintupdate   u.dir.rupdate
#define d_reintstale    u.dir.rstale


struct objlist {
	struct dllist_head objl_lh;
}

/* The data structure we want here is a binary search tree, but we use a list instead. -JJK */
struct obj
{
	struct dllist_head obj_chain;
	ViceFid obj_fid;
	Vnode *obj_vptr;
	olist obj_sl;   /* list of spooled vmres log records  */
	olist obj_rsl;  /* list of spooled rvm log records  */
	union {
		struct {
			ViceStoreId	sid;   /* sid of LAST data
			store (used to avoid multiple bulk transfers) */
			Inode sinode;    /* inode to dec on success */
			Inode finode;  /* inode to dec on failure */
			Inode tinode;  /* inode to trunc on success */
			unsigned tlength; /* length to trunc t_inode
					     to (on success) */
		} file;
		struct {
			PDirInode cinode;  /* cloned inode (in RVM)  */
			int inodemod;      /* inode or pages modified */
			int needsres;      /* does directory need to
			be resolved at end of reintegration? */
			int purgelog;      /* should directory log be purged */
			int trunclog;  	   /* should log be truncated */
			unsigned rupdate : 1;  /* was directory
						  updated during
						  reintegration */
			unsigned rstale : 1;   /* reintegration: is
			client's directory version info stale? */
		} dir;
	} obj_u;

};


#endif	not _VICE_VLIST_H_
