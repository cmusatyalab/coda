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

/*
 * Vlist.h -- Created October 1989
 * Author Puneet Kumar
 */

#ifndef _VICE_VLIST_H_
#define _VICE_VLIST_H_ 1

struct vle;

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdlib.h>
#include <util.h>
#include <codadir.h>
#ifdef __cplusplus
}
#endif
#include <srv.h>
#include <olist.h>
#include <dlist.h>
#include <vice.h>
#include "cvnode.h"

extern int VLECmp(vle *, vle *);
extern vle *FindVLE(dlist &, ViceFid *);
extern vle *AddVLE(dlist &, ViceFid *);

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

/* The data structure we want here is a binary search tree, but we use a list instead. -JJK */
struct vle : public dlink {
    ViceFid fid;
    Vnode *vptr;
    olist sl; /* list of spooled log records - for res logs in vm only */
    olist rsl; /* list of spooled log records - for res logs in rvm */
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
    } u;

    vle(ViceFid *Fid)
    {
        fid  = *Fid;
        vptr = 0;

        if (!ISDIR(fid)) {
            f_sid     = NullSid;
            f_sinode  = 0;
            f_finode  = 0;
            f_tinode  = 0;
            f_tlength = 0;
        } else {
            d_cinode        = 0;
            d_inodemod      = 0;
            d_needsres      = 0;
            d_needslogpurge = 0;
            d_needslogtrunc = 0;
            d_reintupdate   = 0;
            d_reintstale    = 0;
        }
    };

    ~vle() { CODA_ASSERT(vptr == 0); };
};

#endif /* _VICE_VLIST_H_ */
