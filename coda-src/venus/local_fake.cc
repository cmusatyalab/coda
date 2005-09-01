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

/** \file
 *  \brief Code to handle object expansion.
 *
 * This is a generalized implementation that will (eventually) deal with
 * all the details for expanding conflicting object both for local-global and
 * server-server conflict repair.
 *
 * It might at some point even be useful for accessing backup copies of objects
 * even when they are not in conflict.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <struct.h>

#ifdef __cplusplus
}
#endif

#include "fso.h"
#include "venusvol.h"
#include "worker.h"

/// Expand an object (typically a conflict).
/**
 * We're not really doing much to the object itself because we want it to stay
 * visible as the '_localcache' copy in the expanded tree.
 *
 * Expansion involves the following steps,
 * -# Create a fake directory in the repair volume (Repair@localhost).
 *    Populate this directory with mountlinks pointing at the current object
 *    and the individual server replicas.
 * -# Identify our parent, if we are a mountpoint the parent is the directory
 *    that contains the mountlink that we are covering.
 * -# Create a fake mountlink object that exists in the same volume as our
 *    parent. Point this mountlink at the fake repair directory.
 * -# Disconnect the current object (or our mountlink) from the parent
 *    directory.
 * -# Replace the directory entry in our parent with a reference to the fake
 *    mountlink.
 */

int fsobj::ExpandObject(void)
{
    fsobj *mod_fso, *fakedir;
    char name[CODA_MAXNAMLEN+1];
    VenusFid dirfid;
    int isroot;

    /* do not expand an already expanded object, technically not a problem
     * but it is kind of pointless and would complicate collapsing slightly */
    if (IsExpandedObj())
	return EINPROGRESS;

    /* ignore local objects which can't be meaningfully expanded */
    if(IsLocalObj())
      return EINVAL;

    isroot = IsRoot();

    /* disconnect from our parent */
    mod_fso = isroot ? u.mtpoint : pfso;
    CODA_ASSERT(mod_fso);

    if(mod_fso->IsExpandedObj()) /* Don't allow replica/recursive expansion */
      return EINPROGRESS;

    Recov_BeginTrans();

    RVMLIB_REC_OBJECT(*this);

    if (isroot) {
	FSO_ASSERT(this, this == mod_fso->u.root);
	strcpy(name,comp);
	UnmountRoot();
	mod_fso->UncoverMtPt();
    } else {
	mod_fso->dir_LookupByFid(name, &fid);
	mod_fso->dir_Delete(name);
	mod_fso->DetachChild(this);
	pfso = NULL;
    }

    /* create a fake directory */
    {
	volent *repairvol = VDB->Find(LocalRealm, "Repair");
	fakedir = repairvol->NewFakeDirObj(name);
	dirfid = fakedir->fid;
	VDB->Put(&repairvol);
    }

    /* add an entry for ourselves */
    {
	fsobj *fakelink;
	char *name = LOCALCACHE;

	if(IsFake() || !HAVEALLDATA(this))
	  name = LOCALCACHE_HIDDEN;

	fakelink = fakedir->vol->NewFakeMountLinkObj(&fid, name);
	fakelink->pfso = fakedir;
	fakelink->pfid = fakedir->fid;

	fakelink->u.root = this;

	fakedir->dir_Create(name, &fakelink->fid);

	fakedir->AttachChild(fakelink);
	LOG(10, ("fsobj::ExpandObject: new entry (%s, %s) -> %s\n", name,
		FID_(&fakelink->fid), FID_(&fid)));
	FSDB->Put(&fakelink);
    }

    { /* Make entries for each of the rw-replicas. */
	struct in_addr volumehosts[VSG_MEMBERS];
	VolumeId volumeids[VSG_MEMBERS];
	repvol *vp = (repvol *)vol;
	VenusFid replicafid = fid;
	fsobj *fakelink;

	vp->GetHosts(volumehosts);
	vp->GetVids(volumeids);
	for (int i = 0; i < VSG_MEMBERS; i++) {
	    if (!volumehosts[i].s_addr) continue;
	    srvent *s = FindServer(&volumehosts[i]);
	    CODA_ASSERT(s != NULL);

	    replicafid.Volume = volumeids[i];
	    fakelink = fakedir->vol->NewFakeMountLinkObj(&replicafid, s->name);
	    fakelink->pfso = fakedir;
	    fakelink->pfid = fakedir->fid;
	    fakedir->dir_Create(s->name, &fakelink->fid);
	    fakedir->AttachChild(fakelink);
	    LOG(10, ("fsobj::ExpandObject: new entry (%s, %s) -> %s\n",
		    s->name, FID_(&fakelink->fid), FID_(&replicafid)));
	    FSDB->Put(&fakelink);
	}
    }

    /* create a link pointing at the fake directory we just populated */
    if (isroot) {
	RVMLIB_REC_OBJECT(*mod_fso);
	mod_fso->DiscardData();
	mod_fso->SetMtLinkContents(&fakedir->fid);
	LOG(10, ("volent::ExpandObject: changed mountlink to %s -> %s\n",
	    FID_(&mod_fso->fid), mod_fso->data.symlink));
    } else {
	fsobj *fakelink;
	fakelink = mod_fso->vol->NewFakeMountLinkObj(&fakedir->fid, name);

	/* attach the new fake mountlink to the old parent directory */
	fakelink->pfso = mod_fso;
	fakelink->pfid = mod_fso->fid;
	mod_fso->dir_Create(name, &fakelink->fid);
	mod_fso->AttachChild(fakelink);
	FSDB->Put(&fakelink);
    }

    RVMLIB_REC_OBJECT(mod_fso->flags);
    mod_fso->flags.modified = 1;
    mod_fso->flags.local = 1;
    flags.expanded = 1;
    flags.local = 1; /* so we don't get Kill()ed on a collapse */

    fakedir->Matriculate();

    /* mark any CML entries related to ourselves 'expanded' */
    ExpandCMLEntries();

    FSO_HOLD(this);

    FSDB->Put(&fakedir);
    Recov_EndTrans(DMFP);

    /* make sure we tell the kernel about the changes */
    k_Purge(&mod_fso->fid, 1);
    k_Purge(&fid, 0);

    return 0;
}

