/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#endif __cplusplus

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
#endif __cplusplus

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

/* ********** beginning of misc routines ********** */
static int filecopy(int here, int there)
{
    register int kount;
    char buffer[BUFSIZ];
    kount = 0;
    while (kount == 0 && (kount=read(here,buffer,BUFSIZ)) > 0)
      kount -= write (there,buffer,kount);
    return (kount ? -1 : 0);
}
/* ********** end of misc routines ********** */

/* ********** beginning of cmlent methods ********** */
/* must be called from within a transaction */
void cmlent::TranslateFid(ViceFid *global, ViceFid *local)
{
    OBJ_ASSERT(this, global && local);
    LOG(100, ("cmlent::TransalteFid:global = 0x.%x.%x.%x local = 0x%x.%x.%x\n",
	      global->Volume, global->Vnode, global->Unique,
	      local->Volume, local->Vnode, local->Unique));
    ViceFid *Fids[3];
    GetAllFids(Fids);
    int count = 0;
    for (int i = 0; i < 3; i++) {
	if (Fids[i] != NULL) {
	    /* Check if Fids[i] is global */
	    if (!memcmp((const void *)Fids[i], (const void *)global, (int)sizeof(ViceFid))) {
		RVMLIB_REC_OBJECT(*Fids[i]);
		memmove((void *) Fids[i], (const void *)local, (int)sizeof(ViceFid));
		count++;
	    }
	}
    }
    LOG(100, ("cmlent::TranslateFid: %d fids has been replaced\n", count));
}

/* must not be called from within a transaction */
int cmlent::LocalFakeify()
{
    int rc;
    ViceVersionVector *VVs[3];
    ViceFid *Fids[3];
    GetVVandFids(VVs, Fids);
    /* 
     * note that for each cmlent, Fids[0] is always the root fid of the
     * subtree that are affected by the IFT. the only exception is 
     * for a rename operation where Fids[2] could be the root fid of
     * another subtree that is affected by the operation.
     */
    ViceFid *fid = Fids[0];
    OBJ_ASSERT(this, !FID_VolIsLocal(fid));
    fsobj *root;
    OBJ_ASSERT(this, root = FSDB->Find(fid));
    if (DYING(root)) {
	LOG(100, ("cmlent::LocalFakeify: object 0x%x.%x.%x removed\n",
		  fid->Volume, fid->Vnode, fid->Unique));
	/* it must belong to some local subtree to be repaired */
	SetRepairFlag();
	/* prevent this cmlent from being aborted or reintegrated later */
	return ENOENT;
    }
    rc = root->LocalFakeify();
    if (rc != 0) return rc;
    
    fid = Fids[2];
    if (fid == NULL || FID_VolIsLocal(fid)) {
	SetRepairFlag();
	return (0);
    }
    OBJ_ASSERT(this, !FID_VolIsLocal(fid));
    OBJ_ASSERT(this, root = FSDB->Find(fid));
    if (DYING(root)) {
	LOG(100, ("cmlent::LocalFakeify: object 0x%x.%x.%x removed\n",
		  fid->Volume, fid->Vnode, fid->Unique));
	SetRepairFlag();
	return ENOENT;
    }
    rc = root->LocalFakeify();
    if (rc == 0) {
	SetRepairFlag();	
    }
    return rc;
}

