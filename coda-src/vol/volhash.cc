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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/vol/RCS/volhash.cc,v 4.1 1997/01/08 21:52:19 rvb Exp $";
#endif /*_BLURB_*/








/*
 *
 * Implementation of the Volume name hash abstraction.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <inconsist.h>
#include <callback.h>
#include <util.h>
#include "volhash.h"

typedef int (*PFIV)(void *);


PRIVATE vhashtab *VolTable;
PRIVATE int VolIdHash(VolumeId volid);

int VolIdHash(VolumeId volid)
{
    register unsigned int sum = 0;
    register int n;
    char s[32], *tmp;
    
    sprintf(s, "%010lu", volid);
    /* Sum the id in reverse so that consecutive integers, as strings, do not
       hash to consecutive locations */
    tmp = s;
    for (sum = 0, n = strlen(s), tmp += n-1; n--; tmp--) {
        sum = (sum*31) + (*tmp-31);
    }
    return(sum);
}

/* Initializes the volume table to the specified size. */
/* NOTE: 'size' must be a power of 2! */
void InitVolTable(int size)
{
    VolTable = new vhashtab(size, (int (*)(void *)) VolIdHash, "VolTable");
/*    VolTable = new vhashtab(size, NULL, "VolTable");*/
}

/* Constructor for vhashtab */
vhashtab::vhashtab(int size, int (*hashfn)(void *), char *n)
: ohashtab(size, hashfn)
{
    assert(size > 0);
    name = new char [strlen(n) + 1];
    strcpy(name, n);
    vols = 0;
    
    lock = 0;
}

vhashtab::~vhashtab()
{
    if (name) delete name;
}

void vhashtab::Lock(int level)
{
    lock = level;
}

void vhashtab::Unlock()
{
    lock = 0;
}

void vhashtab::add(hashent *vol)
{
    ohashtab::insert((void *)vol->id, vol);
    vols++;
}

void vhashtab::remove(hashent *vol)
{
    if (!vol) {
	LogMsg(0, VolDebugLevel, stdout, "hashtab::remove called on null entry!");
	exit(-1);
    }

    ohashtab::remove((void *)vol->id, vol);
    vols--;
}

hashent *vhashtab::find(VolumeId volid)
{
    vhash_iterator next(*this, volid);
    hashent *vol;

    while (vol = next()) {
	if(vol->id == volid)
	    return(vol);
    }

    return(NULL);
}

/* Returns the number of volumes in the table */
int vhashtab::volumes() {
    return(vols);
}

void vhashtab::vprint(FILE *fp) {
    if (fp == NULL) {
	printf("%#08x : %s\n", (long)this, name);
	printf("%d volumes\n", vols);
    }
    else {
	fprintf(fp, "%#08x : %s\n", (long)this, name);
	fprintf(fp, "%d volumes\n", vols);
    }
}

/* initialize vhash iterator; key of -1 iterates through whole table */
vhash_iterator::vhash_iterator(vhashtab& voltab, VolumeId key)
: ohashtab_iterator(voltab, (void *)key)
{
}

/* returns next element in specified bucket or table */
hashent *vhash_iterator::operator()() {
    hashent *vol = (hashent *)ohashtab_iterator::operator()();
    return(vol);
}

hashent::hashent(VolumeId volid, int volindex) {
    id = volid;
    index = volindex;
}


/* Lookup a volume in the volume name hash table. Returns the volume's */
/* index in recoverable storage, or -1 if not found */
int HashLookup(VolumeId volid) {
    hashent *vol = VolTable->find(volid);
    if (vol == NULL) return (-1);
    return(vol->index);
}

/* insert a new volume into the volume name hash table. */
/* Returns -1 if the entry already exists */
int HashInsert(VolumeId volid, int vol_index) {
    hashent *vol = VolTable->find(volid);
    if (vol != NULL) return (-1);
    vol = new hashent(volid, vol_index);
    VolTable->add(vol);
    return(0);
}

/* delete a volume from the volume name hash table. */
/* returns -1 if the entry does not exist */
int HashDelete(VolumeId volid) {
    hashent *vol = VolTable->find(volid);
    if (vol == NULL) return (-1);
    VolTable->remove(vol);
    delete vol;
    return(0);
}
