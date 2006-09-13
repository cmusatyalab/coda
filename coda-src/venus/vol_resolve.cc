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
 *
 *    Implementation of the Venus Resolve facility.
 *
 *    ToDo:
 *       1. Simplify.  There is no need to Mwait on an array of fids! (waiting on a single fid will suffice)
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <struct.h>

#include <unistd.h>
#include <stdlib.h>

/* interfaces */
#include <vice.h>
#ifdef __cplusplus
}
#endif


/* from util */
#include <olist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"


#ifdef VENUSDEBUG
int resent::allocs = 0;
int resent::deallocs = 0;
#endif

struct resolve_node {
  struct resolve_node *next;
  struct resolve_node *prev;
  VenusFid HintFid;
};

void repvol::Resolve()
{
    LOG(0, ("repvol::Resolve: %s\n", name));
    MarinerLog("resolve::%s\n", name);

    fsobj *f;
    int code = 0, nonhint = 0, firsttime = 1;
    vproc *v = VprocSelf();
    struct resolve_node *head, *cur, *hint;

    Volid volid;
    volid.Realm = realm->Id();
    volid.Volume = vid;

    /* Set up resolve list */

    if((head = (struct resolve_node *)malloc(sizeof(struct resolve_node)))
       == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }

    /* hint serves as a blank node that records the hinted fid if necessary. */
    if((hint = (struct resolve_node *)malloc(sizeof(struct resolve_node)))
       == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }

    /* cur always holds the current fid under resolution. */
    cur = head;

    /* Set up doubly linked list. */
    cur->prev = NULL;
    cur->next = hint;
    hint->prev = cur;
    hint->next = NULL;

    /* Grab control of the volume. */
    v->Begin_VFS(&volid, CODA_RESOLVE);
    VOL_ASSERT(this, v->u.u_error == 0);

    /* check if someone else already resolved the volume while we were
     * waiting to enter the volume */
    if (state != Resolving) {
	code = EALREADY;
	goto Exit;
    }

    /* Flush all COP2 entries. */
    /* This would NOT be necessary if ViceResolve took a "PiggyCOP2" parameter! -JJK */
    code = FlushCOP2();
    if (code != 0) goto Exit;

    resent *r;
    while ((r = (resent *)res_list->get())) {
	mgrpent *m = 0;
	connent *c = 0;

	if(firsttime) {
	  firsttime = 0;

	  /* Set head of list to original fid parameter. */
	  cur->HintFid.Realm = r->fid.Realm;
	  cur->HintFid.Volume = r->fid.Volume;
	  cur->HintFid.Vnode = r->fid.Vnode;
	  cur->HintFid.Unique = r->fid.Unique;

	  /* Necessary initialization for FID_EQ. */
	  hint->HintFid.Realm = r->fid.Realm;
	}

	{
	    /* Get an Mgroup. */
	    code = GetMgrp(&m, V_UID);
	    if (code != 0) goto HandleResult;

	    /* Pick a coordinator and get a connection to it. */
	    struct in_addr *phost = m->GetPrimaryHost();
	    CODA_ASSERT(phost->s_addr != 0);
	    srvent *s = GetServer(phost, GetRealmId());
	    code = s->GetConn(&c, ANYUSER_UID);
	    PutServer(&s);
	    if (code != 0) goto HandleResult;

	    /* Make the RPC call. */
	    if(!nonhint) {
	      MarinerLog("store::ResolveHinted (%s)\n", FID_(&r->fid));
	      UNI_START_MESSAGE(ViceResolveHinted_OP);
	      code = ViceResolveHinted(c->connid, MakeViceFid(&r->fid),
				       MakeViceFid(&(hint->HintFid)));
	      UNI_END_MESSAGE(ViceResolveHinted_OP);
	      MarinerLog("store::resolvehinted done\n");

	      /* Examine the return code to decide what to do next. */
	      code = Collate(c, code);
	      UNI_RECORD_STATS(ViceResolveHinted_OP);
	    }
	    if(nonhint || (code == EOPNOTSUPP)) {

	      if(code == EOPNOTSUPP)
		LOG(0, ("repvol::Resolve: Server doesn't support "
			"ResolveHints!\n"));

	    MarinerLog("store::Resolve (%s)\n", FID_(&r->fid));
	    UNI_START_MESSAGE(ViceResolve_OP);
	    code = ViceResolve(c->connid, MakeViceFid(&r->fid));
	    UNI_END_MESSAGE(ViceResolve_OP);
	    MarinerLog("store::resolve done\n");

	    /* Examine the return code to decide what to do next. */
	    code = Collate(c, code);
	    UNI_RECORD_STATS(ViceResolve_OP);
	}

	}

	/* Demote the object (if cached) */
	f = FSDB->Find(&r->fid);
	if (f) f->Demote();

	if (code == ERESHINT) {
	  struct resolve_node *trav;

	  /* First, check to see if the hinted fid is bad. */

	  if((hint->HintFid.Volume == (unsigned int)NULL) &&
	     (hint->HintFid.Vnode == (unsigned int)NULL) &&
	     (hint->HintFid.Unique == (unsigned int)NULL)) {

	    /* The server is out of hints. Submit original node
	     * for non-hinted resolution, which will mark everything
	     * from original resolution in conflict. */

	    nonhint = 1;
	    ResSubmit(NULL, &(head->HintFid));
	    goto HandleResult;
	  }

	  /* Look to see if we've already tried to resolve the hinted fid. */

	  for(trav = head; ((trav != NULL) && (trav != hint));
	      trav=trav->next) {
	    if(FID_EQ(&(trav->HintFid), &(hint->HintFid))) {

	      /* We have created a resolution loop in the file system
	       * hierarchy. Submit original node
	       * for non-hinted resolution, which will mark everything
	       * from original resolution in conflict. */

	      LOG(0,("Resolve: Recursive resolve failed, resubmitting "
		      "original object for non-hinted resolution.\n"));

	      nonhint = 1;
	      ResSubmit(NULL, &(head->HintFid));
	      goto HandleResult;
	    }
	  }
	  if(trav == hint) {
	    /* General hint case. Add the hinted fid to the list,
	     * to be resolved if the hinted resolution succeeds. */

	    LOG(0,("Resolve: Submitting rename source for resolution\n"));
	    ResSubmit(NULL, &(hint->HintFid));

	    cur = hint;
	    if((hint = (struct resolve_node *)
		malloc(sizeof(struct resolve_node))) == NULL) {
	      perror("malloc");
	      exit(EXIT_FAILURE);
	    }

	    cur->next = hint;
	    hint->next = NULL;
	    hint->prev = cur;

	    /* Necessary initialization for FID_EQ. */
	    hint->HintFid.Realm = cur->HintFid.Realm;
	  }

	  /* If this succeeds, we will resubmit the failed node for
	   * resolution. */
	}

	if (code == VNOVNODE) {
	    VenusFid *pfid;
	    if (f) {
		pfid = &(f->pfid);
		if (!FID_EQ(pfid, &NullFid) && FID_VolEQ(pfid, &r->fid) &&
		    (pfid->Vnode != r->fid.Vnode) ) {

		    LOG(10,("Resolve: Submitting parent for resolution\n"));
		    ResSubmit(NULL, pfid);

		    /* Retrying resolve on parent is also a "hint" of sorts.
		     * Make sure to add it into the hint linked list to
		     * avoid possible cycles. */

		    cur = hint; /* Move up a node. */

		    /* Remember the pfid. */
		    cur->HintFid.Realm = pfid->Realm;
		    cur->HintFid.Volume = pfid->Volume;
		    cur->HintFid.Vnode = pfid->Vnode;
		    cur->HintFid.Unique = pfid->Unique;

		    if((hint = (struct resolve_node *)
			malloc(sizeof(struct resolve_node))) == NULL) {
		      perror("malloc");
		      exit(EXIT_FAILURE);
		    }

		    cur->next = hint;
		    hint->next = NULL;
		    hint->prev = cur;

		    /* Necessary initialization for FID_EQ. */
		    hint->HintFid.Realm = cur->HintFid.Realm;

		    /* We shouldn't resubmit ourselves as this might lead to an
		     * endless loop. Hopefully the hoard/getattr that triggered
		     * the resolution will loop around and retry. --JH */
		    // ResSubmit(NULL, r->fid);
		}
	    } else
		LOG(10,("Resolve: Couldn't find current object\n"));
	    goto HandleResult;
	}

	if(!code && !nonhint && cur && hint && (cur->prev != NULL)) {
	  struct resolve_node *freeme = cur;

	  /* We succeeded in some recursed level of resolution, and have
	   * something else to try; try the one before to see if
	   * it could succeed now. */

	  LOG(0, ("Resolve: Recursive call succeeded on (%s), falling back "
		  "on (%s).\n", FID_(&(cur->HintFid)),
		  FID_(&(cur->prev->HintFid))));

	  /* Remove from linked list. */
	  cur = cur->prev;
	  hint->prev = cur;
	  cur->next = hint;
	  free(freeme);

	  /* Resubmit a previously failed resolve. */
	  ResSubmit(NULL, &(cur->HintFid));
	  goto HandleResult;
	}

	if(code && !nonhint && (code != ERESHINT)) {

	  /* We failed, !nonhint meaning it was a hinted resolution. Stop
	   * with the hinted resolution attempts, and simply try a normal
	   * resolution on the original obj to mark everything in conflict. */

	  LOG(0,("Resolve: Recursive resolve failed, resubmitting "
		 "original object for non-hinted resolution.\n"));

	  nonhint = 1;
	  ResSubmit(NULL, &(head->HintFid));
	}

	if(!code && !nonhint && cur && hint && (cur->prev != NULL)) {
	  struct resolve_node *freeme = cur;

	  /* We succeeded in some recursed level of resolution; try the
	   * one before to see if it could succeed now. */

	  /* Remove from linked list. */
	  cur = cur->prev;
	  hint->prev = cur;
	  cur->next = hint;
	  free(freeme);

	  /* Resubmit a previously failed resolve. */
	  ResSubmit(NULL, &(cur->HintFid));
	}

	if(code && !nonhint && (code != ERESHINT)) {

	  /* We failed deep within a recursive resolution. Abort. */

	  LOG(0,("Resolve: Recursive resolve failed, resubmitting "
		 "original object for non-hinted resolution.\n"));

	  nonhint = 1;
	  ResSubmit(NULL, &(head->HintFid));
	}

HandleResult:
	PutConn(&c);
	if (m) m->Put();
	r->HandleResult(code);
    }

Exit:
    {
	/* Release pending resents.  Waiters can retry if they wish. */
	resent *r;
	while ((r = (resent *)res_list->get()))
	    r->HandleResult(ERETRY);
    }

    {
      /* Clean up resolution linked list. */
      struct resolve_node *freenow, *freenext;
      for(freenow = head; freenow != NULL; freenow = freenext) {
	freenext = freenow->next;
	free(freenow);
      }
    }

    /* Surrender control of the volume. */
    VOL_ASSERT(this, v->u.u_error == 0);
    flags.transition_pending = 1;
    /* if code was non-zero, return EINVAL to End_VFS to force this
       resolve to inc fail count rather than success count */
    if (code) v->u.u_error = EINVAL;
    v->End_VFS();
    /* reset it, 'cause we can't leave errors just laying around */
    v->u.u_error = 0;
}


