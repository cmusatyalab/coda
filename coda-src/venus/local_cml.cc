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
/* must be called from within a transaction */
void cmlent::TranslateFid(VenusFid *global, VenusFid *local)
{
    OBJ_ASSERT(this, global && local);
    LOG(100, ("cmlent::TranslateFid: global = %s local = %s\n",
	      FID_(global), FID_(local)));
    VenusFid *Fids[3];
    GetAllFids(Fids);
    int count = 0;
    for (int i = 0; i < 3; i++) {
        /* Check if Fids[i] is global */
	if (Fids[i] && FID_EQ(Fids[i], global)) {
            RVMLIB_REC_OBJECT(*Fids[i]);
            *Fids[i] = *local;
            count++;
	}
    }
    LOG(100, ("cmlent::TranslateFid: %d fids have been replaced\n", count));
}

/* must not be called from within a transaction */
int cmlent::LocalFakeify()
{
    int rc;
    VenusFid *Fids[3];
    GetAllFids(Fids);
    /* 
     * note that for each cmlent, Fids[0] is always the root fid of the
     * subtree that are affected by the IFT. the only exception is 
     * for a rename operation where Fids[2] could be the root fid of
     * another subtree that is affected by the operation.
     */
    VenusFid *fid = Fids[0];
    OBJ_ASSERT(this, !FID_IsLocalFake(fid));
    fsobj *root;
    OBJ_ASSERT(this, root = FSDB->Find(fid));
    if (DYING(root)) {
	LOG(100, ("cmlent::LocalFakeify: object %s removed\n",
		  FID_(fid)));
	/* it must belong to some local subtree to be repaired */
	SetRepairFlag();
	/* prevent this cmlent from being aborted or reintegrated later */
	return ENOENT;
    }
    rc = root->LocalFakeify();
    if (rc != 0) return rc;
    
    fid = Fids[2];
    if (!fid || FID_IsLocalFake(fid)) {
	SetRepairFlag();
	return (0);
    }
    OBJ_ASSERT(this, root = FSDB->Find(fid));
    if (DYING(root)) {
	LOG(100, ("cmlent::LocalFakeify: object %s removed\n",
		  FID_(fid)));
	SetRepairFlag();
	return ENOENT;
    }
    rc = root->LocalFakeify();
    if (rc == 0) {
	SetRepairFlag();	
    }
    return rc;
}

/* CheckRepair step 1: check mutation operand(s) */
static int CheckRepair_GetObjects(const char *operation, VenusFid *fid,
				  fsobj **global, fsobj **local,
				  char *msg, int *mcode, int *rcode)
{
    char path[MAXPATHLEN];
    int rc;

    LOG(100, ("cmlent::CheckRepair: %s on %s\n", operation, FID_(fid)));

    rc = LRDB->FindRepairObject(fid, global, local);
    if (rc != 0) {
	/* figure out what the error was */
	(*local)->GetPath(path, 1);
	*mcode = MUTATION_MISS_TARGET;
	*rcode = REPAIR_FAILURE;
	switch (rc) {
	case EIO:
	case ENOENT:
	    sprintf(msg, "conflict: %s target %s no longer exits on servers", operation, path);
	    break;
	case EINCONS:
	    sprintf(msg, "conflict: %s target %s in server/server conflict", operation, path);
	    break;
	case ESYNRESOLVE:
	case EASYRESOLVE:
	case ERETRY:
	    sprintf(msg, "fetching %s target failed (%d), please re-try!", operation, rc);
	    break;
	default:
	    sprintf(msg, "fetching %s target failed (%d)", operation, rc);
	}
	return rc;
    }

    CODA_ASSERT(*global != NULL);
    return 0;
}