/// Instantiate a new fake directory object
fsobj *volent::NewFakeDirObj(char *comp)
{
    VenusFid fakefid;
    fsobj *dir;

    fakefid = GenerateFakeFid();
    dir = new (FROMFREELIST, 25000) fsobj(&fakefid, comp);

    /* Initialize the new object */
    RVMLIB_REC_OBJECT(*dir);
    dir->stat.DataVersion = 1;
    dir->stat.Owner = V_UID;
    dir->stat.Date = Vtime();
    dir->stat.Mode = 0555;
    dir->stat.LinkCount = 1;
    dir->stat.VnodeType = Directory;
    dir->flags.local = 1;   /* because it is in the repair volume */

    dir->dir_MakeDir();

    LOG(10, ("volent::NewFakeDirObj: made a directory %s\n", FID_(&fakefid)));

    return dir;
}

/// Instantiate a new fake mountlink object
fsobj *volent::NewFakeMountLinkObj(VenusFid *fid, char *comp)
{
    VenusFid fakefid;
    fsobj *link;

    fakefid = GenerateFakeFid();
    link = new (FROMFREELIST, 25000) fsobj(&fakefid, comp);

    /* Initialize the new object */
    RVMLIB_REC_OBJECT(*link);
    link->stat.DataVersion = 1;
    link->stat.Owner = V_UID;
    link->stat.Date = Vtime();
    link->stat.Mode = 0644;
    link->stat.LinkCount = 1;
    link->stat.VnodeType = SymbolicLink;
    link->flags.local = 1;

    link->SetMtLinkContents(fid);

    LOG(10, ("volent::NewFakeMountLinkObj: made a mountlink %s -> %s\n",
	    FID_(&fakefid), link->data.symlink));

    UpdateCacheStats(&FSDB->FileDataStats, CREATE, BLOCKS(link));

    link->Matriculate();

    return link;
}