/*
  BEGIN_HTML
  <a name="checkrepair"><strong> check the semantic requirements for
  the current mutation operation </strong></a> 
  END_HTML
*/
/* need not be called from within a transaction */
void cmlent::CheckRepair(char *msg, int *mcode, int *rcode)
{	
    /*
     * this method checks whether the mutation operation of this cmlent can be
     * performed "in commit order" to the global state that is current visible to
     * vnues. it has the following OUT parameters:
     * msg  : a string of diagnostic error messages to be used by the repair tool.
     * mcode: a mutation error code indicating the nature of local/global conflict.
     * rcode: a repair error code indicating nature of the needed repair operation.
     */
    
    /* decl of shared local variables */
    char LocalPath[MAXPATHLEN], GlobalPath[MAXPATHLEN];
    ViceFid *fid, dummy;
    fsobj *ParentObj;
    int rc;

    /* initialization */
    *mcode = 0;
    *rcode = 0;
    strcpy(msg, "no conflict");
    fsobj *GlobalObjs[3];
    fsobj *LocalObjs[3];
    for (int i = 0; i < 3; i++) {
	GlobalObjs[i] = NULL;
	LocalObjs[i] = NULL;
    }
    vproc *vp = VprocSelf();
    vuid_t vuid = CRTORUID(vp->u.u_cred);
    
    switch (opcode) {
    case OLDCML_NewStore_OP:
	fid = &u.u_store.Fid;
	LOG(100, ("cmlent::CheckRepair: Store on 0x%x.%x.%x\n", fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: store target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: store target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching store target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching store target failed (%d)", rc);		
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));
	
	/* step 2: check mutation access rights */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	if (ParentObj->CheckAcRights(vuid, PRSFS_WRITE, 0) == EACCES) {
	    LOG(100, ("cmlent::CheckRepair: acl check failed\n"));
	    ParentObj->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
 
	/* step 3: check mutation semantic integrity, only check VV here */
	if (VV_Cmp(&(GlobalObjs[0]->stat.VV), &(LocalObjs[0]->stat.VV)) != VV_EQ) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers", GlobalPath);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_OVER_WRITE;
	}
	break;
    case OLDCML_Utimes_OP:
	fid = &u.u_utimes.Fid;
	LOG(100, ("cmlent::CheckRepair: Utimes on 0x%x.%x.%x\n", fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: utimes target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: utimes target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching utimes target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching utimes target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	if (ParentObj->CheckAcRights(vuid, PRSFS_WRITE, 0) == EACCES) {
	    LOG(100, ("cmlent::CheckRepair: acl check failed\n"));
	    ParentObj->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
 
	/* step 3: check mutation semantic integrity, only check VV here */
	if (VV_Cmp(&(GlobalObjs[0]->stat.VV), &(LocalObjs[0]->stat.VV)) != VV_EQ) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers", GlobalPath);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_OVER_WRITE;
	}
	break;
    case OLDCML_Chown_OP:
	fid = &u.u_chown.Fid;
	LOG(100, ("cmlent::CheckRepair: Chown on 0x%x.%x.%x\n", fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: chown target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: chown target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching chown target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching chown target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	if (ParentObj->CheckAcRights(vuid, PRSFS_ADMINISTER, 0) == EACCES) {
	    LOG(100, ("cmlent::CheckRepair: acl check failed\n"));
	    ParentObj->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
 
	/* step 3: check mutation semantic integrity, only check VV here */
	if (VV_Cmp(&(GlobalObjs[0]->stat.VV), &(LocalObjs[0]->stat.VV)) != VV_EQ) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers", GlobalPath);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_OVER_WRITE;
	}
	break;
    case OLDCML_Chmod_OP:
	fid = &u.u_chmod.Fid;
	LOG(100, ("cmlent::CheckRepair: Chmod on 0x%x.%x.%x\n", fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: chmod target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: chmod target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching chmod target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching chmod target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	OBJ_ASSERT(this, ParentObj = LRDB->GetGlobalParentObj(&GlobalObjs[0]->fid));
	if (ParentObj->CheckAcRights(vuid, PRSFS_WRITE, 0) == EACCES) {
	    LOG(100, ("cmlent::CheckRepair: acl check failed\n"));
	    ParentObj->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
 
	/* step 3: check mutation semantic integrity, only check VV here */
	if (VV_Cmp(&(GlobalObjs[0]->stat.VV), &(LocalObjs[0]->stat.VV)) != VV_EQ) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers", GlobalPath);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_OVER_WRITE;
	}
	break;
    case OLDCML_Create_OP:
	fid = &u.u_create.PFid;
	LOG(100, ("cmlent::CheckRepair: Create (%s) under 0x%x.%x.%x\n", (char *)u.u_create.Name,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: create parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: create parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching create parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching create parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_INSERT, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	}

	/* step 3: check mutation semantic integrity, only check name/name conflict here */
	if (GlobalObjs[0]->dir_Lookup((char *)u.u_create.Name, &dummy, CLU_CASE_SENSITIVE) == 0) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s/%s exist on servers", GlobalPath, (char *)u.u_create.Name);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	}
	break;
    case OLDCML_Link_OP:
	/* we need to check both the parent and the child here */
	fid = &u.u_link.PFid;
	LOG(100, ("cmlent::CheckRepair: Link %s under 0x%x.%x.%x\n", (char *)u.u_link.Name,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: link parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: link parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching link parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching link parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	fid = &u.u_link.CFid;
	LOG(100, ("cmlent::CheckRepair: Link %s to 0x%x.%x.%x\n", (char *)u.u_link.Name,
		  fid->Volume, fid->Vnode, fid->Unique));
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[1], &LocalObjs[1]);
	if (rc != 0) {
	    LocalObjs[1]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: link target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: link target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching link target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching link target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[1] != NULL));


	/* step2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_INSERT, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	}

	/* step 3: check mutation semantic integrity: only check name/name conflict here */
	if (GlobalObjs[0]->dir_Lookup((char *)u.u_link.Name, &dummy, CLU_CASE_SENSITIVE) == 0) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s/%s exist on servers\n", GlobalPath, 
		    (char *)u.u_link.Name);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	}
	break;	
    case OLDCML_SymLink_OP:
	fid = &u.u_symlink.PFid;
	LOG(100, ("cmlent::CheckRepair: Symlink (%s->%s) under 0x%x.%x.%x\n", (char *)u.u_symlink.NewName,
		  (char *)u.u_symlink.OldName, fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: symlink parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: symlink parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching symlink parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching symlink parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_INSERT, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	}

	/* step 3: check mutation semantic integrity, only check name/name conflict here */
	if (GlobalObjs[0]->dir_Lookup((char *)u.u_symlink.NewName, &dummy, CLU_CASE_SENSITIVE) == 0) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s/%s exist on servers", GlobalPath, (char *)u.u_symlink.NewName);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	}
	break;
    case OLDCML_MakeDir_OP:
	fid = &u.u_mkdir.PFid;
	LOG(100, ("cmlent::CheckRepair: Mkdir (%s) under 0x%x.%x.%x\n", (char *)u.u_mkdir.Name,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: mkdir parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: mkdir parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching mkdir parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching mkdir parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_INSERT, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	}

	/* step 3: check mutation semantic integrity, only check name/name conflict here */
	if (GlobalObjs[0]->dir_Lookup((char *)u.u_mkdir.Name, &dummy, CLU_CASE_SENSITIVE) == 0) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s/%s exist on servers", GlobalPath, (char *)u.u_mkdir.Name);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	}
	break;
    case OLDCML_Remove_OP:
	fid = &u.u_remove.PFid;
	LOG(100, ("cmlent::CheckRepair: Remove (%s) under 0x%x.%x.%x\n", (char *)u.u_remove.Name,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: remove parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: remove parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching remove parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching remove parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	fid = &u.u_remove.CFid;
	LOG(100, ("cmlent::CheckRepairObjects: Remove target 0x%x.%x.%x\n",
		  fid->Volume, fid->Vnode, fid->Unique));
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[1], &LocalObjs[1]);
	OBJ_ASSERT(this, (LocalObjs[1] == NULL) || DYING(LocalObjs[1]) ||
		         (LocalObjs[1]->stat.LinkCount > 0));
	if (rc != 0) {
	    LocalObjs[1]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: remove target %s no longer exits on servers", LocalPath);
		*rcode = 0; /* this is clearly a success */
		break;
	    case EINCONS:
		sprintf(msg, "conflict: remove target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching remove target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching remove target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[1] != NULL));

	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_DELETE, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}

	/* step 3: check mutation semantic integrity, only check remove/update conflict here */
	if (VV_Cmp(&(GlobalObjs[1]->stat.VV), &u.u_remove.CVV) != VV_EQ) {
	    GlobalObjs[1]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers", GlobalPath);
	    *mcode = MUTATION_RU_CONFLICT;
	    *rcode = REPAIR_FORCE_REMOVE;
	}	
	break;
    case OLDCML_RemoveDir_OP:
	fid = &u.u_rmdir.PFid;
	LOG(100, ("cmlent::CheckRepair: RemoveDir (%s) on 0x%x.%x.%x\n", (char *)u.u_rmdir.Name,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: rmdir parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: rmdir parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching rmdir parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching rmdir parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	fid = &u.u_rmdir.CFid;
	LOG(100, ("cmlent::CheckRepairObjects: RemoveDir target 0x%x.%x.%x\n",
		  fid->Volume, fid->Vnode, fid->Unique));

	rc = LRDB->FindRepairObject(fid, &GlobalObjs[1], &LocalObjs[1]);
	OBJ_ASSERT(this, (LocalObjs[1] == NULL) || DYING(LocalObjs[1]));
	if (rc != 0) {
	    LocalObjs[1]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: rmdir target %s no longer exits on servers", LocalPath);
		*rcode = 0; /* this is clearly a success */
		break;
	    case EINCONS:
		sprintf(msg, "conflict: rmdir target %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching rmdir target failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching rmdir target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[1] != NULL));


	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_DELETE, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}

	/* step 3: check mutation semantic integrity, only check remove/update conflict here */
	if (!(GlobalObjs[1]->dir_IsEmpty())) {
	    GlobalObjs[1]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s not empty on servers\n", GlobalPath);
	    *mcode = MUTATION_VV_CONFLICT;
	    *rcode = REPAIR_FAILURE;    
	    break;
	}
	if (VV_Cmp(&(GlobalObjs[1]->stat.VV), &u.u_rmdir.CVV) != VV_EQ) {
	    GlobalObjs[1]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s updated on servers\n", GlobalPath);
	    *mcode = MUTATION_RU_CONFLICT;
	    *rcode = REPAIR_FORCE_REMOVE;
	}	
	break;
    case OLDCML_Rename_OP:
	fid = &u.u_rename.SPFid;
	LOG(100, ("cmlent::CheckRepair: Rename (%s) from 0x%x.%x.%x\n", (char *)u.u_rename.OldName,
		  fid->Volume, fid->Vnode, fid->Unique));

	/* step 1: check mutation operand(s) */
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[0], &LocalObjs[0]);
	if (rc != 0) {
	    LocalObjs[0]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: rename source parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: rename source parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching rename source parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching rename source parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[0] != NULL));

	fid = &u.u_rename.TPFid;
	LOG(100, ("cmlent::CheckRepair: Rename (%s) to 0x%x.%x.%x\n", (char *)u.u_rename.NewName,
		  fid->Volume, fid->Vnode, fid->Unique));
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[1], &LocalObjs[1]);
	if (rc != 0) {
	    LocalObjs[1]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_PARENT;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: rename target parent %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: rename target parent %s in server/server conflict", LocalPath);
		break;
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching rename target parent failed (%d), please re-try!", rc);		
		break;
	    default:
		sprintf(msg, "fetching rename target parent failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[1] != NULL));

	fid = &u.u_rename.SFid;
	LOG(100, ("cmlent::CheckRepair: Rename target object 0x%x.%x.%x\n",
		  fid->Volume, fid->Vnode, fid->Unique));
	rc = LRDB->FindRepairObject(fid, &GlobalObjs[2], &LocalObjs[2]);
	if (rc != 0) {
	    LocalObjs[2]->GetPath(LocalPath, 1);
	    *mcode = MUTATION_MISS_TARGET;
	    *rcode = REPAIR_FAILURE;
	    switch (rc) {
	    case EIO:
	    case ENOENT:
		sprintf(msg, "conflict: rename target %s no longer exits on servers", LocalPath);
		break;
	    case EINCONS:
		sprintf(msg, "conflict: rename target %s in server/server conflict", LocalPath);
		break;
	    default:
	    case ESYNRESOLVE:
	    case EASYRESOLVE:
	    case ERETRY:
		sprintf(msg, "fetching rename target failed (%d), please re-try!", rc);	        
		break;
		sprintf(msg, "fetching rename target failed (%d)", rc);
	    }
	    break;
	}
	OBJ_ASSERT(this, (rc == 0) && (GlobalObjs[2] != NULL));

	/* step 2: check mutation access rights */
	if (GlobalObjs[0]->CheckAcRights(vuid, PRSFS_DELETE, 0) == EACCES) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on source parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
	if (GlobalObjs[1]->CheckAcRights(vuid, PRSFS_INSERT, 0) == EACCES) {
	    GlobalObjs[1]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: acl check failure on target parent %s", GlobalPath);
	    *mcode = MUTATION_ACL_FAILURE;
	    *rcode = REPAIR_FAILURE;
	    break;
	}

	/* step 3: check mutation semantic integrity */
	if (GlobalObjs[1]->dir_Lookup((char *)u.u_rename.NewName, &dummy, CLU_CASE_SENSITIVE) == 0) {
	    GlobalObjs[1]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: target %s/%s exist on servers\n", GlobalPath, 
		    (char *)u.u_rename.NewName);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	    break;
	}
	if (GlobalObjs[0]->dir_Lookup((char *)u.u_rename.OldName, &dummy, CLU_CASE_SENSITIVE) != 0) {
	    GlobalObjs[0]->GetPath(GlobalPath, 1);
	    sprintf(msg, "conflict: source %s/%s no longer exist on servers\n", GlobalPath, 
		    (char *)u.u_rename.OldName);
	    *mcode = MUTATION_NN_CONFLICT;
	    *rcode = REPAIR_FAILURE;
	}
	break;
    case OLDCML_Repair_OP:
	fid = &u.u_repair.Fid;
	LOG(0, ("cmlent::CheckRepair: Disconnected Repair on 0x%x.%x.%x\n",
		fid->Volume, fid->Vnode, fid->Unique));
	break;
    default:
	CHOKE("cmlent::CheckRepair: bogus opcode %d", opcode);
    }
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
    ViceFid *fid;
    fsobj *GObj, *LObj;
    fsobj *GPObj, *LPObj;
    char GlobalPath[MAXPATHLEN], LocalPath[MAXPATHLEN];
    switch (opcode) {
    case OLDCML_NewStore_OP:
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
	    LOG(100, ("cmlent::DoRepair: do store on 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));

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
    case OLDCML_Chmod_OP:
	{   
	    fid = &u.u_chmod.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    LOG(100, ("cmlent::DoRepair: do chmod on 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));
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
    case OLDCML_Chown_OP:
	{   fid = &u.u_chown.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do chown on 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));
	    vuid_t NewOwner = LObj->stat.Owner; 		/* use local new owner */
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
    case OLDCML_Utimes_OP:
	{   
	    fid = &u.u_chown.Fid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do utimes on 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));
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
    case OLDCML_Create_OP:
	{   /* do fid replacement after the creation succeeded. */
	    fid = &u.u_create.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do create on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_create.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "create semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsFile());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairCreate(&target, (char *)u.u_create.Name, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "create %s/%s succeeded", GlobalPath, (char *)u.u_create.Name);
		target->UnLock(WR);		/* release write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_create.CFid);
	    } else {
		sprintf(msg, "create %s/%s failed(%d)", GlobalPath, (char *)u.u_create.Name, code);
	    }
	    break;
	}
    case OLDCML_Link_OP:
	{   
	    fid = &u.u_link.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do link on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_link.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj && GObj->IsFile() && LObj->IsFile());
	    LOG(100, ("cmlent::DoRepair: do link on target 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));
	    code = GPObj->RepairLink((char *)u.u_link.Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "link %s/%s succeeded", GlobalPath, (char *)u.u_link.Name);
	    } else {
		sprintf(msg, "link %s/%s failed(%d)", GlobalPath, (char *)u.u_link.Name, code);
	    }
	    break;
	}
    case OLDCML_MakeDir_OP:
	{   
	    fid = &u.u_link.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do mkdir on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_mkdir.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "mkdir semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsDir());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairMkdir(&target, (char *)u.u_mkdir.Name, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "mkdir %s/%s succeeded", GlobalPath, (char *)u.u_mkdir.Name);
		target->UnLock(WR);			/* relese write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_mkdir.CFid);
	    } else {
		sprintf(msg, "mkdir %s/%s failed(%d)", GlobalPath, (char *)u.u_mkdir.Name, code);
	    }
	    break;
	}
    case OLDCML_SymLink_OP:
	{   
	    fid = &u.u_symlink.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do symlink on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_symlink.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != EIO && code != ENOENT) {
		sprintf(msg, "symlink semantic re-validation failed (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, LObj != NULL && LObj->IsSymLink());
	    unsigned short NewMode = LObj->stat.Mode;
	    fsobj *target = NULL;
	    code = GPObj->RepairSymlink(&target, (char *)u.u_symlink.NewName, 
					(char *)u.u_symlink.OldName, NewMode, FSDB->StdPri());
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "symlink %s/%s -> %s succeeded", GlobalPath, (char *)u.u_symlink.NewName,
			(char *)u.u_symlink.OldName);
		target->UnLock(WR);			/* relese write lock on the new object */
		LRDB->ReplaceRepairFid(&target->fid, &u.u_symlink.CFid);
	    } else {
		sprintf(msg, "symlink %s/%s -> %s failed(%d)", GlobalPath, (char *)u.u_symlink.NewName,
			(char *)u.u_symlink.OldName, code);
	    }
	    break;
	}
    case OLDCML_Remove_OP:
	{   
	    fid = &u.u_remove.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do remove on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_remove.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj) || LObj->stat.LinkCount > 0));
	    LOG(100, ("cmlent::DoRepair: do remove on global target 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique));
	    code = GPObj->RepairRemove((char *)u.u_remove.Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "remove %s/%s succeeded", GlobalPath, (char *)u.u_remove.Name);
	    } else {
		sprintf(msg, "remove %s/%s failed(%d)", GlobalPath, (char *)u.u_remove.Name, code);
	    }
	    break;
	}
    case OLDCML_RemoveDir_OP:
	{   
	    fid = &u.u_rmdir.PFid;
	    code = LRDB->FindRepairObject(fid, &GPObj, &LPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GPObj && LPObj && GPObj->IsDir() && LPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GPObj->fid.Volume, GPObj->fid.Vnode, GPObj->fid.Unique,
		      LPObj->fid.Volume, LPObj->fid.Vnode, LPObj->fid.Unique));
	    fid = &u.u_rmdir.CFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj != NULL && (LObj == NULL || DYING(LObj)) && GObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rmdir on global target 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique));
	    code = GPObj->RepairRmdir((char *)u.u_rmdir.Name, GObj);
	    GPObj->GetPath(GlobalPath, 1);
	    if (code == 0) {
		sprintf(msg, "rmdir %s/%s succeeded", GlobalPath, (char *)u.u_rmdir.Name);
	    } else {
		sprintf(msg, "rmdir %s/%s failed(%d)", GlobalPath, (char *)u.u_rmdir.Name, code);
	    }
	    break;
	}
    case OLDCML_Rename_OP:
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
	    LOG(100, ("cmlent::DoRepair: do rename on source parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GSPObj->fid.Volume, GSPObj->fid.Vnode, GSPObj->fid.Unique,
		      LSPObj->fid.Volume, LSPObj->fid.Vnode, LSPObj->fid.Unique));
	    fid = &u.u_rename.TPFid;
	    code = LRDB->FindRepairObject(fid, &GTPObj, &LTPObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GTPObj && LTPObj && GTPObj->IsDir() && LTPObj->IsDir());
	    LOG(100, ("cmlent::DoRepair: do rename on target parent 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GTPObj->fid.Volume, GTPObj->fid.Vnode, GTPObj->fid.Unique,
		      LTPObj->fid.Volume, LTPObj->fid.Vnode, LTPObj->fid.Unique));
	    fid = &u.u_rename.SFid;
	    code = LRDB->FindRepairObject(fid, &GObj, &LObj);
	    if (code != 0) {
		sprintf(msg, "can not obtain global mutation objects (%d)", code);
		break;
	    }
	    OBJ_ASSERT(this, GObj && LObj);
	    LOG(100, ("cmlent::DoRepair: do rename on source object 0x%x.%x.%x and 0x%x.%x.%x\n",
		      GObj->fid.Volume, GObj->fid.Vnode, GObj->fid.Unique,
		      LObj->fid.Volume, LObj->fid.Vnode, LObj->fid.Unique));
	    code = GTPObj->RepairRename(GSPObj, (char *)u.u_rename.OldName,
					GObj, (char *)u.u_rename.NewName,
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
		sprintf(msg, "rename %s/%s -> %s/%s succeeded", SPath, (char *)u.u_rename.OldName,
			TPath, (char *)u.u_rename.NewName);
	    } else {
		sprintf(msg, "rename %s/%s -> %s/%s failed(%d)", SPath, (char *)u.u_rename.OldName,
			TPath, (char *)u.u_rename.NewName, code);
	    }
	    break;
	}
    case OLDCML_Repair_OP:
	{
	    fid = &u.u_repair.Fid;
	    LOG(0, ("cmlent::DoRepair: Disconnected Repair on 0x%x.%x.%x\n",
		    fid->Volume, fid->Vnode, fid->Unique));
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
    OBJ_ASSERT(this, msg);
    switch (opcode) {
    case OLDCML_NewStore_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_store.Fid, log, this);
		sprintf(msg, "store %s", path);
		break;
	}
    case OLDCML_Chmod_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_chmod.Fid, log, this);
		sprintf(msg, "chmod %s", path);
		break;
	}
    case OLDCML_Chown_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_chown.Fid, log, this);
		sprintf(msg, "chown %s", path);
		break;
	}
    case OLDCML_Utimes_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_utimes.Fid, log, this);
		sprintf(msg, "setattr %s", path);
		break;
	}
    case OLDCML_Create_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_create.CFid, log, this);
		sprintf(msg, "create %s", path);
		break;
	}
    case OLDCML_Link_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_link.CFid, log, this);
		sprintf(msg, "link %s", path);
		break;
	}
    case OLDCML_MakeDir_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_mkdir.CFid, log, this);
		sprintf(msg, "mkdir %s", path);
		break;
	}
    case OLDCML_SymLink_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_symlink.CFid, log, this);
		sprintf(msg, "symlink %s --> %s", path, (char *)u.u_symlink.NewName);
		break;
	}
    case OLDCML_Remove_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_remove.CFid, log, this);
		sprintf(msg, "remove %s", path);
		break;
	}
    case OLDCML_RemoveDir_OP:
	{	char path[MAXPATHLEN];
		RecoverPathName(path, &u.u_rmdir.CFid, log, this);
		sprintf(msg, "rmdir %s", path);
		break;
	}
    case OLDCML_Rename_OP:
	{	char sppath[MAXPATHLEN];
		char tppath[MAXPATHLEN];
		RecoverPathName(sppath, &u.u_rename.SPFid, log, this);
		RecoverPathName(tppath, &u.u_rename.TPFid, log, this);
		sprintf(msg, "rename %s/%s -> %s/%s", sppath, (char *)u.u_rename.OldName, 
			tppath, (char *)u.u_rename.NewName);
		break;
	}
    case OLDCML_Repair_OP:
	{	ViceFid *fid = &u.u_repair.Fid;
		LOG(0, ("cmlent::GetLocalOpMsg: Disconnected Repair on 0x%x.%x.%x\n",
			fid->Volume, fid->Vnode, fid->Unique));
		sprintf(msg, "disconnected repair on 0x%lx.%lx.%lx",
 			fid->Volume, fid->Vnode, fid->Unique);
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
int cmlent::InLocalRepairSubtree(ViceFid *LocalRootFid)
{
    /*
     * check whether the objects mutated by this cmlent belongs to
     * the subtree rooted at the object whose fid equals RootFid.
     */
    OBJ_ASSERT(this, LocalRootFid && FID_VolIsLocal(LocalRootFid));
    LOG(100, ("cmlent::InLocalRepairSubtree: LocalRootFid = 0x%x.%x.%x\n",
	      LocalRootFid->Volume, LocalRootFid->Vnode, LocalRootFid->Unique));
    ViceFid *Fids[3];
    fsobj *OBJ;
    ViceVersionVector *VVs[3];
    GetVVandFids(VVs, Fids);

    for (int i = 0; i < 3; i++) {
	if (!Fids[i]) continue;
	if (!FID_VolIsLocal(Fids[i])) continue;
	OBJ_ASSERT(this, OBJ = FSDB->Find(Fids[i]));
	if (OBJ->IsAncestor(LocalRootFid)) return 1;
    }
    
    return 0;
}

/* need not be called from within a transaction */
int cmlent::InGlobalRepairSubtree(ViceFid *GlobalRootFid)
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
    OBJ_ASSERT(this, GlobalRootFid && !FID_VolIsLocal(GlobalRootFid));
    LOG(100, ("cmlent::InGlobalRepairSubtree: GlobalRootFid = 0x%x.%x.%x\n",
	GlobalRootFid->Volume, GlobalRootFid->Vnode, GlobalRootFid->Unique));

    if (flags.to_be_repaired || flags.repair_mutation) {
	LOG(100, ("cmlent::InGlobalRepairSubtree: repair flag(s) set already\n"));
	return 0;
    }

    ViceFid *Fids[3];
    ViceVersionVector *VVs[3];
    GetVVandFids(VVs, Fids);
    fsobj *OBJ;

    for (int i = 0; i < 3; i++) {
	if (!Fids[i]) continue;
	if (FID_VolIsLocal(Fids[i])) break;
	OBJ = FSDB->Find(Fids[i]);
	if (!OBJ) continue;
	if (OBJ->IsAncestor(GlobalRootFid)) return 1;
    }
    return 0;
}

