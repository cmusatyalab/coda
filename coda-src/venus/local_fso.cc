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

/* this file contains local-repair related fsobj methods */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <struct.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <codadir.h>
#include <fcntl.h>

/* interfaces */
#include <vcrcommon.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif



/* from venus */
#include "fso.h"
#include "local.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "venusvol.h"
#include "worker.h"

/* MUST be called from within a transaction */
void fsobj::SetComp(char *name)
{
    RVMLIB_REC_OBJECT(comp);
    if (comp) rvmlib_rec_free(comp);
    if (name && name[0] != '\0')
	 comp = rvmlib_rec_strdup(name);
    else comp = rvmlib_rec_strdup("");
}

const char *fsobj::GetComp(void)
{
    if (comp && comp[0] != '\0')
	 return comp;
    else return FID_(&fid);
}

/* must be called from within a transaction */
void fsobj::SetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 1;
}

/* must be called from within a transaction */
void fsobj::UnsetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 0;
}

/* need not be called from within a transaction */
cmlent *fsobj::FinalCmlent(int tid)
{
    /* return the last cmlent done by iot tid */
    LOG(100, ("fsobj::FinalCmlent: %s\n", FID_(&fid)));
    FSO_ASSERT(this, mle_bindings);
    dlist_iterator next(*mle_bindings);
    dlink *d;
    cmlent *last = (cmlent *)0;

    while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	cmlent *m = (cmlent *)b->binder;
	CODA_ASSERT(m);
	if (m->GetTid() != tid) continue;
	last = m;
    }
    CODA_ASSERT(last && last->GetTid() == tid);
    return last;
}

int fsobj::RepairStore()
{
    FSO_ASSERT(this, REACHABLE(this));

    vproc *vp = VprocSelf();
    Date_t Mtime = Vtime();
    unsigned long NewLength = stat.Length;

    return DisconnectedStore(Mtime, vp->u.u_uid, NewLength, 1);
}

int fsobj::RepairSetAttr(unsigned long NewLength, Date_t NewDate,
			 uid_t NewOwner, unsigned short NewMode,
			 RPC2_CountedBS *acl)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedSetAttr(Mtime, vp->u.u_uid, NewLength, NewDate, NewOwner,
			       NewMode, 1);
}

int fsobj::RepairCreate(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedCreate(Mtime, vp->u.u_uid, t_fso_addr, name, Mode,
			      target_pri, 1);
}

int fsobj::RepairRemove(char *name, fsobj *target_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedRemove(Mtime, vp->u.u_uid, name, target_fso, 1);
}

int fsobj::RepairLink(char *name, fsobj *source_fso) {
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedLink(Mtime, vp->u.u_uid, name, source_fso, 1);
}

int fsobj::RepairRename(fsobj *s_parent_fso, char *s_name, fsobj *s_fso, char *t_name, fsobj *t_fso)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedRename(Mtime, vp->u.u_uid, s_parent_fso, s_name, s_fso,
			      t_name, t_fso, 1);
}


int fsobj::RepairMkdir(fsobj **t_fso_addr, char *name, unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedMkdir(Mtime, vp->u.u_uid, t_fso_addr, name, Mode,
			     target_pri, 1);
}

int fsobj::RepairRmdir(char *name, fsobj *target_fso)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedRmdir(Mtime, vp->u.u_uid, name, target_fso, 1);
}

int fsobj::RepairSymlink(fsobj **t_fso_addr, char *name, char *contents,
			    unsigned short Mode, int target_pri)
{
    Date_t Mtime = Vtime();
    vproc *vp = VprocSelf();
    return DisconnectedSymlink(Mtime, vp->u.u_uid, t_fso_addr, name, contents,
			       Mode, target_pri, 1);
}

/*  *****  SetLocalVV  *****  */

/* Used by Repair routines when sending local information to the servers! */
/* Call with object write-locked? */
int fsobj::SetLocalVV(ViceVersionVector *newvv)
{
    LOG(0, ("fsobj::SetLocalVV: (%s)\n", GetComp()));

    FSO_ASSERT(this, newvv != NULL);

    /* Do op locally. */
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(stat);
    stat.VV = *newvv;
    Recov_EndTrans(CMFP);

    return(0);
}