/// Set the (fake) symlink contents for mount-links */
/* MUST be called from within a transaction */
void fsobj::SetMtLinkContents(VenusFid *fid)
{
    Realm *realm;
    const char *realmname;

    FSO_ASSERT(this, !data.havedata);

    realm = REALMDB->GetRealm(fid->Realm);
    realmname = realm->Name();

    stat.Length = 29 + strlen(realmname);
    data.symlink = (char *)rvmlib_rec_malloc(stat.Length+1);
    rvmlib_set_range(data.symlink, stat.Length+1);
    sprintf(data.symlink, "@%08x.%08x.%08x@%s.",
	    fid->Volume, fid->Vnode, fid->Unique, realmname);
    realm->PutRef();
}

/// Collapse an expanded object.
/**
 * Reverse the expansion, interestingly this function will be called on the
 * fake local object that has replaced the original.
 */
int fsobj::CollapseObject(void)
{
    fsobj *mtlink, *link_fso = NULL, *localcache = NULL, *mod_fso = NULL;
    int rc;

    /* We could be called either on a modified mountlink, a fake mountlink in
     * the parent volume, the fake expansion directory, the _localcache fake
     * link in the expanded directory, or on the expanded object itself.
     * We probably should 'normalize' this so that any possible call will end
     * up on the expanded fake directory object */

    if(!IsLocalObj()) {
      LOG(0, ("fsobj::CollapseObject: (%s) not an expanded object\n",
	      FID_(&fid)));
      return EINVAL;
    }

    if(!IsDir() || !vol->IsRepairVol()) {
      /* refocus the collapse, if possible (crawl towards the fake dir) */
      if(IsMTLink()) {
	CODA_ASSERT(pfso);
	if(pfso->IsLocalObj()) {
	  /* replica mountpoint */
	  CODA_ASSERT(pfso);
	  LOG(10, ("fsobj::CollapseObject: (%s) is a replica mountlink, refocusing collapse on 'fake' parent directory\n", FID_(&fid)));
	  return pfso->CollapseObject();
	}

	if (TryToCover(NULL, V_UID) == 0) {
	  LOG(10, ("fsobj::CollapseObject: (%s) is a expanded directory's mountlink, refocusing collapse on its root\n", FID_(&fid)));
	  return u.root->CollapseObject();
	}
	LOG(0, ("fsobj::CollapseObject: (%s) unable to find expanded directory\n", FID_(&fid)));
	return EIO;
      }
      else {
	/* replica (local or global) */
	if (IsMtPt()) {
	    CODA_ASSERT(u.mtpoint);
	    LOG(10, ("fsobj::CollapseObject: (%s) is a replica, refocusing on the replica's mountpoint\n", FID_(&fid)));
	    return u.mtpoint->CollapseObject();
	}
	LOG(0, ("fsobj::CollapseObject: (%s) unable to find expanded parent\n", FID_(&fid)));
	return EIO;
      }
    }

    /* find the expanded object, covering mountpoint if necessary */
    vproc *vp = VprocSelf();

    rc = Lookup(&localcache, NULL, LOCALCACHE, vp->u.u_uid,
		CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT, 1);
    if(rc && !localcache)
      rc = Lookup(&localcache, NULL, LOCALCACHE_HIDDEN, vp->u.u_uid,
		  CLU_CASE_SENSITIVE | CLU_TRAVERSE_MTPT, 1);
    if(rc || !localcache) {
      LOG(0, ("fsobj::CollapseObject: Lookup failed for LOCALCACHE:%d\n", rc));
      return rc;
    }

    LOG(10, ("fsobj::CollapseObject: Fake directory (%s) collapse attempted, LOCALCACHE is %s\n", FID_(&fid), FID_(&localcache->fid)));

    Recov_BeginTrans();

    /* detach the fake mountlink */
    mtlink = u.mtpoint;
    UnmountRoot();
    mtlink->UncoverMtPt();

    link_fso = localcache->u.mtpoint;
    CODA_ASSERT(localcache->IsRoot());
    CODA_ASSERT(link_fso);

    RVMLIB_REC_OBJECT(*localcache);

    localcache->UnmountRoot();
    link_fso->UncoverMtPt();

    /* reattach things */
    if (mtlink->vol != localcache->vol) {
        /* sort of rebuild the mountlink we destroyed in fsobj::Expand() */
        RVMLIB_REC_OBJECT(*mtlink);
	mtlink->DiscardData();
	mtlink->SetMtLinkContents(&localcache->fid);
	mtlink->data.symlink[0] = '$';
	mod_fso = mtlink;
    } else {
	mod_fso = mtlink->pfso;
	CODA_ASSERT(mod_fso);
	RVMLIB_REC_OBJECT(*mod_fso);
	mod_fso->dir_Delete(mtlink->comp);
	mod_fso->DetachChild(mtlink);
	mtlink->pfso = NULL;
	mtlink->pfid = NullFid;

	mod_fso->dir_Create(mtlink->comp, &localcache->fid);
	mod_fso->AttachChild(localcache);
	localcache->pfso = mod_fso;

	/* kill the mountlink to our fake directory */
	mtlink->Kill();
    }

    mod_fso->flags.modified = 0;
    mod_fso->flags.local = 0;
    localcache->flags.expanded = 0;
    localcache->flags.local = 0;

    localcache->CollapseCMLEntries();

    /* kill the expanded directory and its descendants */
    Kill();

    FSO_RELE(localcache);
    /* XXX: We seem to be losing a temporary reference
     * to localcache when we shutdown and restart venus.
     * It could be that the FSO_HOLD in ExpandObject
     * is not recorded in RVM and gets lost. This conditional
     * may not fix every case of this problem. -AW */
    Recov_EndTrans(DMFP);

    FSDB->Put(&localcache);
    return rc;
}

