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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vol/Attic/vsg.h,v 4.1 1997/01/08 21:52:22 rvb Exp $";
#endif /*_BLURB_*/





#ifndef _VOL_VSG_H_
#define _VOL_VSG_H_	1
#include <ohash.h>
#define VSG_MEMBERS 8
#define VSGHASHTBLSIZE	64
class vsgent {
    friend int GetHosts(unsigned long, unsigned long *, int *);
    friend int AddMember(vsgent *);
    friend unsigned long GetVSGAddress(unsigned long *, int );
    friend void ClearVSGDB();
    friend void InitVSGDB();
    friend void PrintVSGDB();
    friend void CheckVSGDB();

    /* global data */
    static ohashtab *hosttab;
    static ohashtab *vsgaddrtab;
    static int nvsgs;

    /* immutable data */
    unsigned long VSGaddr;
    unsigned long Hosts[VSG_MEMBERS];
    int nhosts;

    /* mutable data */
    olink htabhandle;
    olink vsgtabhandle;
    
    vsgent(unsigned long, unsigned long *, int);
    ~vsgent();

  public:
    void print();
    void print(FILE *);
    void print(int);
};

#define VSGPATH	"/vice/db/VSGDB"
extern int GetHosts(unsigned long, unsigned long *, int *);
extern int AddMember(vsgent *);
extern unsigned long GetVSGAddress(unsigned long *, int);
extern void InitVSGDB();
extern void ClearVSGDB();
extern void CheckVSGDB();

/* this is the old vsg stuff - 
 * It corresponds to the multi cast vsg groups
 * Someday both these vsg's should be merged.
 * Since no one is working the the multi cast stuff in the kernel
 * I am commenting out the calls in vrdb.c 
 */
#ifdef notdef
#include <olist.h>


class vsgtab;
class vsgtab_iterator;
class vsgent;


class vsgtab : public olist {
    char *name;
    void add(vsgent *);
    void remove(vsgent *);
    vsgent *find(unsigned long);
  public:
    vsgtab(char * ="anonymous vsgtab");
    vsgtab(vsgtab&);	// not supported!
    operator=(vsgtab&);	// not supported!
    ~vsgtab();
    void join(unsigned long);
    void UnMark();
    void GarbageCollect();
    void print();
    void print(FILE *);
    void print(int);
};


class vsgtab_iterator : public olist_iterator {
  public:
    vsgtab_iterator(vsgtab&);
    vsgent *operator()();
};


class vsgent : public olink {
  friend class vsgtab;
    unsigned long addr;
    int fd;
    int mark;
  public:
    vsgent(unsigned long, int);
    void Mark();
    void UnMark();
    int IsMarked();
    void print();
    void print(FILE *);
    void print(int);
};


extern vsgtab JoinedVSGs;

#endif notdef


#endif _VOL_VSG_H_
