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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/vol_COP2.cc,v 4.4 1998/09/29 16:38:20 braam Exp $";
#endif /*_BLURB_*/







/*
 *
 *    Implementation of the Venus COP2 facility.
 *
 *    The purpose of these routines is to distribute the UpdateSet to the AVSG members
 *    following a mutating operation.  Distribution may be synchronous, i.e., COP2() is
 *    invoked by the worker thread immediately following the COP1 call, or asynchronous.
 *    Asynchronous has two variants:  piggybacked and non-piggybacked.  In the piggybacked
 *    case the UpdateSet is sent in the next worker-invoked RPC to the relevant VSG.  In
 *    the non-piggybacked case the UpdateSet is sent in a COP2 RPC invoked by the volume
 *    daemon.  Non-piggybacked is used when piggybacked is not enabled or when no RPC
 *    occurs within a timeout period.  In either case multiple UpdateSets (corresponding to
 *    multiple COP1s) may be sent in the same RPC.  Also, UpdateSet propagation is idempotent,
 *    so UpdateSets piggybacked on successive RPCs to the same VSG may overlap.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include "coda_assert.h"
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from util */
#include <dlist.h>

/* from vv */
#include <nettohost.h>

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "local.h"


static const int COP2EntrySize = (int)(sizeof(ViceStoreId) + sizeof(ViceVersionVector));

#ifdef VENUSDEBUG
int cop2ent::allocs = 0;
int cop2ent::deallocs = 0;
#endif VENUSDEBUG

/* Send a buffer-full of UpdateSets. */
int volent::COP2(mgrpent *m, RPC2_CountedBS *PiggyBS) {
    LOG(10, ("volent::COP2: \n"));

    int code = 0;

    /* Make the RPC call. */
    MarinerLog("store::COP2 %s\n", name);
    MULTI_START_MESSAGE(ViceCOP2_OP);
    code = (int)MRPC_MakeMulti(ViceCOP2_OP, ViceCOP2_PTR,
			   VSG_MEMBERS, m->rocc.handles,
			   m->rocc.retcodes, m->rocc.MIp, 0, 0,
			   PiggyBS);
    MULTI_END_MESSAGE(ViceCOP2_OP);
    MarinerLog("store::COP2 done\n");

    /* Collate responses from individual servers and decide what to do next. */
    code = Collate_COP2(m, code);
    MULTI_RECORD_STATS(ViceCOP2_OP);
    if (code == 0) ClearCOP2(PiggyBS);

    return(code);
}


/* Send a single UpdateSet. */
int volent::COP2(mgrpent *m, ViceStoreId *StoreId, vv_t *UpdateSet) {
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen = 0;
    char PiggyData[COP2SIZE];
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    htonsid(StoreId, (ViceStoreId *)&PiggyBS.SeqBody[PiggyBS.SeqLen]);
    PiggyBS.SeqLen += sizeof(ViceStoreId);
    htonvv(UpdateSet, (ViceVersionVector *)&PiggyBS.SeqBody[PiggyBS.SeqLen]);
    PiggyBS.SeqLen += sizeof(ViceVersionVector);

    return(COP2(m, &PiggyBS));
}


/* Send pending UpdateSets that are greater than window seconds old. */
/* Younger UpdateSets may also be sent in order to complete a buffer. */
/* Other UpdateSets will either be piggybacked on subsequent RPCs, or sent directly when they are older. */
int volent::FlushCOP2(time_t window) {
    CODA_ASSERT(!FID_VolIsFake(vid));
    LOG(100, ("volent::FlushCOP2: vol = %x, window = %d\n",
	       vid, window));

    int code = 0;

    for (;;) {
	time_t curr_time = Vtime();

	/* Find the first pending COP2 message. */
	/* Don't bother getting an Mgrp unless there is at least one "sufficiently old" cop2ent. */
	cop2ent	*c = (cop2ent *)cop2_list->first();	    /* guaranteed oldest entry! */
	if (c == 0) break;
	time_t age = (curr_time - c->time);
	if (age > 0 && age < window) break;

	/* Get an Mgrp for the "system" user. */
	mgrpent *m = 0;
	code = GetMgrp(&m, V_UID);
	if (code != 0) { PutMgrp(&m); break; }

	/* Get a buffer full of pending COP2 messages. */
	/* Note that all cop2ents may have vanished due to concurrent */
	/* flushing by another thread (which ran during the GetMgrp)! */
	RPC2_CountedBS BS;
	char buf[COP2SIZE];
	BS.SeqLen = 0;
	BS.SeqBody = (RPC2_ByteSeq)buf;
	GetCOP2(&BS);
	if (BS.SeqLen == 0) {
	    LOG(1, ("volent::FlushCOP2: vol = %x, No Entries!\n", vid));
	    PutMgrp(&m);
	    break;
	}

	/* Make the call. */
	code = COP2(m, &BS);
	PutMgrp(&m);
	if (code != 0) break;

	/* Start over at the head of the list. */
    }

    return(code);
}

