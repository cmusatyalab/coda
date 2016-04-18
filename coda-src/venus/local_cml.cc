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

/* Find a consistent server replica to act as our 'global'.
 * All of this is necessary to create these replica objects in fsdb::Get. */
static int GetGlobalReplica(fsobj **global, VenusFid *fid)
{
    VolumeId volumeids[VSG_MEMBERS];
    volent *vol;
    repvol *rvol;
    int rc;
    VenusFid replicafid = *fid;
    vproc *vp = VprocSelf();

    CODA_ASSERT(global);
    *global = NULL;

    VDB->Get(&vol, MakeVolid(fid));
    if (!vol) return VNOVOL;

    rc = EVOLUME;
    if (!vol->IsReplicated())
	goto unlock_out;

    rvol = (repvol *)vol;
    rvol->GetVids(volumeids);

    for (int i = 0; i < VSG_MEMBERS; i++) {
	if (!volumeids[i]) continue;

	replicafid.Volume = volumeids[i];
	rc = FSDB->Get(global, &replicafid, vp->u.u_uid, RC_DATA);
	if (rc == 0) break;

	/* any error code is no good, even EINCONS */
	/* This will probably be an error point when we get to handling
	 * mixed local/global and server/server conflicts, where EINCONS
	 * would be an acceptable error return. */
	if (*global)
	    FSDB->Put(global);

	continue;
    }

    if (*global) {
	LOG(0, ("CheckRepair_GetObjects: using global %s -> %s\n",
		FID_(&replicafid), FID_(fid)));

	/* XXX this isn't safe, we drop the lock, but don't have some other
	 * refcount to pin the object down. */
	FSDB->Put(global);
	rc = VFAIL;
	*global = FSDB->Find(&replicafid);
    }

unlock_out:
    VDB->Put(&vol);

    return (*global) ? 0 : rc;
}

/* ********** beginning of cmlent methods ********** */
/* CheckRepair step 1: check mutation operand(s) */
static int CheckRepair_GetObjects(const char *operation, VenusFid *fid,
				  fsobj **global, fsobj **local,
				  char *msg, int *mcode, int *rcode)
{
    vproc *vp = VprocSelf();
    char path[MAXPATHLEN];
    int rc;

    LOG(100, ("cmlent::CheckRepair: %s on %s\n", operation, FID_(fid)));

    /* Get is used instead of Find to get a useful error code */
    rc = FSDB->Get(local, fid, vp->u.u_uid, RC_DATA);
    if (rc != 0) {
	/* figure out what the error was */

        path[0] = '\0';
	if(*local)
	  (*local)->GetPath(path, 1);
	*mcode = MUTATION_MISS_TARGET;
	*rcode = REPAIR_FAILURE;
	switch (rc) {
	case EIO:
	case ENOENT:
	    sprintf(msg, "conflict: %s target %s no longer exists on servers",
		    operation, path);
	    break;
	case EINCONS:
	    sprintf(msg, "conflict: %s target %s in server/server conflict",
		    operation, path);
	    break;
	case ESYNRESOLVE:
	case EASYRESOLVE:
	case ERETRY:
	    sprintf(msg, "fetching %s target failed (%d), please re-try!",
		    operation, rc);
	    break;
	default:
	    sprintf(msg, "fetching %s target failed (%d)", operation, rc);
	}
	return rc;
    }

    FSDB->Put(local); /* release RW lock */
    *local = FSDB->Find(fid); /* find the object instead */

    rc = GetGlobalReplica(global, fid);
    if(rc || !(*global)) {
      LOG(0, ("CheckRepair_GetObjects: Couldn't find a global replica for fid (%s)!\n", FID_(fid)));
      return rc;
    }

    return 0;
}

/* CheckRepair step 2: check mutation access rights */
static int CheckRepair_CheckAccess(fsobj *fso, int rights,
				   char *msg, int *mcode, int *rcode)
{
    vproc *vp = VprocSelf();
    char path[MAXPATHLEN];
    int ret;

    ret = fso->CheckAcRights(vp->u.u_uid, rights, 0);
    if (ret == EACCES) {
	LOG(0, ("cmlent::CheckRepair: acl check failed\n"));
	fso->GetPath(path, 1);
	sprintf(msg, "conflict: acl check failure on parent %s\n", path);
	*mcode = MUTATION_ACL_FAILURE;
	*rcode = REPAIR_FAILURE;
	return -1;
    }
    return 0;
}

