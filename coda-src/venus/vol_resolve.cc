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

void repvol::Resolve()
{
    LOG(0, ("repvol::Resolve: %s\n", name));
    MarinerLog("resolve::%s\n", name);

    fsobj *f;
    int code = 0;
    vproc *v = VprocSelf();

    VolFid vfid;
    vfid.Realm = realm->id;
    vfid.Volume = vid;

    /* Grab control of the volume. */
    v->Begin_VFS(&vfid, CODA_RESOLVE);
    VOL_ASSERT(this, v->u.u_error == 0);

    /* Flush all COP2 entries. */
    /* This would NOT be necessary if ViceResolve took a "PiggyCOP2" parameter! -JJK */
    code = FlushCOP2();
    if (code != 0) goto Exit;

    resent *r;
    while ((r = (resent *)res_list->get())) {
	mgrpent *m = 0;
	connent *c = 0;

	{
	    /* Get an Mgroup. */
	    code = GetMgrp(&m, V_UID);
	    if (code != 0) goto HandleResult;

	    /* Pick a coordinator and get a connection to it. */
	    struct in_addr *phost = m->GetPrimaryHost();
	    CODA_ASSERT(phost->s_addr != 0);
            srvent *s;
            GetServer(&s, phost);
	    code = s->GetConn(&c, V_UID);
            PutServer(&s);
	    if (code != 0) goto HandleResult;

	    /* Make the RPC call. */
	    MarinerLog("store::Resolve (%s)\n", FID_(&r->fid));
	    UNI_START_MESSAGE(ViceResolve_OP);
	    code = ViceResolve(c->connid, MakeViceFid(&r->fid));
	    UNI_END_MESSAGE(ViceResolve_OP);
	    MarinerLog("store::resolve done\n");

	    /* Examine the return code to decide what to do next. */
	    code = Collate(c, code);
	    UNI_RECORD_STATS(ViceResolve_OP);
	}

	/* Demote the object (if cached) */
	f = FSDB->Find(&r->fid);
	if (f) f->Demote();

	if (code == VNOVNODE) {
	    VenusFid *pfid;
	    if (f) {
		pfid = &(f->pfid);
		if (!FID_EQ(pfid, &NullFid) && FID_VolEQ(pfid, &r->fid) &&
		    (pfid->Vnode != r->fid.Vnode) ) {

		    LOG(10,("Resolve: Submitting parent for resolution\n"));
		    ResSubmit(NULL, pfid);

		    /* We shouldn't resubmit ourselves as this might lead to an
		     * endless loop. Hopefully the hoard/getattr that triggered
		     * the resolution will loop around and retry. --JH */
		    // ResSubmit(NULL, r->fid);
		}
	    } else
		LOG(10,("Resolve: Couldn't find current object\n"));
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
    VOL_ASSERT(this, fid->Realm == realm->id && fid->Volume == vid);

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
#ifdef __BSD44__
    r->u.u_cred.cr_uid = V_UID;
#endif /* __BSD44__ */
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
