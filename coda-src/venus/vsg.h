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

#ifndef _VSG_H_
#define _VSG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include <dllist.h>
#include <vcrcommon.h>
#include "refcounted.h"

class vsgdb;
class mgrpent;

class vsgent : private RefCountedObject {
    friend class vsgdb;

private:
    unsigned int nhosts; /* number of replica servers in this VSG */
    struct in_addr hosts[VSG_MEMBERS];  /* hosts in this VSG */
    struct dllist_head mgrpents;        /* list of mgroups for this VSG */
    RealmId realm;

protected:
    struct dllist_head vsgs; /* list of all vsgents */

    vsgent(struct in_addr Hosts[VSG_MEMBERS], RealmId realm);
    ~vsgent(void);

    int CmpHosts(struct in_addr Hosts[VSG_MEMBERS]) {
      return (memcmp(hosts, Hosts, VSG_MEMBERS * sizeof(struct in_addr)) == 0);
    }

public:
#ifdef VENUSDEBUG
    static unsigned int allocs;
    static unsigned int deallocs;
#endif
    void Put(void) { PutRef(); }

    int GetMgrp(mgrpent **m, vuid_t vuid, int auth = 1);
    RealmId Realm(void) { return realm; }
    void KillMgrps(void);
    void KillUserMgrps(vuid_t vuid);
    void KillMgrpMember(struct in_addr *);

    unsigned int NHosts(void) { return nhosts; }
    void GetHosts(struct in_addr Hosts[VSG_MEMBERS]) {
      memcpy(Hosts, hosts, VSG_MEMBERS * sizeof(struct in_addr));
    }

    void print(FILE *f);
};
    
class vsgdb {
private:
    struct dllist_head vsgents;        /* list of VSGs */

public:
    vsgdb(void);
    ~vsgdb(void);

    vsgent *GetVSG(struct in_addr hosts[VSG_MEMBERS], RealmId realm);
    void KillUserMgrps(vuid_t vuid);

    void print(FILE *f);
    void print() { print(stdout); }
};

extern vsgdb *VSGDB;

void VSGDBInit(void);

#endif /* _VSG_H_ */

