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

   Copyright (C) 1998  John-Anthony Owens, Samuel Ieong, Rudi Seitz

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

*/


#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include <gdbm.h>

#include <coda_assert.h>

#include "pdb.h"

#define PDB_MAIN "/vice/db/coda.pdb"
#define PDB_NAME  "/vice/db/name.pdb"

struct PDB_HANDLE_S {
	GDBM_FILE main;
	GDBM_FILE name;
};


PDB_HANDLE PDB_db_open(int mode)
{
	struct PDB_HANDLE_S *handle;

	handle = malloc(sizeof(*handle));
	memset(handle, 0, sizeof(*handle));
	CODA_ASSERT(handle);

	switch (mode) {
	case O_RDWR: 
	case O_WRONLY:
		handle->main = gdbm_open(PDB_MAIN, 0, GDBM_WRITER, 0, 0);
		handle->name = gdbm_open(PDB_NAME, 0, GDBM_WRITER, 0, 0);
		break;
	case O_RDONLY:
		handle->main = gdbm_open(PDB_MAIN, 0, GDBM_READER, 0, 0);
		handle->name = gdbm_open(PDB_NAME, 0, GDBM_READER, 0, 0);
		break;
	default:
		return NULL;
	}

	if (!handle->main) {
		fprintf(stderr, "Error opening %s databases\n", PDB_MAIN);
		exit(1);
	}

	if (!handle->name) {
		fprintf(stderr, "Error opening %s databases\n", PDB_NAME);
		exit(1);
	}
	
	return handle;

}

/* NOTE: caller must NOT free or modify *name after call, 
   and may not use a returned pointer after this function
   is called again. 

   return 0 when no more records are present 
   return -1 on the special index record
*/
int PDB_db_nextkey(PDB_HANDLE h, int *id)
{
	static datum next_id;
	static datum idd;
	static int first = 1;

	if ( first ) {
		idd = gdbm_firstkey(h->main);
		if ( !idd.dptr )
			return 0;
		if ( idd.dsize != 4) {
			first = 0;
			return -1;
		}
		*id = ntohl(*(int *)(idd.dptr));
		first = 0;
		return 1;
	} else {
		next_id = gdbm_nextkey(h->main, idd);
		if ( idd.dptr ) 
			free(idd.dptr);
		idd = next_id; 
	}		

	if ( !next_id.dptr ) {
		first = 1;
		return 0;
	}
	*id = ntohl(*(int *)(next_id.dptr));
	return 1;
}




void PDB_db_close(PDB_HANDLE h)
{

	CODA_ASSERT(h && h->name && h->main);

	gdbm_close(h->name);
	gdbm_close(h->main);
	free(h);
}

void PDB_db_maxids(PDB_HANDLE h, int32_t *uid, int32_t *gid)
{
	datum d;
	datum f;
	char zero = 0;
	int32_t *ids;

	d.dsize = sizeof(zero);
	d.dptr = &zero;
	
	f = gdbm_fetch(h->main, d);

	if ( !f.dptr ) {
		*uid = 0; 
		*gid = 0;
	} else {
		CODA_ASSERT(f.dsize == 2*sizeof(int32_t));
		ids = (int32_t *) f.dptr;
		*uid = ntohl(ids[0]);
		*gid = ntohl(ids[1]);
	}

}


void PDB_db_update_maxids(PDB_HANDLE h, int32_t uid, int32_t gid, int mode)
{
	datum d;
	datum f;
	int rc;
	char zero = 0;
	int32_t olduid, oldgid;
	int32_t *ids = NULL;
	
	CODA_ASSERT( uid >= 0 && gid <= 0);

	d.dsize = sizeof(zero);
	d.dptr = &zero;
	
	f = gdbm_fetch(h->main, d);

	if ( !f.dptr ) {
		CODA_ASSERT( (uid == 0) && (gid == 0) );
		olduid = -1; 
		oldgid = 1;
		f.dsize = 2 * sizeof(int32_t);
		ids = malloc(f.dsize);
		f.dptr = (void *) ids;
	} else {		
		CODA_ASSERT(f.dsize == 2*sizeof(int32_t));
		ids = (int32_t *) f.dptr;
		olduid = ntohl(ids[0]);
		oldgid = ntohl(ids[1]);
		CODA_ASSERT(olduid >= 0 || oldgid <= 0); 
	}

	if(mode != PDB_MAXID_FORCE){
		if (  uid > olduid )
			ids[0] = htonl(uid);
		if (  gid < oldgid ) 
			ids[1] = htonl(gid);
	}
	else{
		ids[0] = htonl(uid);
		ids[1] = htonl(gid);
	}

	rc = gdbm_store(h->main, d, f, GDBM_REPLACE);
	CODA_ASSERT(rc == 0);

	free(ids);
}

