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

#ifndef _VOL_VSG_H_
#define _VOL_VSG_H_	1
#include <ohash.h>
#include <vcrcommon.h>
#define VSGHASHTBLSIZE	64

class vsgent {
    friend int GetHosts(unsigned long, unsigned long *, int *);
    friend int AddMember(vsgent *);
    friend unsigned long GetVSGAddress(unsigned long *, int);
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

#define VSGPATH	vice_sharedfile("db/VSGDB")
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

#endif /* notdef */


#endif /* _VOL_VSG_H_ */