/* Asynchronous resolve is indicated by NULL waitblk. */
void repvol::ResSubmit(char **waitblkp, VenusFid *fid)
{
    VOL_ASSERT(this, fid->Realm == realm->Id() && fid->Volume == vid);

    /* Create a new resolver entry for the fid, unless one already exists. */
    {
	olist_iterator next(*res_list);
	resent *r = 0;
	while ((r = (resent *)next()))
	    if (FID_EQ(&r->fid, fid)) break;
	if (r == 0) {
	    r = new resent(fid);
	    res_list->append(r);
	}

	/* Bump refcnt and set up pointer to resent in case of SYN resolve. */
	if (waitblkp != 0) {
	    r->refcnt++;
	    *(resent **)waitblkp = r;
	}
    }

    /* Force volume state transition at next convenient point. */
    flags.transition_pending = 1;

    /* Demote the object (if cached). */
    fsobj *f = FSDB->Find(fid);
    if (f) f->Demote();
}


/* Wait for resolve to complete. */
int repvol::ResAwait(char *waitblk)
{
    int code = 0;

    resent *r = (resent *)waitblk;
    while (r->result == -1)
	VprocWait((char *)r);

    code = r->result;
    if (--(r->refcnt) == 0)
	delete r;

    return(code);
}


/* *****  Resent  ***** */

