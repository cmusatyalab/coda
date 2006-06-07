/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 * Specification of the Volume name hash table.
 *
 */

#ifndef _VOLHASH_H
#define _VOLHASH_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
}
#endif

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
    vhashtab(int size, intptr_t (*hashfn)(void *), char*);
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

    int get_index() {return(index);};
  public:
    hashent(VolumeId, int);
    ~hashent() {};
};

extern int HashInsert(VolumeId, int);
extern int HashLookup(VolumeId);
extern int HashDelete(VolumeId);

#endif /* _VOLHASH_H */
