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


/* this file contains local-repair related cmlent methods */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/file.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif

/* from libal */
#include <prs.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "local.h"
#include "fso.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"

/* interfaces */
#include <cml.h>

/* ********** beginning of cmlent methods ********** */

/* need not be called from within a transaction */
void cmlent::GetLocalOpMsg(char *msg)
{
    char path[MAXPATHLEN];

    OBJ_ASSERT(this, msg);
    switch (opcode) {
    case CML_Store_OP:
	{
		RecoverPathName(path, &u.u_store.Fid, log, this);
		sprintf(msg, "store %s", path);
		break;
	}
    case CML_Chmod_OP:
	{
		RecoverPathName(path, &u.u_chmod.Fid, log, this);
		sprintf(msg, "chmod %s", path);
		break;
	}
    case CML_Chown_OP:
	{
		RecoverPathName(path, &u.u_chown.Fid, log, this);
		sprintf(msg, "chown %s", path);
		break;
	}
    case CML_Utimes_OP:
	{
		RecoverPathName(path, &u.u_utimes.Fid, log, this);
		sprintf(msg, "setattr %s", path);
		break;
	}
    case CML_Create_OP:
	{
		RecoverPathName(path, &u.u_create.CFid, log, this);
		sprintf(msg, "create %s", path);
		break;
	}
    case CML_Link_OP:
	{
		RecoverPathName(path, &u.u_link.CFid, log, this);
		sprintf(msg, "link %s", path);
		break;
	}
    case CML_MakeDir_OP:
	{
		RecoverPathName(path, &u.u_mkdir.CFid, log, this);
		sprintf(msg, "mkdir %s", path);
		break;
	}
    case CML_SymLink_OP:
	{
		RecoverPathName(path, &u.u_symlink.CFid, log, this);
		sprintf(msg, "symlink %s --> %s", path, (char *)NewName);
		break;
	}
    case CML_Remove_OP:
	{
		RecoverPathName(path, &u.u_remove.CFid, log, this);
		sprintf(msg, "remove %s", path);
		break;
	}
    case CML_RemoveDir_OP:
	{
		RecoverPathName(path, &u.u_rmdir.CFid, log, this);
		sprintf(msg, "rmdir %s", path);
		break;
	}
    case CML_Rename_OP:
	{
		char tppath[MAXPATHLEN];
		RecoverPathName(path, &u.u_rename.SPFid, log, this);
		RecoverPathName(tppath, &u.u_rename.TPFid, log, this);
		sprintf(msg, "rename %s/%s -> %s/%s",
			path, (char *)Name, tppath, (char *)NewName);
		break;
	}
    case CML_Repair_OP:
	{	VenusFid *fid = &u.u_repair.Fid;
		LOG(0, ("cmlent::GetLocalOpMsg: Disconnected Repair on %s\n",
			FID_(fid)));
		sprintf(msg, "disconnected repair on %s", FID_(fid));
		break;
        }
    default:
	CHOKE("cmlent::GetLocalOpMsg: bogus opcode %d", opcode);
    }
}

/* must not be called from within a transaction */
void cmlent::SetRepairFlag()
{
    if (flags.to_be_repaired == 0) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(*this);
	flags.to_be_repaired = 1;
	this->tid = 0;	/* also reset its tid, may not be necessary */
	Recov_EndTrans(MAXFP);
    }
}