resent::resent(VenusFid *Fid) {
    LOG(10, ("resent::resent: fid = (%s)\n", FID_(Fid)));

    fid = *Fid;
    result = -1;
    refcnt = 0;

#ifdef VENUSDEBUG
    allocs++;
#endif
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
resent::resent(resent& r) {
    abort();
}


int resent::operator=(resent& r) {
    abort();
    return(0);
}


resent::~resent() {
#ifdef VENUSDEBUG
    deallocs++;
#endif

    LOG(10, ("resent::~resent: fid = (%s)\n", FID_(&fid)));

    CODA_ASSERT(refcnt == 0);
}


void resent::HandleResult(int code) {
    result = code;

    if (result == EINCONS) {
	/* Purge the object if it is cached. */
	fsobj *f = FSDB->Find(&fid);
	if (f != 0) {
	    f->Lock(WR);
	    Recov_BeginTrans();
	    f->Kill();
	    Recov_EndTrans(CMFP);
	    FSDB->Put(&f);
	}
    }

    if (refcnt == 0)
	delete this;
    else
	VprocSignal((char *)this);
}


void resent::print() {
    print(stdout);
}


void resent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void resent::print(int afd) {
    fdprint(afd, "%#08x : fid = (%s), result = %d, refcnt = %d\n",
	     (long)this, FID_(&fid), result, refcnt);
}


/* *****  Resolver  ***** */

static const int ResolverStackSize = 32768;
static const int MaxFreeResolvers = 2;


class resolver : public vproc {
  friend void Resolve(volent *);

    static olist freelist;
    olink handle;

    resolver();
    resolver(resolver&);	/* not supported! */
    int operator=(resolver&);	/* not supported! */
    ~resolver();

  protected:
    virtual void main(void);
};

olist resolver::freelist;


void Resolve(volent *v)
{
    /* Get a free resolver. */
    resolver *r;
    olink *o = resolver::freelist.get();
    r = (o == 0)
      ? new resolver
      : strbase(resolver, o, handle);
    CODA_ASSERT(r->idle);

    /* Set up context for resolver. */
    r->u.Init();
    r->u.u_vol = v;
    v->hold();			    /* vproc::End_VFS() will do release */

    /* Set it going. */
    r->idle = 0;
    VprocSignal((char *)r);
}


resolver::resolver() :
	vproc("Resolver", NULL, VPT_Resolver, ResolverStackSize)
{
    LOG(100, ("resolver::resolver(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    /* Poke main procedure. */
    start_thread();
}


resolver::resolver(resolver& r) : vproc((vproc&)r) {
    abort();
}


int resolver::operator=(resolver& r) {
    abort();
    return(0);
}


resolver::~resolver() {
    LOG(100, ("resolver::~resolver: %-16s : lwpid = %d\n", name, lwpid));

}


void resolver::main()
{
    for (;;) {
	/* Wait for new request. */
	idle = 1;
	VprocWait((char *)this);
	if (idle) CHOKE("resolver::main: signalled but not dispatched!");
	if (!u.u_vol) CHOKE("resolver::main: no volume!");

	/* Do the resolve. */
        CODA_ASSERT(u.u_vol->IsReplicated());
	((repvol *)u.u_vol)->Resolve();

	seq++;

	/* Commit suicide if we already have enough free resolvers. */
	if (freelist.count() == MaxFreeResolvers) {
	    idle = 1;
	    delete VprocSelf();
	}

	/* Else put ourselves on free list. */
	freelist.append(&handle);
    }
}