/* need not be called from within a transaction */
void cmlent::GetVVandFids(ViceVersionVector *vvs[], ViceFid *fids[]) 
{
    fids[0] = 0; fids[1] = 0; fids[2] = 0;
    vvs[0] = 0; vvs[1] = 0; vvs[2] = 0;
    switch(opcode) {
        case OLDCML_NewStore_OP:
	    fids[0] = &u.u_store.Fid;
	    vvs[0] = &u.u_store.VV;
	    break;

	case OLDCML_Utimes_OP:
	    fids[0] = &u.u_utimes.Fid;
	    vvs[0] = &u.u_utimes.VV;
	    break;

	case OLDCML_Chown_OP:
	    fids[0] = &u.u_chown.Fid;
	    vvs[0] = &u.u_chown.VV;
	    break;

	case OLDCML_Chmod_OP:
	    fids[0] = &u.u_chmod.Fid;
	    vvs[0] = &u.u_chmod.VV;
	    break;

	case OLDCML_Create_OP:
	    fids[0] = &u.u_create.PFid;
	    vvs[0] = &u.u_create.PVV;
	    fids[1] = &u.u_create.CFid;
	    break;

	case OLDCML_Remove_OP:
	    fids[0] = &u.u_remove.PFid;
	    vvs[0] = &u.u_remove.PVV;
	    fids[1] = &u.u_remove.CFid;
	    vvs[1] = &u.u_remove.CVV;
	    break;

	case OLDCML_Link_OP:
	    fids[0] = &u.u_link.PFid;
	    vvs[0] = &u.u_link.PVV;
	    fids[1] = &u.u_link.CFid;
	    vvs[1] = &u.u_link.CVV;
	    break;

	case OLDCML_Rename_OP:
	    fids[0] = &u.u_rename.SPFid;
	    vvs[0] = &u.u_rename.SPVV;
	    fids[1] = &u.u_rename.SFid;
	    vvs[1] = &u.u_rename.SVV;
	    if (!FID_EQ(&u.u_rename.SPFid, &u.u_rename.TPFid)) {
		fids[2] = &u.u_rename.TPFid;
		vvs[2] = &u.u_rename.TPVV;
	    }
	    break;

	case OLDCML_MakeDir_OP:
	    fids[0] = &u.u_mkdir.PFid;
	    vvs[0] = &u.u_mkdir.PVV;
	    fids[1] = &u.u_mkdir.CFid;
	    break;

	case OLDCML_RemoveDir_OP:
	    fids[0] = &u.u_rmdir.PFid;
	    vvs[0] = &u.u_rmdir.PVV;
	    fids[1] = &u.u_rmdir.CFid;
	    vvs[1] = &u.u_rmdir.CVV;
	    break;

	case OLDCML_SymLink_OP:
	    fids[0] = &u.u_symlink.PFid;
	    vvs[0] = &u.u_symlink.PVV;
	    fids[1] = &u.u_symlink.CFid;
	    break;

        case OLDCML_Repair_OP:
	    fids[0] = &u.u_repair.Fid;
	    break;
	default:
	    CHOKE("cmlent::GetVVandFids: bogus opcode (%d)", opcode);
    }
}