void PDB_db_write(PDB_HANDLE h, int32_t id, char *name, void *buf)
{
	datum namerec, mainrec;
	int rc;
	datum *bufd;
	int32_t netid;

	CODA_ASSERT(id && name && buf);

	netid = htonl(id);
	bufd = (datum *)buf;

	namerec.dsize = strlen(name);
	namerec.dptr = name;

	mainrec.dsize = sizeof(netid);
	mainrec.dptr = (char *)&netid;

	rc = gdbm_store(h->main, mainrec, *bufd, GDBM_REPLACE); 
	CODA_ASSERT(rc == 0);

	rc = gdbm_store(h->name, namerec, mainrec, GDBM_REPLACE); 
	CODA_ASSERT(rc == 0);

	if (id > 0)
		PDB_db_update_maxids(h, id, 0, PDB_MAXID_SET);
	else
		PDB_db_update_maxids(h, 0, id, PDB_MAXID_SET);
}


void *PDB_db_read(PDB_HANDLE h, int32_t id, char *name)
{
	datum d;
	datum *foundid;
	int32_t realid;

	foundid = (datum *) malloc(sizeof(*foundid));
	CODA_ASSERT(foundid);

	realid = htonl(id);
	if ( name ) {
		d.dsize = strlen(name);
		d.dptr = name; 
		*foundid = gdbm_fetch(h->name, d);
		if (!foundid->dptr) 
			return (void *)foundid;
		realid = *(int32_t *)foundid->dptr;
		free(foundid->dptr);
	}
		
	d.dsize = sizeof(realid);
	d.dptr = (char *)&realid;
	*foundid = gdbm_fetch(h->main, d);

	return (void *)foundid;
}


void PDB_db_delete(PDB_HANDLE h, int32_t id, char *name)
{
	datum key; 
	int32_t realid;

	realid = htonl(id);
	key.dsize = sizeof(realid); 
	key.dptr = (char *)&realid;

	gdbm_delete(h->main, key);

	if (!name) 
		return;

	key.dsize = strlen(name); 
	key.dptr = name;

	gdbm_delete(h->name, key);
}


void PDB_db_delete_xfer(PDB_HANDLE h, char *name)
{
	datum key;

	CODA_ASSERT (name);
	key.dsize = strlen(name); 
	key.dptr = name;

	gdbm_delete(h->name, key);
}


int PDB_db_exists(void) 
{
	GDBM_FILE db;
	int rc1, rc2;
	struct stat buf;
	datum key;
	char zero = 0;
	int result;
   
	rc1 = stat(PDB_MAIN, &buf);
	rc2 = stat(PDB_NAME, &buf);
	if ( (rc1 && !rc2) || (!rc1 && rc2)) {
		fprintf(stderr, "One of %s and %s exists, the other not;"
			"please cleanup first\n", PDB_MAIN, PDB_NAME);
		exit(1);
	}

	/* things don't exist */
	if ( rc1 ) 
		return 0;
	
	/* check the sanity */
	
	/* this record has a special 1-byte key equal to zero */
	key.dsize = 1;
	key.dptr = &zero;

	/* open the profile database in read mode */
	db = gdbm_open(PDB_MAIN, 0, GDBM_READER, 0, 0);
	if(!db) {
		fprintf(stderr, "opening: %s\n", 
			gdbm_strerror(gdbm_errno));
		exit(1);
	}

	/* check if the record exists */
	result = gdbm_exists(db, key);
	gdbm_close(db);
   
	if (! result ) {
		fprintf(stderr, 
			"base PDB record does not exist. start over.\n");
		exit(1);
	}

	return 1;
}


void PDB_db_compact(PDB_HANDLE h) 
{
	gdbm_reorganize(h->main);
	gdbm_reorganize(h->name);
}


int PDB_setupdb(void)
{
	GDBM_FILE dbmain;
	GDBM_FILE dbname;
	PDB_HANDLE h;

	if ( PDB_db_exists() ) 
		return -EEXIST;
	
	dbmain = gdbm_open(PDB_MAIN, 0, GDBM_WRCREAT, 0600, NULL);
	dbname = gdbm_open(PDB_NAME, 0, GDBM_WRCREAT, 0600, NULL);

	if ( !dbmain || !dbname ) {
		fprintf(stderr, "Fatal error in PDB creation\n");
		exit(1);
	}

	gdbm_close(dbmain);
	gdbm_close(dbname);
	

	h = PDB_db_open(O_WRONLY);
	PDB_db_update_maxids(h, 0, 0, PDB_MAXID_FORCE);
	PDB_db_close(h);

	return 0;
}
