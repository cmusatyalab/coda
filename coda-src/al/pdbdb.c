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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include "coda_db.h"
#include <coda_assert.h>
#include "pdb.h"

#include <vice_file.h>

#define PDB_MAIN vice_sharedfile("db/prot_users")
#define PDB_NAME vice_sharedfile("db/prot_index")

struct PDB_HANDLE_S {
	db_type *main;
	db_type *name;
};

#ifndef HAVE_NDBM
static BTREEINFO btreeinfo = { 0, 0, 0, 0, 1024, NULL, NULL, 4321 };
#endif

/*
 * uid's and gid's are packed as:     uid > 0
 *                                    gid < 0
 *
 * I think it was a bad choice...
 *
 * ... Sigh. At least let's make the conversions easier:
 */
 
#define H2DB_ID(x)      (htonl((u_int32_t) x))
#define DB2H_ID(x)      ((int32_t)ntohl(x))

PDB_HANDLE PDB_db_open(int mode)
{
	struct PDB_HANDLE_S *handle;
	char pdb_main[MAXPATHLEN];
	char pdb_name[MAXPATHLEN];

	strcpy(pdb_main, PDB_MAIN);
	strcpy(pdb_name, PDB_NAME);

	handle = malloc(sizeof(*handle));
	CODA_ASSERT(handle);

	memset(handle, 0, sizeof(*handle));

#ifndef HAVE_NDBM
	strcat(pdb_main, ".db");
	strcat(pdb_name, ".db");
#endif
	switch (mode) {
	case O_RDWR: 
	case O_WRONLY:
		handle->main = db_open(pdb_main, O_RDWR, 0600, DB_BTREE,
                                       &btreeinfo);
		handle->name = db_open(pdb_name, O_RDWR, 0600, DB_BTREE,
                                       &btreeinfo);
		break;
	case O_RDONLY:
		handle->main = db_open(pdb_main, O_RDONLY, 0600, DB_BTREE,
                                       &btreeinfo);
		handle->name = db_open(pdb_name, O_RDONLY, 0600, DB_BTREE,
                                       &btreeinfo);
		break;
	default:
		return NULL;
	}

	if (!handle->main) {
		fprintf(stderr, "Error opening %s databases\n", pdb_main);
		exit(1);
	}

	if (!handle->name) {
		fprintf(stderr, "Error opening %s databases\n", pdb_name);
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
#ifdef HAVE_NDBM
#define R_FIRST		0
#define R_NEXT		1
#endif
	static int which = R_FIRST;
	db_data key, content;
	u_int32_t uid;
        int rc;

	memset(&key, 0, sizeof(db_data));

	if (which == R_FIRST) {
	    db_first(h->main, &key, &content, rc);
	} else {
	    db_next(h->main, &key, &content, rc);
	}

        if ( rc != RET_SUCCESS ) {
            which = R_FIRST;
            return 0;
        }

        which = R_NEXT;

        if ( key.db_datasize != 4 )
            return -1;

	memcpy(&uid, key.db_dataptr, sizeof(u_int32_t));
        *id = DB2H_ID(uid);

	return 1;
}


void PDB_db_close(PDB_HANDLE h)
{
	CODA_ASSERT(h && h->name && h->main);

	db_close(h->name);
	db_close(h->main);
	free(h);
}

void PDB_db_maxids(PDB_HANDLE h, int32_t *uid, int32_t *gid)
{
	db_data key, value;
	char zero = 0;
	u_int32_t ids[2];
        int rc;

	memset(&key, 0, sizeof(db_data));
	key.db_datasize = sizeof(zero);
	key.db_dataptr = &zero;
	
	memset(&value, 0, sizeof(db_data));

        db_get(h->main, &key, &value, 0, rc);

	if ( rc != RET_SUCCESS) {
		*uid = 0; 
		*gid = 0;
	} else {
		CODA_ASSERT(value.db_datasize == 2*sizeof(u_int32_t));
		memcpy(ids, value.db_dataptr, 2*sizeof(u_int32_t));
		*uid = DB2H_ID(ids[0]);
		*gid = DB2H_ID(ids[1]);
	}
}


void PDB_db_update_maxids(PDB_HANDLE h, int32_t uid, int32_t gid, int mode)
{
	db_data key, value;
	int rc;
	char zero = 0;
	int32_t olduid, oldgid;
	u_int32_t ids[2];
	u_int32_t init_ids[2] = { htonl(0), htonl(0) };
	
	CODA_ASSERT(uid >= 0 && gid <= 0);

	memset(&key, 0, sizeof(db_data));
	key.db_datasize = sizeof(zero);
	key.db_dataptr = &zero;

	memset(&value, 0, sizeof(db_data));
	
        db_get(h->main, &key, &value, 0, rc);

	if ( rc != RET_SUCCESS ) {
		CODA_ASSERT( (uid == 0) && (gid == 0) );
		memcpy(ids, init_ids, 2*sizeof(u_int32_t));
	} else {
		CODA_ASSERT(value.db_datasize == 2*sizeof(u_int32_t));
		memcpy(ids, value.db_dataptr, 2*sizeof(u_int32_t));
	}
	olduid = DB2H_ID(ids[0]);
	oldgid = DB2H_ID(ids[1]);
	CODA_ASSERT(olduid >= 0 || oldgid <= 0); 

	if ( mode != PDB_MAXID_FORCE ) {
		if (  uid > olduid )
			ids[0] = H2DB_ID(uid);
		if (  gid < oldgid ) 
			ids[1] = H2DB_ID(gid);
	}
	else{
		ids[0] = H2DB_ID(uid);
		ids[1] = H2DB_ID(gid);
	}

	value.db_datasize = 2 * sizeof(u_int32_t);
	value.db_dataptr = (char *) ids;

        rc = db_put(h->main, &key, &value, DB_PUT_FLAG);
	CODA_ASSERT(rc == RET_SUCCESS);
}

void PDB_db_write(PDB_HANDLE h, int32_t id, char *name, void *data, size_t size)
{
	db_data namerec, mainrec, dbdata;
	u_int32_t netid;
	int rc;

	CODA_ASSERT(id && name && data);

	netid = H2DB_ID(id);

	memset(&mainrec, 0, sizeof(db_data));
	mainrec.db_datasize = sizeof(netid);
	mainrec.db_dataptr = (char *)&netid;

	memset(&namerec, 0, sizeof(db_data));
	namerec.db_datasize = strlen(name);
	namerec.db_dataptr = name;

	memset(&dbdata, 0, sizeof(db_data));
	dbdata.db_datasize = size;
	dbdata.db_dataptr = data;

        rc = db_put(h->main, &mainrec, &dbdata, DB_PUT_FLAG);
	CODA_ASSERT(rc == RET_SUCCESS);

        rc = db_put(h->name, &namerec, &mainrec, DB_PUT_FLAG);
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
	db_data key, value;
	u_int32_t realid;
	int rc;

	realid = H2DB_ID(id);
	if ( name ) {
		memset(&key, 0, sizeof(db_data));
		key.db_datasize = strlen(name);
		key.db_dataptr = name; 

		memset(&value, 0, sizeof(db_data));

                db_get(h->name, &key, &value, 0, rc);
		if ( rc != RET_SUCCESS ) {
			*data = NULL;
			*size = 0;
			return;
                }
		memcpy(&realid, value.db_dataptr, sizeof(u_int32_t));
	}
		
	memset(&key, 0, sizeof(db_data));
	key.db_datasize = sizeof(realid);
	key.db_dataptr = (char *)&realid;

	memset(&value, 0, sizeof(db_data));

        db_get(h->main, &key, &value, 0, rc);
        if ( rc != RET_SUCCESS ) {
		*data = NULL;
		*size = 0;
		return;
        }

	*data = malloc(value.db_datasize);
	CODA_ASSERT(*data);

	memcpy(*data, value.db_dataptr, value.db_datasize);
	*size = value.db_datasize;
	return;
}


void PDB_db_delete(PDB_HANDLE h, int32_t id, char *name)
{
	db_data key; 
	u_int32_t realid;

	realid = H2DB_ID(id);

	memset(&key, 0, sizeof(db_data));
	key.db_datasize = sizeof(realid); 
	key.db_dataptr = (char *)&realid;

        db_del(h->main, &key, 0);

	if (!name) return;

	memset(&key, 0, sizeof(db_data));
	key.db_datasize = strlen(name); 
	key.db_dataptr = name;

        db_del(h->name, &key, 0);
}


void PDB_db_delete_xfer(PDB_HANDLE h, char *name)
{
	db_data key;

	CODA_ASSERT (name);
	key.db_datasize = strlen(name); 
	key.db_dataptr = name;

        db_del(h->name, &key, 0);
}


int PDB_db_exists(void) 
{
	db_type *db;
	int rc1, rc2;
	struct stat buf;
	db_data key, value;
	char zero = 0;
	int result;
	char pdb_main[MAXPATHLEN];
	char pdb_name[MAXPATHLEN];
	
	strcpy(pdb_main, PDB_MAIN);
	strcpy(pdb_name, PDB_NAME);

#ifdef HAVE_NDBM
	strcat(pdb_main, ".dir");
	strcat(pdb_name, ".dir");
#else
	strcat(pdb_main, ".db");
	strcat(pdb_name, ".db");
#endif   
	rc1 = stat(pdb_main, &buf);
	rc2 = stat(pdb_name, &buf);
	if ( (rc1 && !rc2) || (!rc1 && rc2)) {
		fprintf(stderr, "One of %s and %s exists, the other not;"
			"please cleanup first\n", pdb_main, pdb_name);
		exit(1);
	}

	/* things don't exist */
	if ( rc1 ) 
		return 0;
	
	/* check the sanity */

	/* this record has a special 1-byte key equal to zero */
	memset(&key, 0, sizeof(db_data));
	key.db_datasize = sizeof(zero);
	key.db_dataptr = &zero;

	/* open the profile database in read mode */
#ifdef HAVE_NDBM
        db = db_open(PDB_MAIN, O_RDONLY, 0600, DB_BTREE, &btreeinfo);
#else
        db = db_open(pdb_main, O_RDONLY, 0600, DB_BTREE, &btreeinfo);
#endif
	if(!db) {
		fprintf(stderr, "Error opening %s: %s\n", 
			pdb_main, strerror(errno));
		exit(1);
	}

	/* check if the record exists */
        db_get(db, &key, &value, 0, result);
        db_close(db);
   
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
	db_type *dbmain, *dbname;
	PDB_HANDLE h;
	char pdb_main[MAXPATHLEN];
	char pdb_name[MAXPATHLEN];

	strcpy(pdb_main, PDB_MAIN);
	strcpy(pdb_name, PDB_NAME);

#ifndef HAVE_NDBM
	strcat(pdb_main, ".db");
	strcat(pdb_name, ".db");
#endif   

	if ( PDB_db_exists() ) 
		return -EEXIST;
	
        dbmain = db_open(pdb_main, O_RDWR | O_CREAT | O_EXCL, 0600, DB_BTREE,
                        &btreeinfo);
        dbname = db_open(pdb_name, O_RDWR | O_CREAT | O_EXCL, 0600, DB_BTREE,
                        &btreeinfo);

	if ( !dbmain || !dbname ) {
		fprintf(stderr, "Fatal error in PDB creation\n");
		exit(1);
	}

        db_close(dbmain);
        db_close(dbname);

	h = PDB_db_open(O_WRONLY);
	PDB_db_update_maxids(h, 0, 0, PDB_MAXID_FORCE);
	PDB_db_close(h);

	return 0;
}