/* need not be called from within a transaction */
void cmlent::GetAllFids(ViceFid *fids[]) 
{
    fids[0] = 0; fids[1] = 0; fids[2] = 0;
    switch(opcode) {
        case OLDCML_NewStore_OP:
	    fids[0] = &u.u_store.Fid;
	    break;

	case OLDCML_Utimes_OP:
	    fids[0] = &u.u_utimes.Fid;
	    break;

	case OLDCML_Chown_OP:
	    fids[0] = &u.u_chown.Fid;
	    break;

	case OLDCML_Chmod_OP:
	    fids[0] = &u.u_chmod.Fid;
	    break;

	case OLDCML_Create_OP:
	    fids[0] = &u.u_create.PFid;
	    fids[1] = &u.u_create.CFid;
	    break;

	case OLDCML_Remove_OP:
	    fids[0] = &u.u_remove.PFid;
	    fids[1] = &u.u_remove.CFid;
	    break;

	case OLDCML_Link_OP:
	    fids[0] = &u.u_link.PFid;
	    fids[1] = &u.u_link.CFid;
	    break;

	case OLDCML_Rename_OP:
	    fids[0] = &u.u_rename.SPFid;
	    fids[1] = &u.u_rename.SFid;
	    fids[2] = &u.u_rename.TPFid;
	    break;

	case OLDCML_MakeDir_OP:
	    fids[0] = &u.u_mkdir.PFid;
	    fids[1] = &u.u_mkdir.CFid;
	    break;

	case OLDCML_RemoveDir_OP:
	    fids[0] = &u.u_rmdir.PFid;
	    fids[1] = &u.u_rmdir.CFid;
	    break;

	case OLDCML_SymLink_OP:
	    fids[0] = &u.u_symlink.PFid;
	    fids[1] = &u.u_symlink.CFid;
	    break;

        case OLDCML_Repair_OP:
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
    ViceFid *Fids[3];
    GetAllFids(Fids);
    for (int i = 0; i < 3; i++) {
	if (Fids[i] == NULL) continue;
	if (FID_VolIsLocal(Fids[i])) {
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