/* local-global conflict detection code */
int fsobj::IsToBeRepaired(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;
    
    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;
      
      CODA_ASSERT(m);
      
      /* If a cmlent is bound to an "expanded" fsobj, we need to allow
       * access to its data and can't return a conflict. This situation occurs
       * when we have a directory conflict and expand it, but the _localcache
       * replica's children show up as inconsistent due to bindings. */

      if (m->IsToBeRepaired())
	return 1;

      /* else this cmlent is not in conflict */
    }
  }
  return 0;
}

/* This function finds the uid within the first broken cmlent associated with
 * this fsobj, for use in launching ASRs. */

uid_t fsobj::WhoIsLastAuthor(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;
    
    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;
      
      CODA_ASSERT(m);
      
      /* If a cmlent is bound to an "expanded" fsobj, we need to allow
       * access to its data and can't return a conflict. This situation occurs
       * when we have a directory conflict and expand it, but the _localcache
       * replica's children show up as inconsistent due to bindings. */

      if (m->IsToBeRepaired())
	return m->uid;

      /* else this cmlent is not in conflict */
    }
  }
  return (uid_t)-1;
}

/* must be called from within transaction */
void fsobj::ExpandCMLEntries(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;

    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;

      CODA_ASSERT(m);

      RVMLIB_REC_OBJECT(*m);
      m->expansions++;
    }
  }
}

/* must be called from within transaction */
void fsobj::CollapseCMLEntries(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;

    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;

      CODA_ASSERT(m);

      RVMLIB_REC_OBJECT(*m);

      if(m->expansions > 0)
	m->expansions--;
    }
  }
}

/* need not be called from within transaction */
int fsobj::HasExpandedCMLEntries(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;

    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;

      CODA_ASSERT(m);

      if(m->expansions > 0)
	return 1;
    }
  }

  return 0;
}