/* CheckRepair step 3: check mutation semantic integrity, VV conflicts */
static int CheckRepair_CheckVVConflict(fsobj *fso, ViceVersionVector *VV,
				       char *msg)
{
    char path[MAXPATHLEN];

    if (VV_Cmp(fso->VV(), VV) != VV_EQ) {
	fso->GetPath(path, 1);
	sprintf(msg, "conflict: target %s updated on servers\n", path);
	return -1;
    }
    return 0;
}

/* CheckRepair step 3: check mutation semantic integrity, name/name conflicts */
static int CheckRepair_CheckNameConflict(fsobj *fso, char *name, int need,
					 char *msg, int *mcode, int *rcode)
{
    VenusFid dummy;
    char path[MAXPATHLEN];
    int exists;

    exists = (fso->dir_Lookup(name, &dummy, CLU_CASE_SENSITIVE) == 0);
    if ((!need && exists) || (need && !exists)) {
	fso->GetPath(path, 1);
	sprintf(msg, "conflict: target %s/%s %s on servers\n",
		path, name, need ? "is missing" : "exists");
	*mcode = MUTATION_NN_CONFLICT;
	*rcode = REPAIR_FAILURE;
	return -1;
    }
    return 0;
}

/*
  CheckRepair - check the semantic requirements for the current mutation
  operation
*/
/* need not be called from within a transaction */
void cmlent::CheckRepair(char *msg, int *mcode, int *rcode)
{
    /*
     * this method checks whether the mutation operation of this cmlent can be
     * performed "in commit order" to the global state that is current visible
     * to venus. it has the following OUT parameters:
     * msg  : a string of diagnostic error messages to be used by the repair
     *        tool.
     * mcode: a mutation error code indicating the nature of local/global
     *        conflict.
     * rcode: a repair error code indicating nature of the needed repair
     *        operation.
     */

    /* decl of shared local variables */
    fsobj *ParentObj;
    int rc = 0;

    /* initialization */
    fsobj *GlobalObjs[3] = {NULL, NULL, NULL};
    fsobj *LocalObjs[3]  = {NULL, NULL, NULL};
    strcpy(msg, "no conflict");

    switch (opcode) {
    case CML_Store_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("store", &u.u_store.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	/* can't use global pfso cause it doesn't exist, nor does pfid */
	/* if this doesn't work, use local->pfid with global->fid.Volume */
	OBJ_ASSERT(this, (ParentObj = LocalObjs[0]->pfso)); /*used to be global */
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(LocalObjs[0], GlobalObjs[0]->VV(), msg);
	break;

    case CML_Utimes_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("utimes", &u.u_utimes.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, (ParentObj = LocalObjs[0]->pfso));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(LocalObjs[0], GlobalObjs[0]->VV(), msg);
	break;

    case CML_Chown_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("chown", &u.u_chown.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, (ParentObj = LocalObjs[0]->pfso));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_ADMINISTER,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(LocalObjs[0], GlobalObjs[0]->VV(), msg);
	break;

    case CML_Chmod_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("chmod", &u.u_chmod.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, (ParentObj = LocalObjs[0]->pfso));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(LocalObjs[0], GlobalObjs[0]->VV(), msg);
	break;

    case CML_Create_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("create", &u.u_create.PFid,
				    &GlobalObjs[0], &LocalObjs[0],
				    msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_INSERT,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	rc = CheckRepair_CheckNameConflict(GlobalObjs[0], (char *)Name, 0,
					   msg, mcode, rcode);
	break;

    case CML_Link_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("link source", &u.u_link.PFid,
				    &GlobalObjs[0], &LocalObjs[0],
				    msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_GetObjects("link target", &u.u_link.CFid,
				    &GlobalObjs[1], &LocalObjs[1],
				    msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_INSERT,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	rc = CheckRepair_CheckNameConflict(GlobalObjs[0], (char *)Name, 0,
					   msg, mcode, rcode);
	break;

    case CML_SymLink_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("symlink", &u.u_symlink.PFid,
				    &GlobalObjs[0], &LocalObjs[0],
				    msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_INSERT,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	rc = CheckRepair_CheckNameConflict(GlobalObjs[0], (char *)NewName, 0,
					   msg, mcode, rcode);
	break;

    case CML_MakeDir_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("mkdir", &u.u_mkdir.PFid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_INSERT,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	rc = CheckRepair_CheckNameConflict(GlobalObjs[0], (char *)Name, 0,
					   msg, mcode, rcode);
	break;

    case CML_Remove_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("remove", &u.u_remove.PFid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_GetObjects("remove target", &u.u_remove.CFid,
				    &GlobalObjs[1], &LocalObjs[1],
				    msg, mcode, rcode);
	if (rc) break;

	OBJ_ASSERT(this, (LocalObjs[1] == NULL) || DYING(LocalObjs[1]) ||
		         (LocalObjs[1]->stat.LinkCount > 0));

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_DELETE,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_RU_CONFLICT;
	*rcode = REPAIR_FORCE_REMOVE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[1], &u.u_remove.CVV, msg);
	break;

    case CML_RemoveDir_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("removedir", &u.u_rmdir.PFid,
				    &GlobalObjs[0], &LocalObjs[0],
				    msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_GetObjects("removedir target", &u.u_rmdir.CFid,
				    &GlobalObjs[1], &LocalObjs[1],
				    msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_DELETE,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	if (!(GlobalObjs[1]->dir_IsEmpty())) {
	    char path[MAXPATHLEN];
	    GlobalObjs[1]->GetPath(path, 1);
	    sprintf(msg, "conflict: target %s not empty on servers\n", path);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	    rc = -1;
	    break;
	}

	*mcode = MUTATION_RU_CONFLICT;
	*rcode = REPAIR_FORCE_REMOVE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[1], &u.u_rmdir.CVV, msg);
	break;

    case CML_Rename_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("rename source dir", &u.u_rename.SPFid,
				    &GlobalObjs[0], &LocalObjs[0],
				    msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_GetObjects("rename target dir", &u.u_rename.TPFid,
				    &GlobalObjs[1], &LocalObjs[1],
				    msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_GetObjects("rename target object", &u.u_rename.SFid,
				    &GlobalObjs[2], &LocalObjs[2],
				    msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	rc = CheckRepair_CheckAccess(GlobalObjs[0], PRSFS_DELETE,
				     msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_CheckAccess(GlobalObjs[1], PRSFS_INSERT,
				     msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	rc = CheckRepair_CheckNameConflict(GlobalObjs[0], (char *)Name, 1,
					   msg, mcode, rcode);
	if (rc) break;

	rc = CheckRepair_CheckNameConflict(GlobalObjs[1], (char *)NewName, 0,
					   msg, mcode, rcode);
	break;

    case CML_Repair_OP:
	LOG(0, ("cmlent::CheckRepair: Disconnected Repair on 0x%s\n",
		FID_(&u.u_repair.Fid)));
	break;

    default:
	CHOKE("cmlent::CheckRepair: bogus opcode %d", opcode);
    }
    if (!rc)
	*mcode = *rcode = 0;

    LOG(0, ("cmlent::CheckRepair: mcode = %d rcode = %d msg = %s\n", *mcode, *rcode, msg));
}


static int DoRepair_GetObjects(VenusFid *fid, fsobj **global, fsobj **local)
{
    int rc;

    LOG(100, ("cmlent::DoRepair: (%s)\n", FID_(fid)));

    *local = FSDB->Find(fid);
    rc = GetGlobalReplica(global, fid);
    if (rc) return rc;

    CODA_ASSERT((*global) && (*local)); /* bad news at the moment*/
    return 0;
}

/*
  BEGIN_HTML
  <a name="dorepair"><strong> replay the actual actions required by
  the current mutation operation </strong></a>
  END_HTML
*/
/* must not be called from within a transaction */
int cmlent::DoRepair(char *msg, int rcode)
{
    OBJ_ASSERT(this, msg != NULL);
    int code = 0;
    VenusFid *fid;
    fsobj *GObj, *LObj;
    fsobj *GPObj, *LPObj;
    char GlobalPath[MAXPATHLEN], LocalPath[MAXPATHLEN];
    switch (opcode) {
    case CML_Store_OP:
	{  /*
	    * copy the cache file of the local object into the cache file
	    * file of the global, then s Store call on the global object.
	    */
	    fid = &u.u_store.Fid;
	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)\n",
			code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj && GObj->IsFile() && LObj->IsFile());
	    LOG(100, ("cmlent::DoRepair: do store on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));

	    if (!HAVEALLDATA(LObj))
		CHOKE("DoRepair: Store with no local data!\n");

#if 0 /* old */
	    /* call on _replicated_ volume (used to be global) */
	    /* since our VV struct must match global VV, do a SetLocalVV */
	    code = LObj->SetLocalVV(GObj->VV());
	    if(code) {
	      LOG(0, ("cmlent::DoRepair: Updating local VV failed!: %d\n",
		      code));
	      sprintf(msg, "store failed(couldn't set VV)\n");
	      break;
	    }
	    code = LObj->RepairStore();
#else /* new, uses individual VV's */
	    {
	      struct in_addr volumehosts[VSG_MEMBERS];
	      VolumeId volumeids[VSG_MEMBERS];
	      VenusFid replicafid = *fid;
	      fsobj *replicas[VSG_MEMBERS];
	      volent *vol;
	      vproc *vp = VprocSelf();

	      VDB->Get(&vol, MakeVolid(fid));
	      CODA_ASSERT(vol && vol->IsReplicated());

	      repvol *rv = (repvol *)vol;

	      rv->GetHosts(volumehosts);
	      rv->GetVids(volumeids);
	      VDB->Put(&vol);

	      if (!HAVEALLDATA(LObj))
		CHOKE("DoRepair: Store with no local data!");

	      code = 0;
	      for (int i = 0; (i < VSG_MEMBERS) && !code; i++) {
		if (!volumehosts[i].s_addr) continue;
		srvent *s = FindServer(&volumehosts[i]);
		CODA_ASSERT(s != NULL);

		replicafid.Volume = volumeids[i];
		code = FSDB->Get(&(replicas[i]), &replicafid,
				 vp->u.u_uid, RC_DATA);
		if(code || !replicas[i]) {
		  LOG(0, ("cmlent::DoRepair: failed fsdb::Get of %s(%d)\n",
			  FID_(&replicafid), code));
		  if(replicas[i])
		    FSDB->Put(&(replicas[i]));
		  break;
		}

		/* copy the local-obj cache file into the global-obj cache */
		LObj->data.file->Copy(replicas[i]->data.file);

		code = replicas[i]->RepairStore();
		LOG(0, ("cmlent::DoRepair: repair-storing (%s) %s\n",
			FID_(&replicafid), (code ? "failed" : "succeeded")));

		FSDB->Put(&(replicas[i]));
	      }
	    }
#endif
	    LObj->GetPath(LocalPath, 1);
	    if (rcode == REPAIR_OVER_WRITE) {
		if (code == 0) {
		    sprintf(msg, "overwrite %s succeeded\n", LocalPath);
		} else {
		    sprintf(msg, "overwrite %s failed(%d)\n", LocalPath, code);
		}
	    } else {
		if (code == 0) {
		    sprintf(msg, "store %s succeeded\n", LocalPath);
		} else {
		    sprintf(msg, "store %s failed(%d)\n", LocalPath, code);
		}
	    }
	    break;
	}
    case CML_Chmod_OP:
	{
	    fid = &u.u_chmod.Fid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)\n", code);
		break;
	    }
	    LOG(100, ("cmlent::DoRepair: do chmod on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    unsigned short NewMode = LObj->stat.Mode;		/* use local new mode */
	    code = LObj->RepairSetAttr((unsigned long)-1, (Date_t)-1,
				       (uid_t)-1, NewMode,
				       (RPC2_CountedBS *)NULL);
	    LObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "chmod %s succeeded", LocalPath);
	    } else {
		sprintf(msg, "chmod %s failed(%d)", LocalPath, code);
	    }
	    break;
	}
    case CML_Chown_OP:
	{
	    fid = &u.u_chown.Fid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do chown on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    uid_t NewOwner = LObj->stat.Owner; 		/* use local new owner */
	    code = LObj->RepairSetAttr((unsigned long)-1, (Date_t)-1,
				       NewOwner, (unsigned short)-1,
				       (RPC2_CountedBS *)NULL);
	    LObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "chown %s succeeded", LocalPath);
	    } else {
		sprintf(msg, "chown %s failed(%d)", LocalPath, code);
	    }
	    break;
	}
    case CML_Utimes_OP:
	{
	    fid = &u.u_chown.Fid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do utimes on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    Date_t NewDate = LObj->stat.Date;		/* use local date */
	    code = LObj->RepairSetAttr((unsigned long)-1, NewDate,
				       (unsigned short)-1, (unsigned short)-1,
				       NULL);
	    LObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "setattr %s succeeded", LocalPath);
	    } else {
		sprintf(msg, "setattr %s failed(%d)", LocalPath, code);
	    }
	    break;
	}
    case CML_Create_OP:
	{   /* do fid replacement after the creation succeeded. */
	    fid = &u.u_create.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)",
			code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do create on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_create.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "create semantic re-validation failed (%d)",
			code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsFile());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = LPObj->RepairCreate(&target, (char *)Name, NewMode,
				       FSDB->StdPri());
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "create %s/%s succeeded", LocalPath,
			(char *)Name);
		target->UnLock(WR);  /* release write lock on the new object */
	    } else {
		sprintf(msg, "create %s/%s failed(%d)", LocalPath,
			(char *)Name, code);
	    }
	    break;
	}
    case CML_Link_OP:
	{
	    fid = &u.u_link.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)",
			code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do link on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_link.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj && GObj->IsFile() && LObj->IsFile());
	    LOG(100, ("cmlent::DoRepair: do link on target %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    code = LPObj->RepairLink((char *)Name, GObj);
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "link %s/%s succeeded", LocalPath, (char *)Name);
	    } else {
		sprintf(msg, "link %s/%s failed(%d)", LocalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_MakeDir_OP:
	{
	    fid = &u.u_link.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do mkdir on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_mkdir.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "mkdir semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsDir());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = LPObj->RepairMkdir(&target, (char *)Name,
				      NewMode, FSDB->StdPri());
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "mkdir %s/%s succeeded", LocalPath, (char *)Name);
		target->UnLock(WR);  /* release write lock on the new object */
	    } else {
		sprintf(msg, "mkdir %s/%s failed(%d)",
			LocalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_SymLink_OP:
	{
	    fid = &u.u_symlink.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do symlink on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_symlink.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "symlink semantic re-validation failed (%d)",
			code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsSymLink());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = LPObj->RepairSymlink(&target, (char *)NewName,
					(char *)Name, NewMode, FSDB->StdPri());
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "symlink %s/%s -> %s succeeded",
			LocalPath, (char *)NewName, (char *)Name);
		target->UnLock(WR);   /* relese write lock on the new object */
	    } else {
		sprintf(msg, "symlink %s/%s -> %s failed(%d)",
			LocalPath, (char *)NewName, (char *)Name, code);
	    }
	    break;
	}
    case CML_Remove_OP:
	{
	    fid = &u.u_remove.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)",
			code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do remove on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_remove.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj) || LObj->stat.LinkCount > 0));
	    LOG(100, ("cmlent::DoRepair: do remove on global target %s\n",
		      FID_(&GObj->fid)));
	    code = LPObj->RepairRemove((char *)Name, GObj);
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "remove %s/%s succeeded", LocalPath, (char *)Name);
	    } else {
		sprintf(msg, "remove %s/%s failed(%d)", LocalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_RemoveDir_OP:
	{
	    fid = &u.u_rmdir.PFid;
	    code = DoRepair_GetObjects(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_rmdir.CFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj)) && GObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on global target %s\n",
		      FID_(&GObj->fid)));
	    code = LPObj->RepairRmdir((char *)Name, GObj);
	    LPObj->GetPath(LocalPath, 1);
	    if (code == 0) {
		sprintf(msg, "rmdir %s/%s succeeded", LocalPath, (char *)Name);
	    } else {
		sprintf(msg, "rmdir %s/%s failed(%d)", LocalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_Rename_OP:
	{
	    fsobj *GSPObj, *LSPObj;
	    fsobj *GTPObj, *LTPObj;
	    fid = &u.u_rename.SPFid;
	    code = DoRepair_GetObjects(fid, &GSPObj, &LSPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GSPObj && LSPObj && GSPObj->IsDir() && LSPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rename on source parent %s and %s\n",
		      FID_(&GSPObj->fid), FID_(&LSPObj->fid)));
	    fid = &u.u_rename.TPFid;
	    code = DoRepair_GetObjects(fid, &GTPObj, &LTPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GTPObj && LTPObj && GTPObj->IsDir() && LTPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rename on target parent %s and %s\n",
		      FID_(&GTPObj->fid), FID_(&LTPObj->fid)));
	    fid = &u.u_rename.SFid;

	    code = DoRepair_GetObjects(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do rename on source object %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    code = LTPObj->RepairRename(GSPObj, (char *)Name,
					GObj, (char *)NewName,
					(fsobj *)NULL);
	    /*
	     * note that the target object is always NULL here because a disconnected rename
	     * operation with target object is always split into a rename without a target
	     * preceeded by a remove operation for that target object.
	     */
	    char SPath[MAXPATHLEN], TPath[MAXPATHLEN];
	    LSPObj->GetPath(SPath, 1);
	    LTPObj->GetPath(TPath, 1);
	    if (code == 0) {
		sprintf(msg, "rename %s/%s -> %s/%s succeeded",
			SPath, (char *)Name, TPath, (char *)NewName);
	    } else {
		sprintf(msg, "rename %s/%s -> %s/%s failed(%d)",
			SPath, (char *)Name, TPath, (char *)NewName, code);
	    }
	    break;
	}
    case CML_Repair_OP:
	{
	    fid = &u.u_repair.Fid;
	    LOG(0, ("cmlent::DoRepair: Disconnected Repair on %s\n",
		    FID_(fid)));
	    break;
	}
    default:
	CHOKE("cmlent::DoRepair: bogus opcode %d", opcode);
    }
    return code;
}


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
