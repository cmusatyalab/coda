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

#include <coda_assert.h>
#include "comm.h"
#include "mgrp.h"
#include "user.h"
#include "vsg.h"
#include "realmdb.h"

vsgdb *VSGDB;

#ifdef VENUSDEBUG
unsigned int vsgent::allocs = 0;
unsigned int vsgent::deallocs = 0;
#endif

vsgent::vsgent(struct in_addr Hosts[VSG_MEMBERS], RealmId id)
{
    LOG(10, ("vsgent::vsgent %p %08x %08x %08x %08x %08x %08x %08x %08x\n",
             this, Hosts[0], Hosts[1], Hosts[2], Hosts[3],
             Hosts[4], Hosts[5], Hosts[6], Hosts[7]));

    int i;

    realmid = id;
    nhosts = 0;
    for (i = 0; i < VSG_MEMBERS; i++) {
        hosts[i] = Hosts[i];

        if (Hosts[i].s_addr)
            nhosts++;
    }

    list_head_init(&mgrpents);
    list_head_init(&vsgs);

#ifdef VENUSDEBUG
    allocs++;
#endif
}

vsgent::~vsgent(void)
{
    LOG(10, ("vsgent::~vsgent %p %08x %08x %08x %08x %08x %08x %08x %08x\n",
             this, hosts[0], hosts[1], hosts[2], hosts[3],
             hosts[4], hosts[5], hosts[6], hosts[7]));

#ifdef VENUSDEBUG
    deallocs++;
#endif

    list_del(&vsgs);
    KillMgrps();
    CODA_ASSERT(list_empty(&mgrpents));
}

const int MAXMGRPSPERUSER = 30;  /* Max simultaneous mgrps per user per vsg. */

int vsgent::GetMgrp(mgrpent **m, vuid_t vuid, int auth)
{
    LOG(10, ("vsgent::GetMgrp %p %d %d\n", this, vuid, auth));

    int code = 0;

    /* Get an mgrp for this user. */ 
try_again:
    /* Check whether there is already a free mgroup. */
    struct dllist_head *p;
    int count = 0;

    list_for_each(p, mgrpents) {
        *m = list_entry(p, mgrpent, vsghandle);
        if (vuid != ALL_UIDS && vuid != (*m)->uid)
            continue;

        count++;
        /* if we are truly MT we need atomic TestAndSet instead of InUse */
        if (!(*m)->InUse())
            goto got_mgrp;
    }

    *m = NULL;
    
    /* Wait here if MAX mgrps are already in use. */
    if (count >= MAXMGRPSPERUSER) {
	if (VprocInterrupted()) { return(EINTR); }
        Mgrp_Wait();
	if (VprocInterrupted()) { return(EINTR); }
        goto try_again;
    }

    /* Try to connect to the VSG on behalf of the user. */
    {
        RPC2_Handle MgrpHandle = 0;
        userent *u = NULL;
        struct in_addr mgrpaddr;
        mgrpaddr.s_addr = INADDR_ANY; /* Request to form an mgrp */

	Realm *realm = REALMDB->GetRealm(realmid);
	CODA_ASSERT(realm);
        GetUser(&u, realm, vuid);
        code = u->Connect(&MgrpHandle, &auth, &mgrpaddr);
        PutUser(&u);
	realm->PutRef();

        if (code < 0)
            CHOKE("vsgent::GetMgrp: bogus code (%d) from u->Connect", code);

        if (code)
            return(code);

        /* Create and install the new mgrpent. */
        *m = new mgrpent(this, vuid, MgrpHandle, auth);
        /* if we are truly MT we need to grab a refcount here */
        list_add(&(*m)->vsghandle, &mgrpents);
    }

got_mgrp:
    (*m)->GetRef();
    /* Form the host set. */
    code = (*m)->GetHostSet();
    if (code < 0)
        CHOKE("repvol::GetMgrp: bogus code (%d) from GetHostSet", code);

    if (code)
        goto exit;

    /* Choose whether to multicast or not. XXX probably broken --JH */
    (*m)->rocc.MIp = (UseMulticast) ? &(*m)->McastInfo : 0;
    
/*--- We should have a usable mgrp now */

exit: 
    if (code) {
        (*m)->Put();
        *m = NULL;
    }

    return(code);
}

void vsgent::KillMgrps(void)
{
    LOG(10, ("vsgent::KillMgrps %p\n", this));

    while (!list_empty(&mgrpents)) {
        mgrpent *m = list_entry(mgrpents.next, mgrpent, vsghandle);
        m->Kill(1); /* takes `m' out of the mgrpents list */
    }
}

void vsgent::KillUserMgrps(vuid_t vuid)
{
    LOG(10, ("vsgent::KillUserMgrps %p %d\n", this, vuid));

    struct dllist_head *p;
again:
    list_for_each(p, mgrpents) {
        mgrpent *m = list_entry(p, mgrpent, vsghandle);
        if (m->uid != vuid) continue;

        m->Kill(1);
        /* p->next has been invalidated by Kill, restart the lookup */
        goto again;
    }
}

void vsgent::KillMgrpMember(struct in_addr *addr)
{
    LOG(10, ("vsgent::KillMgrpMember %p %s\n", this, inet_ntoa(*addr)));

    struct dllist_head *p;
    list_for_each(p, mgrpents) {
        mgrpent *m = list_entry(p, mgrpent, vsghandle);
        m->KillMember(addr, 0);
    }
}

void vsgent::print(FILE *f)
{
    struct dllist_head *p;

    fprintf(f, "VSG:");
    for (int i = 0; i < VSG_MEMBERS; i++) {
        fprintf(f, " %s", inet_ntoa(hosts[i]));
    }
    fprintf(f, "\n");
    PrintRef(f);

    list_for_each(p, mgrpents) {
        mgrpent *m = list_entry(p, mgrpent, vsghandle);
        m->print(f);
    }
        
}

vsgdb::vsgdb(void)
{
    list_head_init(&vsgents);
}

vsgdb::~vsgdb(void)
{
    CODA_ASSERT(list_empty(&vsgents));
}

vsgent *vsgdb::GetVSG(struct in_addr hosts[VSG_MEMBERS], RealmId realmid)
{
    LOG(10, ("vsgdb::GetVSG %08x %08x %08x %08x %08x %08x %08x %08x\n",
             hosts[0], hosts[1], hosts[2], hosts[3],
             hosts[4], hosts[5], hosts[6], hosts[7]));

    struct dllist_head *p;
    vsgent *v;

    /* search for a matching VSG */
    list_for_each(p, vsgents) {
        v = list_entry(p, vsgent, vsgs);
        if (v->CmpHosts(hosts)) {
            v->GetRef();
            return v;
        }
    }

    /* no matches found, create a new VSG */
    v = new vsgent(hosts, realmid);
    if (v) list_add(&v->vsgs, &vsgents);

    return v;
}

void vsgdb::KillUserMgrps(vuid_t vuid)
{
    struct dllist_head *p;
    list_for_each(p, vsgents) {
        vsgent *v = list_entry(p, vsgent, vsgs);
        v->KillUserMgrps(vuid);
    }
}

void vsgdb::print(FILE *f)
{
    struct dllist_head *p;
    list_for_each(p, vsgents) {
        vsgent *v = list_entry(p, vsgent, vsgs);
        v->print(f);
    }
}

void VSGDBInit(void)
{
    VSGDB = new vsgdb;
}

