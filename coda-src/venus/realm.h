/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _REALM_H_
#define _REALM_H_

#include <sys/types.h>
#include <rvmlib.h>
#include <coda_assert.h>
#include <auth2.h>
#include "venusfid.h"

class connent;
class userent;

class Realm {
    friend class RealmDB;
    friend class fsobj; // Fakeify

public:
    void *operator new(size_t size) { /*T*/
	void *p = rvmlib_rec_malloc(size);
	CODA_ASSERT(p);
	return p;
    }
    void operator delete(void *p, size_t size) { rvmlib_rec_free(p); } /*T*/

    Realm(const char *realm);	/*T*/
    ~Realm(void);		/*T*/

    void ResetTransient(void);

    void Rec_GetRef(void) {	/*T*/
	RVMLIB_REC_OBJECT(rec_refcount);
	rec_refcount++;
    }
    void Rec_PutRef(void);	/*T*/
    void GetRef(void) { refcount++; }
    void PutRef(void);

    const char *Name(void) { return name; }
    const RealmId Id(void) { return (RealmId)this; }

    /* MUST NOT be called from within a transaction */
    int GetAdmConn(connent **cpp); /*N*/

    userent *GetUser(uid_t uid);
    int NewUserToken(uid_t uid, SecretToken *secretp, ClearToken *clearp);

    void print(FILE *f);

private:
    char *name;
    struct dllist_head realms;
    unsigned int rec_refcount;

/*T*/unsigned int refcount;
/*T*/struct RPC2_addrinfo *rootservers;
/*T*/userent *system_anyuser;
};

#endif /* _REALM_H_ */

