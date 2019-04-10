/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _LOCAL_H_
#define _LOCAL_H_ 1

/* from util */
#include <dlist.h>
#include <rec_dlist.h>

/* from venus */
#include "fso.h"
#include "venusvol.h"
#include <lwp/lock.h>

/* below are methods for repair local subtrees */
void DiscardLocalMutation(repvol *, char *); /*?*/
void PreserveLocalMutation(char *); /*N*/
void PreserveAllLocalMutation(char *); /*N*/
void ListCML(VenusFid *, FILE *); /*U*/

/* class for a dir entry used for process uncached children
   (Satya, 8/12/96): had to change the name from dirent to
   vdirent to prevent name clash with sys/dirent.h in BSD44
*/
class vdirent : public dlink {
    VenusFid fid;
    char name[CODA_MAXNAMLEN + 1];

public:
    vdirent(VenusFid *, char *);
    ~vdirent();
    VenusFid *GetFid();
    char *GetName();

    void print(FILE *);
    void print();
    void print(int);
};

class dir_iterator : public dlist_iterator {
public:
    dir_iterator(dlist &);
    vdirent *operator()();
};

/* class for fsobj object-pointer */
class optent : public dlink {
    fsobj *obj;
    int tag;

public:
    optent(fsobj *);
    ~optent();
    fsobj *GetFso();
    void SetTag(int);
    int GetTag();

    void print(FILE *);
    void print();
    void print(int);
};

class opt_iterator : public dlist_iterator {
public:
    opt_iterator(dlist &);
    optent *operator()();
};

/* class for repvol object-pointer */
class vptent : public dlink {
    repvol *vpt;

public:
    vptent(repvol *);
    ~vptent();
    repvol *GetVol();

    void print(FILE *);
    void print();
    void print(int);
};

class vpt_iterator : public dlist_iterator {
public:
    vpt_iterator(dlist &);
    vptent *operator()();
};

/*
 * constants for local mutation integrity check.
 * VV_CONFLICT: means version vector conflict.
 * NN_CONFLICT: means name/name conflict.
 * RU_CONFLICT: means remove(client)/update(server) conflict.
 */
#define MUTATION_MISS_TARGET 0x1
#define MUTATION_MISS_PARENT 0x2
#define MUTATION_ACL_FAILURE 0x4
#define MUTATION_VV_CONFLICT 0x8
#define MUTATION_NN_CONFLICT 0x10
#define MUTATION_RU_CONFLICT 0x20

/* constants for local repair option */
#define REPAIR_FAILURE 0x1
#define REPAIR_OVER_WRITE 0x2
#define REPAIR_FORCE_REMOVE 0x4

/* constant for the initial value of repair transaction-id number generator */
#define REP_INIT_TID 1000000

/* object-based debug macro */
#define OBJ_ASSERT(o, ex)                                               \
    {                                                                   \
        if (!(ex)) {                                                    \
            (o)->print(GetLogFile());                                   \
            CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, \
                  __LINE__);                                            \
        }                                                               \
    }

#endif /* _LOCAL_H_ */
