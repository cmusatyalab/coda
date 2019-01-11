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

#define f_sid u.file.sid
#define f_sinode u.file.sinode
#define f_finode u.file.finode
#define f_tinode u.file.tinode
#define f_tlength u.file.tlength
#define d_inodemod u.dir.inodemod
#define d_cinode u.dir.cinode
#define d_needsres u.dir.needsres
#define d_needslogpurge u.dir.purgelog
#define d_needslogtrunc u.dir.trunclog
#define d_reintupdate u.dir.rupdate
#define d_reintstale u.dir.rstale

struct objlist {
    struct dllist_head objl_lh;
}

/* The data structure we want here is a binary search tree, but we use a list instead. -JJK */
struct obj {
    struct dllist_head obj_chain;
    ViceFid obj_fid;
    Vnode *obj_vptr;
    olist obj_sl; /* list of spooled vmres log records  */
    olist obj_rsl; /* list of spooled rvm log records  */
    union {
        struct {
            ViceStoreId sid; /* sid of LAST data
			store (used to avoid multiple bulk transfers) */
            Inode sinode; /* inode to dec on success */
            Inode finode; /* inode to dec on failure */
            Inode tinode; /* inode to trunc on success */
            unsigned tlength; /* length to trunc t_inode
					     to (on success) */
        } file;
        struct {
            PDirInode cinode; /* cloned inode (in RVM)  */
            int inodemod; /* inode or pages modified */
            int needsres; /* does directory need to
			be resolved at end of reintegration? */
            int purgelog; /* should directory log be purged */
            int trunclog; /* should log be truncated */
            unsigned rupdate : 1; /* was directory
						  updated during
						  reintegration */
            unsigned rstale : 1; /* reintegration: is
			client's directory version info stale? */
        } dir;
    } obj_u;
};

#endif not _VICE_VLIST_H_
