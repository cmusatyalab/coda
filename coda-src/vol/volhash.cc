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








/*
 *
 * Implementation of the Volume name hash abstraction.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <inconsist.h>
#include <util.h>
#include "volhash.h"
#include "voldefs.h"

typedef int (*PFIV)(void *);


static vhashtab *VolTable;
static int VolIdHash(VolumeId volid);

int VolIdHash(VolumeId volid)
{
    register unsigned int sum = 0;
    register int n;
    char s[32], *tmp;
    
    sprintf(s, VFORMAT, volid);
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
    CODA_ASSERT(size > 0);
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

    while ((vol = next())) {
	if(vol->id == volid)
	    return(vol);
    }

    return(NULL);
}

/* Returns the number of volumes in the table */
int vhashtab::volumes() 
{
    return(vols);
}

void vhashtab::vprint(FILE *fp) {
    if (fp == NULL) {
	printf("%p : %s\n", this, name);
	printf("%d volumes\n", vols);
    }
    else {
	fprintf(fp, "%p : %s\n", this, name);
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
int HashInsert(VolumeId volid, int vol_index) 
{
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
