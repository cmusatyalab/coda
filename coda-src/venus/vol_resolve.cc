#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vol_resolve.cc,v 4.3 1997/06/14 21:48:36 braam Exp $";
#endif /*_BLURB_*/




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
#endif __cplusplus

#include <stdio.h>
#include <struct.h>

#include <unistd.h>
#include <stdlib.h>

/* interfaces */
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus


/* from util */
#include <olist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"


#ifdef VENUSDEBUG
int resent::allocs = 0;
int resent::deallocs = 0;
#endif VENUSDEBUG

void volent::Resolve() {
    LOG(0, ("volent::Resolve: %s\n", name));

    int code = 0;
    vproc *v = VprocSelf();

    /* Grab control of the volume. */
    v->Begin_VFS(vid, VFSOP_RESOLVE);
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
	    unsigned long ph = m->GetPrimaryHost();
	    ASSERT(ph != 0);
	    code = ::GetConn(&c, ph, V_UID, 0);
	    if (code != 0) goto HandleResult;

	    /* Make the RPC call. */
	    MarinerLog("store::Resolve (%x.%x.%x)\n",
		       r->fid.Volume, r->fid.Vnode, r->fid.Unique);
	    UNI_START_MESSAGE(ViceResolve_OP);
	    code = RecResolve(c, &r->fid);
	    UNI_END_MESSAGE(ViceResolve_OP);
	    MarinerLog("store::resolve done\n");

	    /* Examine the return code to decide what to do next. */
	    code = Collate(c, code);
	    UNI_RECORD_STATS(ViceResolve_OP);
	}

HandleResult:
	PutConn(&c);
	PutMgrp(&m);
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

/* The recursive resolution operation, which allows ancestors to get
   resolved first if the Vnode doesn't exist on all servers in the
   VSG */
int volent::RecResolve(connent *c, ViceFid *fid)
{
    int code;
    fsobj *f;
    ViceFid *pfid;

    code = (int) ViceResolve(c->connid, fid);
    /* done unless we want to retry the parent */
    LOG(10,("RecResolve: first attempt for (0x%x.0x%x.0x%x) returns %d\n", fid->Volume, fid->Vnode, fid->Unique, code));
    if ( code != VNOVNODE ) 
        return code;

    /* recursively try ancestors within the same volume */

    f = FSDB->Find(fid);
    if ( f == NULL ) 
        return code;

    pfid = &(f->pfid);
    if ( ( ! FID_EQ(*pfid, NullFid)) && 
	 (pfid->Volume == fid->Volume) 
	 && (pfid->Vnode != fid->Vnode) ) {
        code = RecResolve(c, pfid);
	LOG(10,("RecResolve: recursive call for (0x%x.0x%x.0x%x) returns %d\n", pfid->Volume, pfid->Vnode, pfid->Unique, code));
    } else {
        return code;
    }
    
    /* if parent was resolved successfully, retry child */
    if ( code == 0 ) {
        code = (int) ViceResolve(c->connid, fid);
        LOG(10,("RecResolve: final attempt for (0x%x.0x%x.0x%x) returns %d\n", fid->Volume, fid->Vnode, fid->Unique, code)); 
    }

    return code;
}


/* Asynchronous resolve is indicated by NULL waitblk. */
void volent::ResSubmit(char **waitblkp, ViceFid *fid) {
    VOL_ASSERT(this, fid->Volume == vid);

    /* Create a new resolver entry for the fid, unless one already exists. */
    {
	olist_iterator next(*res_list);
	resent *r = 0;
	while (r = (resent *)next())
	    if (FID_EQ((r->fid), *fid)) break;
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
int volent::ResAwait(char *waitblk) {
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

resent::resent(ViceFid *Fid) {
    LOG(10, ("resent::resent: fid = (%x.%x.%x)\n",
	      Fid->Volume, Fid->Vnode, Fid->Unique));

    fid = *Fid;
    result = -1;
    refcnt = 0;

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
resent::resent(resent& r) {
    abort();
}


resent::operator=(resent& r) {
    abort();
    return(0);
}


resent::~resent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG

    LOG(10, ("resent::~resent: fid = (%x.%x.%x)\n",
	      fid.Volume, fid.Vnode, fid.Unique));

    ASSERT(refcnt == 0);
}


void resent::HandleResult(int code) {
    result = code;

    if (result == EINCONS) {
	/* Purge the object if it is cached. */
	fsobj *f = FSDB->Find(&fid);
	if (f != 0) {
	    f->Lock(WR);
	    ATOMIC(
		f->Kill();
	    , CMFP)
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
    fdprint(afd, "%#08x : fid = (%x.%x.%x), result = %d, refcnt = %d\n",
	     (long)this, fid.Volume, fid.Vnode, fid.Unique, result, refcnt);
}


/* *****  Resolver  ***** */

PRIVATE const int ResolverStackSize = 32768;
PRIVATE const int MaxFreeResolvers = 2;


class resolver : public vproc {
  friend void Resolve(volent *);

    static olist freelist;
    olink handle;

    resolver();
    resolver(resolver&);	/* not supported! */
    operator=(resolver&);	/* not supported! */
    ~resolver();

  public:
    void main(void *);
};

olist resolver::freelist;


void Resolve(volent *v) {
    /* Get a free resolver. */
    resolver *r;
    olink *o = resolver::freelist.get();
    r = (o == 0)
      ? new resolver
      : strbase(resolver, o, handle);
    ASSERT(r->idle);

    /* Set up context for resolver. */
    r->u.Init();
#ifdef __MACH__
    r->u.u_cred.cr_ruid = V_UID;
#endif /* __MACH__ */
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
	vproc("Resolver", (PROCBODY) &resolver::main, VPT_Resolver, ResolverStackSize) {
    LOG(100, ("resolver::resolver(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    /* Poke main procedure. */
    VprocSignal((char *)this, 1);
}


resolver::resolver(resolver& r) : vproc((vproc)r) {
    abort();
}


resolver::operator=(resolver& r) {
    abort();
    return(0);
}


resolver::~resolver() {
    LOG(100, ("resolver::~resolver: %-16s : lwpid = %d\n", name, lwpid));

}


void resolver::main(void *parm) {
    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

    for (;;) {
	/* Wait for new request. */
	idle = 1;
	VprocWait((char *)this);
	if (idle) Choke("resolver::main: signalled but not dispatched!");
	if (!u.u_vol) Choke("resolver::main: no volume!");

	/* Do the resolve. */
	u.u_vol->Resolve();

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
