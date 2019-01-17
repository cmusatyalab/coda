/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

#ifndef _MGRP_H_
#define _MGRP_H_

#include <rpc2/rpc2.h>
#include <dllist.h>
#include <inconsist.h>
#include <vcrcommon.h>
#include "refcounted.h"

void Mgrp_Wait(void);
void Mgrp_Signal(void);

class mgrpent;
class fsobj;
class repvol;
class volent;
class vsgent;
class ClientModifyLog;
class cmlent;

class RepOpCommCtxt {
    friend class mgrpent;
    friend class fsobj;
    friend class repvol;
    friend class reintvol;
    friend class volent;
    friend class vsgent;
    friend class ClientModifyLog;
    friend class cmlent;

    RPC2_Integer HowMany;
    RPC2_Handle handles[VSG_MEMBERS];
    struct in_addr hosts[VSG_MEMBERS];
    RPC2_Integer retcodes[VSG_MEMBERS];
    struct in_addr primaryhost;
    RPC2_Multicast *MIp;
    unsigned dying[VSG_MEMBERS];

public:
    RepOpCommCtxt();
    RepOpCommCtxt(RepOpCommCtxt &) { abort(); } /* not supported! */
    int operator=(RepOpCommCtxt &)
    {
        abort();
        return (0);
    } /* not supported! */
    ~RepOpCommCtxt() {}

    int AnyReturned(int code);

    void print(FILE *f)
    {
        fprintf(f, "%p : HowMany = %d\n", this, (int)HowMany);
    }
    void print() { print(stdout); }
};

class mgrpent : private RefCountedObject {
    friend void MallocPrint(int fd);
    friend void CommInit();
    friend void Mgrp_Wait();
    friend void Mgrp_Signal();
    friend class mgrp_iterator;
    friend class fsobj;
    friend class repvol;
    friend class reintvol;
    friend class volent;
    friend class vsgent;
    friend class ClientModifyLog;
    friend class cmlent;

    /* mgrp syncronization. */
    static char mgrp_sync;

    /* Static state; immutable after construction. */
    vsgent *vsg;
    struct dllist_head vsghandle;
    uid_t uid; /* UID to validate with respect to. */
    RPC2_Multicast McastInfo;
    unsigned authenticated : 1;
    unsigned disconnectfs : 1;

    /* Dynamic state; varies with each call. */
    RepOpCommCtxt rocc;

#ifdef VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif /* VENUSDEBUG */

    /* Constructors, destructors, and private utility routines. */
    mgrpent(vsgent *, uid_t, RPC2_Handle, int);
    ~mgrpent();

public:
    void Put(void);
    void Kill(int tellservers);

    /* the list_empty test protects us during initialization/destruction */
    int InUse(void) { return (refcount > 1 || list_empty(&vsghandle)); }
    int IsAuthenticated(void) { return authenticated; }

    int CreateMember(int idx);
    void KillMember(struct in_addr *, int);
    int GetHostSet();
    void PutHostSet();

    void CheckResult();
    int CheckNonMutating(int);
    int CheckCOP1(int, ViceVersionVector *, int = 1);
    int CheckReintegrate(int, ViceVersionVector *);
    int RVVCheck(ViceVersionVector **, int);
    int DHCheck(ViceVersionVector **, int, int *, int = 0);
    int PickDH(ViceVersionVector **RVVs);
    struct in_addr *GetPrimaryHost(int *ph_ixp = NULL);

    void print(FILE *f)
    {
        fprintf(f,
                "%p : uid = %d, mid = %d, auth = %d, ref = %d, detached = %d\n",
                this, uid, (int)McastInfo.Mgroup, authenticated, refcount,
                list_empty(&vsghandle));
    }
};

#endif /* _MGRP_H_ */
