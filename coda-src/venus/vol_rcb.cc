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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/venus/vol_rcb.cc,v 1.2 1997/01/07 18:42:28 rvb Exp";
#endif /*_BLURB_*/







/*
 *
 * Implementation of the Venus RemoveCallBack (RCB) facility.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#include <netinet/in.h>

#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "venusvol.h"
#include <olist.h>
#include <nettohost.h>
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "vice.h"
#include "local.h"


PRIVATE const int RCBEntrySize = (int) (sizeof(ViceFid) + sizeof(unsigned long));

int volent::RCB(mgrpent *m, RPC2_CountedBS *BS) {
    LOG(10, ("volent::RCB: \n"));

    int code = 0;

    /* Make the RPC call. */
    MarinerLog("store::RCB %s\n", name);
    MULTI_START_MESSAGE(ViceRemoveCallBack_OP);

    /* This needs to be reimplemented! */
/*
    code = MRPC_MakeMulti(ViceRemoveCallBack_OP, ViceRemoveCallBack_PTR,
			   VSG_MEMBERS, m->rocc.handles,
			   m->rocc.retcodes, m->rocc.MIp, 0, 0,
			   BS);
*/
    MULTI_END_MESSAGE(ViceRemoveCallBack_OP);
    MarinerLog("store::RCB done\n");

    /* Collate responses from individual servers and decide what to do next. */
    code = Collate_NonMutating(m, code);
    MULTI_RECORD_STATS(ViceRemoveCallBack_OP);

    return(code);
}


int volent::RCB(connent *c, RPC2_CountedBS *BS) {
    LOG(10, ("volent::RCB: \n"));

    int code = 0;

    /* Make the RPC call. */
    MarinerLog("store::RCB %s\n", name);
    UNI_START_MESSAGE(ViceRemoveCallBack_OP);
/*
    code = ViceRemoveCallBack(c->connid, BS);
*/
    UNI_END_MESSAGE(ViceRemoveCallBack_OP);
    MarinerLog("store::RCB done\n");

    /* Examine the return code to decide what to do next. */
    code = Collate(c, code);
    UNI_RECORD_STATS(ViceRemoveCallBack_OP);

    return(code);
}


int volent::FlushRCB() {
    ASSERT(vid != LocalFakeVid);
    LOG(100, ("volent::FlushRCB: vol = %x\n", vid));

    int code = 0;

    for (;;) {
	if (type == REPVOL) {
	    /* Acquire an Mgroup. */
	    mgrpent *m = 0;
	    code = GetMgrp(&m, V_UID);
	    if (code != 0) { PutMgrp(&m); return(code); }

	    /* Get a buffer full of pending RCB messages. */
	    RPC2_CountedBS BS;
	    char buf[RCBSIZE];
	    BS.SeqLen = 0;
	    BS.SeqBody = (RPC2_ByteSeq)buf;
	    GetRCB(&BS);
	    if (BS.SeqLen == 0)
		{ PutMgrp(&m); return(0); }

	    /* Make the call. */
	    code = RCB(m, &BS);
	    PutMgrp(&m);
	    if (code != 0) return(code);
	    ClearRCB(&BS);
	}
	else {
	    /* Acquire a connection. */
	    connent *c = 0;
	    code = GetConn(&c, V_UID);
	    if (code != 0) { PutConn(&c); return(code); }
	    
	    /* Get a buffer full of pending RCB messages. */
	    RPC2_CountedBS BS;
	    char buf[RCBSIZE];
	    BS.SeqLen = 0;
	    BS.SeqBody = (RPC2_ByteSeq)buf;
	    GetRCB(&BS);
	    if (BS.SeqLen == 0)
		{ PutConn(&c); return(0); }

	    /* Make the call. */
	    code = RCB(c, &BS);
	    PutConn(&c);
	    if (code != 0) return(code);
	    ClearRCB(&BS);
	}
    }
}


void volent::GetRCB(RPC2_CountedBS *BS) {
    LOG(100, ("volent::GetRCB: vol = %x\n", vid));

    olist_iterator next(*RCBList);
    rcbent *r;
    while (r = (rcbent *)next()) {
	if (BS->SeqLen + sizeof(ViceFid) + sizeof(unsigned long) > RCBSIZE) {
	    LOG(100, ("volent::GetRCB: BS full\n"));
	    break;
	}

	/* Copy in the Sid and the US. */
	LOG(100, ("volent::GetRCB: [%x.%x.%x, %d]\n",
		  r->fid.Volume, r->fid.Vnode, r->fid.Unique, r->time));
	htonfid(&r->fid, (ViceFid *)&BS->SeqBody[BS->SeqLen]);
	BS->SeqLen += sizeof(ViceFid);
	*((unsigned long *)&BS->SeqBody[BS->SeqLen]) = htonl(r->time);
	BS->SeqLen += sizeof(unsigned long);
    }

    LOG(100, ("volent::GetRCB: vol = %x, entries = %d\n",
	       vid, BS->SeqLen / RCBEntrySize));
}


void volent::ClearRCB(RPC2_CountedBS *BS) {
    LOG(100, ("volent::ClearRCB: vol = %x\n", vid));

    if (BS->SeqLen % RCBEntrySize != 0)
	Choke("volent::ClearRCB: bogus SeqLen (%d)", BS->SeqLen);

    for (int cursor = 0; cursor < BS->SeqLen; cursor += RCBEntrySize) {
	ViceFid fid;
	ntohfid(((ViceFid *)&BS->SeqBody[cursor]), &fid);
	unsigned long time = ntohl(*((unsigned long *)&BS->SeqBody[cursor + sizeof(ViceFid)]));
	LOG(100, ("volent::ClearRCB: BS = [%x.%x.%x, %x]\n",
		  fid.Volume, fid.Vnode, fid.Unique, time));

	olist_iterator next(*RCBList);
	rcbent *r;
	while (r = (rcbent *)next()) {
	    LOG(100, ("volent::ClearRCB: r = [%x.%x.%x, %d]\n",
		      r->fid.Volume, r->fid.Vnode, r->fid.Unique, r->time));

	    if (FID_EQ(r->fid, fid) && r->time == time) {
		RCBList->remove(r);
		delete r;
		break;
	    }
	}
    }
}


rcbent::rcbent(ViceFid *fidp) {
    LOG(10, ("rcbent::rcbent: fid = (%x.%x.%x)\n",
	      fidp->Volume, fidp->Vnode, fidp->Unique));

    fid = *fidp;
    time = Vtime();
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
rcbent::rcbent(rcbent& r) {
    abort();
}


rcbent::operator=(rcbent& r) {
    abort();
    return(0);
}


rcbent::~rcbent() {
    LOG(10, ("rcbent::~rcbent: fid = (%x.%x.%x), time = %d\n",
	      fid.Volume, fid.Vnode, fid.Unique, time));
}


void rcbent::print() {
    print(stdout);
}


void rcbent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void rcbent::print(int afd) {
    fdprint(afd, "\t\t%#08x : fid = (%x.%x.%x), time = %d\n",
	     (long)this, fid.Volume, fid.Vnode, fid.Unique, time);
}
