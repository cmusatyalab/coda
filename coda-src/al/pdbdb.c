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
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#ifdef HAVE_DB_185_H
#include <db_185.h>
#else 
#include <db.h>
#endif


#include <coda_assert.h>
#include "pdb.h"

#define PDB_MAIN "/vice/db/prot_users.db"
#define PDB_NAME "/vice/db/prot_index.db"

struct PDB_HANDLE_S {
	DB *main;
	DB *name;
};

static BTREEINFO btreeinfo = { 0, 0, 0, 0, 1024, NULL, NULL, 4321 };


PDB_HANDLE PDB_db_open(int mode)
{
	struct PDB_HANDLE_S *handle;

	handle = malloc(sizeof(*handle));
	CODA_ASSERT(handle);

	memset(handle, 0, sizeof(*handle));

	switch (mode) {
	case O_RDWR: 
	case O_WRONLY:
		handle->main = dbopen(PDB_MAIN, O_RDWR, 0600, DB_BTREE,
                                      &btreeinfo);
		handle->name = dbopen(PDB_NAME, O_RDWR, 0600, DB_BTREE,
                                      &btreeinfo);
		break;
	case O_RDONLY:
		handle->main = dbopen(PDB_MAIN, O_RDONLY, 0600, DB_BTREE,
                                      &btreeinfo);
		handle->name = dbopen(PDB_NAME, O_RDONLY, 0600, DB_BTREE,
                                      &btreeinfo);
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
	static int which = R_FIRST;
	DBT key;
        int rc;

	memset(&key, 0, sizeof(DBT));

        rc = h->main->seq(h->main, &key, NULL, which);

        if ( rc != RET_SUCCESS ) {
            which = R_FIRST;
            return 0;
        }

        which = R_NEXT;

        if ( key.size != 4 )
            return -1;

        *id = ntohl(*(int *)(key.data));

	return 1;
}


void PDB_db_close(PDB_HANDLE h)
{
	CODA_ASSERT(h && h->name && h->main);

	h->name->close(h->name);
	h->main->close(h->main);
	free(h);
}

void PDB_db_maxids(PDB_HANDLE h, int32_t *uid, int32_t *gid)
{
	DBT key, value;
	char zero = 0;
	int32_t *ids;
        int rc;

	memset(&key, 0, sizeof(DBT));
	key.size = sizeof(zero);
	key.data = &zero;
	
	memset(&value, 0, sizeof(DBT));

        rc = h->main->get(h->main, &key, &value, 0);

	if ( rc != RET_SUCCESS) {
		*uid = 0; 
		*gid = 0;
	} else {
		CODA_ASSERT(value.size == 2*sizeof(int32_t));
		ids = (int32_t *) value.data;
		*uid = ntohl(ids[0]);
		*gid = ntohl(ids[1]);
	}
}


void PDB_db_update_maxids(PDB_HANDLE h, int32_t uid, int32_t gid, int mode)
{
	DBT key, value;
	int rc;
	char zero = 0;
	int32_t olduid, oldgid;
	int32_t ids[2];
	
	CODA_ASSERT(uid >= 0 && gid <= 0);

	memset(&key, 0, sizeof(DBT));
	key.size = sizeof(zero);
	key.data = &zero;

	memset(&value, 0, sizeof(DBT));
	
        rc = h->main->get(h->main, &key, &value, 0);

	if ( rc != RET_SUCCESS ) {
		CODA_ASSERT( (uid == 0) && (gid == 0) );
		ids[0] = htonl(0);
		ids[1] = htonl(0);
	} else {
		CODA_ASSERT(value.size == 2*sizeof(int32_t));
		ids[0] = ((int32_t *)value.data)[0];
		ids[1] = ((int32_t *)value.data)[1];
	}
	olduid = ntohl(ids[0]);
	oldgid = ntohl(ids[1]);
	CODA_ASSERT(olduid >= 0 || oldgid <= 0); 

	if ( mode != PDB_MAXID_FORCE ) {
		if (  uid > olduid )
			ids[0] = htonl(uid);
		if (  gid < oldgid ) 
			ids[1] = htonl(gid);
	}
	else{
		ids[0] = htonl(uid);
		ids[1] = htonl(gid);
	}

	value.size = 2 * sizeof(int32_t);
	value.data = (void *) &ids;

        rc = h->main->put(h->main, &key, &value, 0);
	CODA_ASSERT(rc == RET_SUCCESS);
}

void PDB_db_write(PDB_HANDLE h, int32_t id, char *name, void *data, size_t size)
{
	DBT namerec, mainrec, dbdata;
	int32_t netid;
	int rc;

	CODA_ASSERT(id && name && data);

	netid = htonl(id);

	memset(&mainrec, 0, sizeof(DBT));
	mainrec.size = sizeof(netid);
	mainrec.data = (char *)&netid;

	memset(&namerec, 0, sizeof(DBT));
	namerec.size = strlen(name);
	namerec.data = name;

	memset(&dbdata, 0, sizeof(DBT));
	dbdata.size = size;
	dbdata.data = data;

        rc = h->main->put(h->main, &mainrec, &dbdata, 0);
	CODA_ASSERT(rc == RET_SUCCESS);

        rc = h->name->put(h->name, &namerec, &mainrec, 0);
	CODA_ASSERT(rc == RET_SUCCESS);

	if (id > 0)
		PDB_db_update_maxids(h, id, 0, PDB_MAXID_SET);
	else
		PDB_db_update_maxids(h, 0, id, PDB_MAXID_SET);

	/* This frees the memory allocated in pdb_pack */
	free(data);
}


void PDB_db_read(PDB_HANDLE h, int32_t id, char *name, void **data,size_t *size)
{
	DBT key, value;
	int32_t realid;
	int rc;

	realid = htonl(id);
	if ( name ) {
		memset(&key, 0, sizeof(DBT));
		key.size = strlen(name);
		key.data = name; 

		memset(&value, 0, sizeof(DBT));

                rc = h->name->get(h->name, &key, &value, 0);
		if ( rc != RET_SUCCESS ) {
			*data = NULL;
			*size = 0;
			return;
                }
		realid = *(int32_t *)value.data;
	}
		
	memset(&key, 0, sizeof(DBT));
	key.size = sizeof(realid);
	key.data = (char *)&realid;

	memset(&value, 0, sizeof(DBT));

        rc = h->main->get(h->main, &key, &value, 0);
        if ( rc != RET_SUCCESS ) {
		*data = NULL;
		*size = 0;
		return;
        }

	*data = malloc(value.size);
	CODA_ASSERT(*data);

	memcpy(*data, value.data, value.size);
	*size = value.size;
	return;
}


void PDB_db_delete(PDB_HANDLE h, int32_t id, char *name)
{
	DBT key; 
	int32_t realid;

	realid = htonl(id);

	memset(&key, 0, sizeof(DBT));
	key.size = sizeof(realid); 
	key.data = (char *)&realid;

        h->main->del(h->main, &key, 0);

	if (!name) return;

	memset(&key, 0, sizeof(DBT));
	key.size = strlen(name); 
	key.data = name;

        h->name->del(h->name, &key, 0);
}


void PDB_db_delete_xfer(PDB_HANDLE h, char *name)
{
	DBT key;

	CODA_ASSERT (name);
	key.size = strlen(name); 
	key.data = name;

        h->name->del(h->name, &key, 0);
}


int PDB_db_exists(void) 
{
	DB *db;
	int rc1, rc2;
	struct stat buf;
	DBT key, value;
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
	memset(&key, 0, sizeof(DBT));
	key.size = sizeof(zero);
	key.data = &zero;

	/* open the profile database in read mode */
        db = dbopen(PDB_MAIN, O_RDONLY, 0600, DB_BTREE, &btreeinfo);
	if(!db) {
		fprintf(stderr, "Error opening %s: %s\n", 
			PDB_MAIN, strerror(errno));
		exit(1);
	}

	/* check if the record exists */
        result = db->get(db, &key, &value, 0);
        db->close(db);
   
	if ( result != RET_SUCCESS ) {
		fprintf(stderr, 
			"base PDB record does not exist. start over.\n");
		exit(1);
	}

	return 1;
}


void PDB_db_compact(PDB_HANDLE h) 
{
#if 0
	gdbm_reorganize(h->main);
	gdbm_reorganize(h->name);
#endif
}


int PDB_setupdb(void)
{
	DB *dbmain, *dbname;
	PDB_HANDLE h;

	if ( PDB_db_exists() ) 
		return -EEXIST;
	
        dbmain = dbopen(PDB_MAIN, O_RDWR | O_CREAT | O_EXCL, 0600, DB_BTREE,
                        &btreeinfo);
        dbname = dbopen(PDB_NAME, O_RDWR | O_CREAT | O_EXCL, 0600, DB_BTREE,
                        &btreeinfo);

	if ( !dbmain || !dbname ) {
		fprintf(stderr, "Fatal error in PDB creation\n");
		exit(1);
	}

        dbmain->close(dbmain);
        dbname->close(dbname);

	h = PDB_db_open(O_WRONLY);
	PDB_db_update_maxids(h, 0, 0, PDB_MAXID_FORCE);
	PDB_db_close(h);

	return 0;
}