/* Send pending COP2 messages up to the last buffer full. */
/* Copy the last buffer-full into the supplied buffer so that it can be piggybacked. */
/* Use the supplied Mgrp for the direct COP2s. */
int volent::FlushCOP2(mgrpent *m, RPC2_CountedBS *PiggyBS) {
    CODA_ASSERT(!FID_VolIsFake(vid));
    LOG(100, ("volent::FlushCOP2(Piggy): vol = %x\n", vid));

    int code = 0;

    while (cop2_list->count() * COP2EntrySize > COP2SIZE) {
	/* Get a buffer full of pending COP2 messages. */
	RPC2_CountedBS BS;
	char buf[COP2SIZE];
	BS.SeqLen = 0;
	BS.SeqBody = (RPC2_ByteSeq)buf;
	GetCOP2(&BS);
	if (BS.SeqLen == 0)
	    { print(logFile); CHOKE("volent::FlushCOP2(Piggy): No Entries!\n"); }

	/* Make the call. */
	code = COP2(m, &BS);
	if (code != 0) break;
    }

    /* Copy out the last buffer-full for piggybacking. */
    if (code == 0 && cop2_list->count() > 0)
	GetCOP2(PiggyBS);

    return(code);
}


void volent::GetCOP2(RPC2_CountedBS *BS) {
    LOG(100, ("volent::GetCOP2: vol = %x\n", vid));

    dlist_iterator next(*cop2_list);
    cop2ent *c;
    while (c = (cop2ent *)next()) {
	/* Copy in the Sid and the US. */
	if (BS->SeqLen + sizeof(ViceStoreId) + sizeof(ViceVersionVector) > COP2SIZE)
	    return;
	htonsid(&c->sid, (ViceStoreId *)&BS->SeqBody[BS->SeqLen]);
	BS->SeqLen += sizeof(ViceStoreId);
	htonvv(&c->updateset, (ViceVersionVector *)&BS->SeqBody[BS->SeqLen]);
	BS->SeqLen += sizeof(ViceVersionVector);
    }

    LOG(100, ("volent::GetCOP2: vol = %x, entries = %d\n",
	       vid, BS->SeqLen / COP2EntrySize));
}


cop2ent *volent::FindCOP2(ViceStoreId *StoreId) {
    dlist_iterator next(*cop2_list);
    cop2ent *c;
    while (c = (cop2ent *)next())
	if (StoreId->Host == c->sid.Host &&
	    StoreId->Uniquifier == c->sid.Uniquifier)
	    return(c);

    return(0);
}


void volent::AddCOP2(ViceStoreId *StoreId, ViceVersionVector *VV) {
    cop2ent *c = new cop2ent(StoreId, VV);
    cop2_list->append(c);	    /* list must be maintained in FIFO order! */
}


void volent::ClearCOP2(RPC2_CountedBS *BS) {
    if (BS->SeqLen == 0) return;

    LOG(100, ("volent::ClearCOP2: vol = %x\n", vid));

    if (BS->SeqLen % COP2EntrySize != 0)
	CHOKE("volent::ClearCOP2: bogus SeqLen (%d)", BS->SeqLen);

    for (int cursor = 0; cursor < BS->SeqLen; cursor += COP2EntrySize) {
	ViceStoreId sid;
	ntohsid((ViceStoreId *)&BS->SeqBody[cursor], &sid);
	cop2ent *c = FindCOP2(&sid);
	if (c) {
	    if (cop2_list->remove(c) != c)
		{ print(logFile); CHOKE("volent::ClearCOP2: remove"); }
	    delete c;
	}
    }
}


static const int MaxFreeCOP2ents = 16;
static dlist freecop2ents;

void *cop2ent::operator new(size_t len) {
    cop2ent *c = 0;

    LOG(100, ("cop2ent::operator new()\n"));
    dlink *d = freecop2ents.get();
    if (d == 0) {
	c = (cop2ent *)new char[len];
	bzero((void *)c, (int)len);
    }
    else
	c = (cop2ent *)d;
    CODA_ASSERT(c);
    return(c);
}

cop2ent::cop2ent(ViceStoreId *Sid, ViceVersionVector *UpdateSet) {

    LOG(100, ("cop2ent::cop2ent()\n"));
    sid = *Sid;
    updateset = *UpdateSet;
    time = Vtime();

#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
cop2ent::cop2ent(cop2ent& c) {
    abort();
}


cop2ent::operator=(cop2ent& c) {
    abort();
    return(0);
}


cop2ent::~cop2ent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
}

void cop2ent::operator delete(void *deadobj, size_t len) {
    cop2ent *c = (cop2ent *) deadobj;

    LOG(100, ("cop2ent::operator delete()\n"));
    if (freecop2ents.count() < MaxFreeCOP2ents)
	freecop2ents.append(c);
    else
	delete [] (char *)deadobj;
}

void cop2ent::print() {
    print(stdout);
}


void cop2ent::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void cop2ent::print(int fd) {
    fdprint(fd, "sid = (%x.%x), vv = [], time = %d\n",
	     sid.Host, sid.Uniquifier, time);
}