/* CheckRepair step 2: check mutation access rights */
static int CheckRepair_CheckAccess(fsobj *fso, int rights,
				   char *msg, int *mcode, int *rcode)
{
    vproc *vp = VprocSelf();
    char path[MAXPATHLEN];

    if (fso->CheckAcRights(vp->u.u_uid, rights, 0) == EACCES) {
	LOG(100, ("cmlent::CheckRepair: acl check failed\n"));
	fso->GetPath(path, 1);
	sprintf(msg, "conflict: acl check failure on parent %s", path);
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
	sprintf(msg, "conflict: target %s updated on servers", path);
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
	sprintf(msg, "conflict: target %s/%s %s on servers",
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
     * msg  : a string of diagnostic error messages to be used by the repair tool.
     * mcode: a mutation error code indicating the nature of local/global conflict.
     * rcode: a repair error code indicating nature of the needed repair operation.
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
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;

	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[0], LocalObjs[0]->VV(),msg);
	break;

    case CML_Utimes_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("utimes", &u.u_utimes.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;
 
	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[0], LocalObjs[0]->VV(),msg);
	break;

    case CML_Chown_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("chown", &u.u_chown.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_ADMINISTER,
				     msg, mcode, rcode);
	if (rc) break;
 
	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[0], LocalObjs[0]->VV(),msg);
	break;

    case CML_Chmod_OP:
	/* step 1 */
	rc = CheckRepair_GetObjects("chmod", &u.u_chmod.Fid, &GlobalObjs[0],
				    &LocalObjs[0], msg, mcode, rcode);
	if (rc) break;

	/* step 2 */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	rc = CheckRepair_CheckAccess(ParentObj, PRSFS_WRITE, msg, mcode, rcode);
	if (rc) break;
 
	/* step 3 */
	*mcode = MUTATION_VV_CONFLICT;
	*rcode = REPAIR_OVER_WRITE;
	rc = CheckRepair_CheckVVConflict(GlobalObjs[0], LocalObjs[0]->VV(),msg);
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

	/* step2 */
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

    LOG(100, ("cmlent::CheckRepair: mcode = %d rcode = %d msg = %s\n", *mcode, *rcode, msg));
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
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj && GObj->IsFile() && LObj->IsFile());
	    LOG(100, ("cmlent::DoRepair: do store on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));

	    if (!HAVEALLDATA(LObj))
		CHOKE("DoRepair: Store with no local data!");

	    /* copy the local-obj cache file into the global-obj cache */
	    LObj->data.file->Copy(GObj->data.file);

	    /* set the local-obj length to the global-obj length */
	    GObj->stat.Length = LObj->stat.Length;
	    code = GObj->RepairStore();
	    GObj->GetPath(GlobalPath, 1);
	    if (rcode == REPAIR_OVER_WRITE) {
		LObj->GetPath(LocalPath, 1);
		if (code == 0) {
		    sprintf(msg, "overwrite %s with %s succeeded", GlobalPath, LocalPath);
		} else {
		    sprintf(msg, "overwrite %s with %s failed(%d)", GlobalPath, LocalPath, code);
		}
	    } else {
		if (code == 0) {
		    sprintf(msg, "store %s succeeded", GlobalPath);
		} else {
		    sprintf(msg, "store %s failed(%d)", GlobalPath, code);
		}
	    }
	    break;
	}
    case CML_Chmod_OP:
	{   
	    fid = &u.u_chmod.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    LOG(100, ("cmlent::DoRepair: do chmod on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    unsigned short NewMode = LObj->stat.Mode;		/* use local new mode */
	    GObj->stat.Mode = NewMode;			        /* set mode for global-obj */
	    code = GObj->RepairSetAttr((unsigned long)-1, (unsigned long)-1, 
				       (unsigned short)-1, NewMode,
				       (RPC2_CountedBS *)NULL);
	    GObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "chmod %s succeeded", GlobalPath);
	    } else {
		sprintf(msg, "chmod %s failed(%d)", GlobalPath, code);
	    }
	    break;
	}
    case CML_Chown_OP:
	{   fid = &u.u_chown.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do chown on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    uid_t NewOwner = LObj->stat.Owner; 		/* use local new owner */
	    GObj->stat.Owner = NewOwner; 	    		/* set for global-obj */
	    code = GObj->RepairSetAttr((unsigned long)-1, (unsigned long)-1, 
				       NewOwner, (unsigned short)-1,
				       (RPC2_CountedBS *)NULL);
	    GObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "chown %s succeeded", GlobalPath);
	    } else {
		sprintf(msg, "chown %s failed(%d)", GlobalPath, code);
	    }
	    break;
	}
    case CML_Utimes_OP:
	{   
	    fid = &u.u_chown.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do utimes on %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    Date_t NewDate = LObj->stat.Date;			/* use local date */
	    GObj->stat.Date = NewDate;	    			/* set time-stamp for global-obj */
	    code = GObj->RepairSetAttr((unsigned long)-1, NewDate, (unsigned short)-1,
				       (unsigned short)-1, NULL);
	    GObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "setattr %s succeeded", GlobalPath);
	    } else {
		sprintf(msg, "setattr %s failed(%d)", GlobalPath, code);
	    }
	    break;
	}
    case CML_Create_OP:
	{   /* do fid replacement after the creation succeeded. */
	    fid = &u.u_create.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do create on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_create.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "create semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsFile());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairCreate(&target, (char *)Name, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "create %s/%s succeeded", GlobalPath, (char *)Name);
		target->UnLock(WR);		/* release write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_create.CFid);
	    } else {
		sprintf(msg, "create %s/%s failed(%d)", GlobalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_Link_OP:
	{   
	    fid = &u.u_link.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do link on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_link.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj && GObj->IsFile() && LObj->IsFile());
	    LOG(100, ("cmlent::DoRepair: do link on target %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    code = GPObj->RepairLink((char *)Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "link %s/%s succeeded", GlobalPath, (char *)Name);
	    } else {
		sprintf(msg, "link %s/%s failed(%d)", GlobalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_MakeDir_OP:
	{   
	    fid = &u.u_link.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do mkdir on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_mkdir.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "mkdir semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsDir());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairMkdir(&target, (char *)Name, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "mkdir %s/%s succeeded", GlobalPath, (char *)Name);
		target->UnLock(WR);			/* relese write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_mkdir.CFid);
	    } else {
		sprintf(msg, "mkdir %s/%s failed(%d)", GlobalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_SymLink_OP:
	{   
	    fid = &u.u_symlink.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do symlink on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_symlink.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "symlink semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsSymLink());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairSymlink(&target, (char *)NewName,
					(char *)Name, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "symlink %s/%s -> %s succeeded",
			GlobalPath, (char *)NewName, (char *)Name);
		target->UnLock(WR);			/* relese write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_symlink.CFid);
	    } else {
		sprintf(msg, "symlink %s/%s -> %s failed(%d)",
			GlobalPath, (char *)NewName, (char *)Name, code);
	    }
	    break;
	}
    case CML_Remove_OP:
	{   
	    fid = &u.u_remove.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do remove on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_remove.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj) || LObj->stat.LinkCount > 0));
	    LOG(100, ("cmlent::DoRepair: do remove on global target %s\n",
		      FID_(&GObj->fid)));
	    code = GPObj->RepairRemove((char *)Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "remove %s/%s succeeded", GlobalPath, (char *)Name);
	    } else {
		sprintf(msg, "remove %s/%s failed(%d)", GlobalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_RemoveDir_OP:
	{   
	    fid = &u.u_rmdir.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on parent %s and %s\n",
		      FID_(&GPObj->fid), FID_(&LPObj->fid)));
	    fid = &u.u_rmdir.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj)) && GObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on global target %s\n",
		      FID_(&GObj->fid)));
	    code = GPObj->RepairRmdir((char *)Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "rmdir %s/%s succeeded", GlobalPath, (char *)Name);
	    } else {
		sprintf(msg, "rmdir %s/%s failed(%d)", GlobalPath, (char *)Name, code);
	    }
	    break;
	}
    case CML_Rename_OP:
	{   
	    fsobj *GSPObj, *LSPObj;
	    fsobj *GTPObj, *LTPObj;
	    fid = &u.u_rename.SPFid;
	    code = LRDB->FindRepairObject(fid, &GSPObj, &LSPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GSPObj && LSPObj && GSPObj->IsDir() && LSPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rename on source parent %s and %s\n",
		      FID_(&GSPObj->fid), FID_(&LSPObj->fid)));
	    fid = &u.u_rename.TPFid;
	    code = LRDB->FindRepairObject(fid, &GTPObj, &LTPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GTPObj && LTPObj && GTPObj->IsDir() && LTPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rename on target parent %s and %s\n",
		      FID_(&GTPObj->fid), FID_(&LTPObj->fid)));
	    fid = &u.u_rename.SFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do rename on source object %s and %s\n",
		      FID_(&GObj->fid), FID_(&LObj->fid)));
	    code = GTPObj->RepairRename(GSPObj, (char *)Name,
					GObj, (char *)NewName,
					(fsobj *)NULL);
	    /* 
	     * note that the target object is always NULL here because a disconnected rename
	     * operation with target object is always split into a rename without a target
	     * preceeded by a remove operation for that target object.
	     */
	    char SPath[MAXPATHLEN], TPath[MAXPATHLEN];
	    GSPObj->GetPath(SPath, 1);
	    GTPObj->GetPath(TPath, 1);
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

/* must not be called from within a transaction */
void cmlent::SetRepairMutationFlag()
{
    if (flags.repair_mutation == 0) {
	Recov_BeginTrans();
	RVMLIB_REC_OBJECT(flags);
	flags.repair_mutation = 1;
	Recov_EndTrans(MAXFP);
    }
}

/* need not be called from within a transaction */
int cmlent::InLocalRepairSubtree(VenusFid *LocalRootFid)
{
    /*
     * check whether the objects mutated by this cmlent belongs to
     * the subtree rooted at the object whose fid equals RootFid.
     */
    OBJ_ASSERT(this, LocalRootFid && FID_IsLocalFake(LocalRootFid));
    LOG(100, ("cmlent::InLocalRepairSubtree: LocalRootFid = %s\n",
	      FID_(LocalRootFid)));
    VenusFid *Fids[3];
    fsobj *OBJ;
    GetAllFids(Fids);

    for (int i = 0; i < 3; i++) {
	if (!Fids[i]) continue;
	if (!FID_IsLocalFake(Fids[i])) continue;
	OBJ_ASSERT(this, OBJ = FSDB->Find(Fids[i]));
	if (OBJ->IsAncestor(LocalRootFid)) return 1;
    }
    
    return 0;
}

/* need not be called from within a transaction */
int cmlent::InGlobalRepairSubtree(VenusFid *GlobalRootFid)
{
    /*
     * check whether this cmlent is a mutation that was performed to mutate
     * objects contained in the global portion of a fake-subtree, rooted at 
     * GlobalRootFid. note that this mutation was not performed within the
     * repair-session for the subtree, otherwise, it could have been set to 
     * the right flag bits and right tid. note that we do not need to worry about 
     * this cmlent pointing to deleted objects because they are dirty and will 
     * not be GCed, so FSDB->Find can alway get the objects we need and OBJ::IsAncestor 
     * also works on dying objects because parent/child relationship is destroyed 
     * only at GC time.
     */
    OBJ_ASSERT(this, GlobalRootFid && !FID_IsLocalFake(GlobalRootFid));
    LOG(100, ("cmlent::InGlobalRepairSubtree: GlobalRootFid = %s\n",
	      FID_(GlobalRootFid)));

    if (flags.to_be_repaired || flags.repair_mutation) {
	LOG(100, ("cmlent::InGlobalRepairSubtree: repair flag(s) set already\n"));
	return 0;
    }

    VenusFid *Fids[3];
    fsobj *OBJ;
    GetAllFids(Fids);

    for (int i = 0; i < 3; i++) {
	if (!Fids[i]) continue;
	if (FID_IsLocalFake(Fids[i])) break;
	OBJ = FSDB->Find(Fids[i]);
	if (!OBJ) continue;
	if (OBJ->IsAncestor(GlobalRootFid)) return 1;
    }
    return 0;
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
