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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/volhash.h,v 4.2 1997/02/26 16:03:57 rvb Exp $";
#endif /*_BLURB_*/







/*
 *
 * Specification of the Volume name hash table.
 *
 */

#ifndef _VOLHASH_H
#define _VOLHASH_H 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <ohash.h>
#include <olist.h>
#include <inconsist.h>

class vhashtab;
class vhash_iterator;
class hashent;

class vhashtab : public ohashtab {
  friend void InitVolTable(int);
    char *name;	    /* table name */
    int vols;    /* number of volumes in table */
    int lock;
  public:
    vhashtab(int, int (*)(void *), char*);
    ~vhashtab();
    void Lock(int);
    void Unlock();
    void add(hashent *);
    void remove(hashent *);
    hashent *find(VolumeId);
    int volumes();
    void vprint(FILE* =NULL);
};

class vhash_iterator : public ohashtab_iterator {
    public:
	vhash_iterator(vhashtab&, VolumeId =-1);
	hashent *operator()();
};

class hashent: public olink {
  friend class vhashtab;
  friend class vhashtab_iterator;
  friend int HashInsert(VolumeId, int);
  friend int HashLookup(VolumeId);
  friend int HashDelete(VolumeId);
    VolumeId	id;
    int		index;
    hashent	*next;

    int get_index() {return(index);};
  public:
    hashent(VolumeId, int);
    ~hashent() {};
};

extern int HashInsert(VolumeId, int);
extern int HashLookup(VolumeId);
extern int HashDelete(VolumeId);

#endif _VOLHASH_H