/* need not be called from within a transaction */
void cmlent::GetVVandFids(ViceVersionVector *vvs[], VenusFid *fids[])
{
    fids[0] = 0; fids[1] = 0; fids[2] = 0;
    vvs[0] = 0; vvs[1] = 0; vvs[2] = 0;
    switch(opcode) {
        case CML_Store_OP:
	    fids[0] = &u.u_store.Fid;
	    vvs[0] = &u.u_store.VV;
	    break;

	case CML_Utimes_OP:
	    fids[0] = &u.u_utimes.Fid;
	    vvs[0] = &u.u_utimes.VV;
	    break;

	case CML_Chown_OP:
	    fids[0] = &u.u_chown.Fid;
	    vvs[0] = &u.u_chown.VV;
	    break;

	case CML_Chmod_OP:
	    fids[0] = &u.u_chmod.Fid;
	    vvs[0] = &u.u_chmod.VV;
	    break;

	case CML_Create_OP:
	    fids[0] = &u.u_create.PFid;
	    vvs[0] = &u.u_create.PVV;
	    fids[1] = &u.u_create.CFid;
	    break;

	case CML_Remove_OP:
	    fids[0] = &u.u_remove.PFid;
	    vvs[0] = &u.u_remove.PVV;
	    fids[1] = &u.u_remove.CFid;
	    vvs[1] = &u.u_remove.CVV;
	    break;

	case CML_Link_OP:
	    fids[0] = &u.u_link.PFid;
	    vvs[0] = &u.u_link.PVV;
	    fids[1] = &u.u_link.CFid;
	    vvs[1] = &u.u_link.CVV;
	    break;

	case CML_Rename_OP:
	    fids[0] = &u.u_rename.SPFid;
	    vvs[0] = &u.u_rename.SPVV;
	    fids[1] = &u.u_rename.SFid;
	    vvs[1] = &u.u_rename.SVV;
	    if (!FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid)) {
		fids[2] = &u.u_rename.TPFid;
		vvs[2] = &u.u_rename.TPVV;
	    }
	    break;

	case CML_MakeDir_OP:
	    fids[0] = &u.u_mkdir.PFid;
	    vvs[0] = &u.u_mkdir.PVV;
	    fids[1] = &u.u_mkdir.CFid;
	    break;

	case CML_RemoveDir_OP:
	    fids[0] = &u.u_rmdir.PFid;
	    vvs[0] = &u.u_rmdir.PVV;
	    fids[1] = &u.u_rmdir.CFid;
	    vvs[1] = &u.u_rmdir.CVV;
	    break;

	case CML_SymLink_OP:
	    fids[0] = &u.u_symlink.PFid;
	    vvs[0] = &u.u_symlink.PVV;
	    fids[1] = &u.u_symlink.CFid;
	    break;

        case CML_Repair_OP:
	    fids[0] = &u.u_repair.Fid;
	    break;
	default:
	    CHOKE("cmlent::GetVVandFids: bogus opcode (%d)", opcode);
    }
}

/* need not be called from within a transaction */
void cmlent::GetAllFids(VenusFid *fids[]) 
{
    fids[0] = 0; fids[1] = 0; fids[2] = 0;
    switch(opcode) {
        case CML_Store_OP:
	    fids[0] = &u.u_store.Fid;
	    break;

	case CML_Utimes_OP:
	    fids[0] = &u.u_utimes.Fid;
	    break;

	case CML_Chown_OP:
	    fids[0] = &u.u_chown.Fid;
	    break;

	case CML_Chmod_OP:
	    fids[0] = &u.u_chmod.Fid;
	    break;

	case CML_Create_OP:
	    fids[0] = &u.u_create.PFid;
	    fids[1] = &u.u_create.CFid;
	    break;

	case CML_Remove_OP:
	    fids[0] = &u.u_remove.PFid;
	    fids[1] = &u.u_remove.CFid;
	    break;

	case CML_Link_OP:
	    fids[0] = &u.u_link.PFid;
	    fids[1] = &u.u_link.CFid;
	    break;

	case CML_Rename_OP:
	    fids[0] = &u.u_rename.SPFid;
	    fids[1] = &u.u_rename.SFid;
	    fids[2] = &u.u_rename.TPFid;
	    break;

	case CML_MakeDir_OP:
	    fids[0] = &u.u_mkdir.PFid;
	    fids[1] = &u.u_mkdir.CFid;
	    break;

	case CML_RemoveDir_OP:
	    fids[0] = &u.u_rmdir.PFid;
	    fids[1] = &u.u_rmdir.CFid;
	    break;

	case CML_SymLink_OP:
	    fids[0] = &u.u_symlink.PFid;
	    fids[1] = &u.u_symlink.CFid;
	    break;

        case CML_Repair_OP:
	    fids[0] = &u.u_repair.Fid;
	    break;

	default:
	    CHOKE("cmlent::GetAllFids: bogus opcode (%d)", opcode);
    }
}


/* must not be called from within a transaction */
void cmlent::SetTid(int Tid)
{
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(this->tid);
    this->tid = Tid;
    Recov_EndTrans(MAXFP);
}


int cmlent::ContainLocalFid()
{
    VenusFid *Fids[3];
    GetAllFids(Fids);
    for (int i = 0; i < 3; i++) {
	if (Fids[i] == NULL) continue;
	if (FID_IsLocalFake(Fids[i])) {
	    return 1;
	}
    }
    return 0;    
}

/* ********** end of cmlent methods ********** */

/* ********** beginning of ClientModifyLog methods ********** */
/* must not be called from within a transaction */
int ClientModifyLog::HaveElements(int Tid)
{
    /* check wether there is any cmlent that has tid equal to Tid */
    cml_iterator next(*this, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	if (m->GetTid() == Tid)
	  return 1;
    }
    return 0;
}
/* ********** end of ClientModifyLog methods ********** */
