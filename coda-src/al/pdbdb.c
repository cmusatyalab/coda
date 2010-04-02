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
#include <coda_assert.h>
#include "rwcdb.h"
#include "pdb.h"

#include <vice_file.h>

#define PDB_MAIN vice_sharedfile("db/prot_users.cdb")

struct PDB_HANDLE_S {
	struct rwcdb main;
	int open_rw;
	int mutations;
};

/*
 * uid's and gid's are packed as:     uid > 0
 *                                    gid < 0
 *
 * I think it was a bad choice...
 *
 * ... Sigh. At least let's make the conversions easier:
 */
 
#define H2DB_ID(x)      (htonl((uint32_t) x))
#define DB2H_ID(x)      ((int32_t)ntohl(x))

static PDB_HANDLE pdb_handle = NULL;

PDB_HANDLE PDB_db_open(int mode)
{
	int rc;

	if (!pdb_handle) {
	    pdb_handle = malloc(sizeof(*pdb_handle));
	    CODA_ASSERT(pdb_handle);

	    memset(pdb_handle, 0, sizeof(*pdb_handle));

	    rc = rwcdb_init(&pdb_handle->main, PDB_MAIN, mode);
	    if (rc) {
		fprintf(stderr, "Error opening %s database\n", PDB_MAIN);
		exit(1);
	    }
	    pdb_handle->open_rw = (mode != O_RDONLY);
	    goto done;
	}

	if (mode == O_RDONLY) {
	    if (!pdb_handle->open_rw) {
		CODA_ASSERT(rwcdb_sync(&pdb_handle->main) != -1);
	    }
	    goto done;
	}

	/* ok we want to access the database for mutations... */
	if (!pdb_handle->open_rw) {
	    /* reopen the database with readwrite permissions */
	    CODA_ASSERT(rwcdb_free(&pdb_handle->main) == 1);
	    rc = rwcdb_init(&pdb_handle->main, PDB_MAIN, mode);
	    if (rc) {
		fprintf(stderr, "Error reopening %s database as readwrite\n",
			PDB_MAIN);
		exit(1);
	    }
	    pdb_handle->open_rw = 1;
	    goto done;
	}

	pdb_handle->mutations++;
	if ((pdb_handle->mutations % 128) == 0)
	    CODA_ASSERT(rwcdb_sync(&pdb_handle->main) == 1);
done:
	return pdb_handle;
}


/* check whether the on-disk image was updated and if it is, reopen the
 * database */
void PDB_db_reopen(PDB_HANDLE h) 
{
    CODA_ASSERT(rwcdb_sync(&pdb_handle->main) != -1);
}


/* NOTE: caller must NOT free or modify *id after call, 
   and may not use a returned pointer after this function
   is called again. 

   return 0 when no more records are present 
   return -1 on errors
*/
int PDB_db_nextkey(PDB_HANDLE h, int *id)
{
#define R_FIRST		0
#define R_NEXT		1
	static int which = R_FIRST;
	uint32_t uid, klen, dpos;
	int rc;

next:
	rc = rwcdb_next(&h->main, which == R_FIRST);
        if ( rc != 1) {
            which = R_FIRST;
            return 0;
        }

        which = R_NEXT;

	klen = rwcdb_keylen(&h->main);
	dpos = rwcdb_datapos(&h->main);

        if ( klen != sizeof(uint32_t) )
	    goto next;

	rc = rwcdb_readkey(&h->main, (char *)&uid, klen, dpos);
	if (rc) return -1;

        *id = DB2H_ID(uid);
	return 1;
}


void PDB_db_close(PDB_HANDLE h)
{
    return;
}

void PDB_db_release(void)
{
    if (!pdb_handle)
	return;

    rwcdb_free(&pdb_handle->main);
    free(pdb_handle);
    pdb_handle = NULL;
}

void PDB_db_maxids(PDB_HANDLE h, int32_t *uid, int32_t *gid)
{
	char zero = 0;
	uint32_t ids[2], dlen, dpos;
        int rc;

	*uid = 0; 
	*gid = 0;

        rc = rwcdb_find(&h->main, &zero, 0);
	if (rc != 1) return;

	dlen = rwcdb_datalen(&h->main);
	dpos = rwcdb_datapos(&h->main);

	CODA_ASSERT(dlen == 2 * sizeof(uint32_t));

	rc = rwcdb_read(&h->main, (char *)&ids, dlen, dpos);
	CODA_ASSERT(rc != -1);

	*uid = DB2H_ID(ids[0]);
	*gid = DB2H_ID(ids[1]);
}


void PDB_db_update_maxids(PDB_HANDLE h, int32_t uid, int32_t gid, int mode)
{
	int rc;
	char zero = 0;
	int32_t olduid, oldgid;
	uint32_t ids[2];
	
	CODA_ASSERT(uid >= 0 && gid <= 0);

	PDB_db_maxids(h, &olduid, &oldgid);
	CODA_ASSERT(olduid >= 0 || oldgid <= 0); 

	if (mode == PDB_MAXID_FORCE || uid > olduid)
	     ids[0] = H2DB_ID(uid);
	else ids[0] = H2DB_ID(olduid);

	if (mode == PDB_MAXID_FORCE || gid < oldgid)
	     ids[1] = H2DB_ID(gid);
	else ids[1] = H2DB_ID(oldgid);

        rc = rwcdb_insert(&h->main, &zero, 0,
			  (char *)&ids, 2 * sizeof(uint32_t));
	CODA_ASSERT(rc == 1);
}

static char *malloc_name(const char *name)
{
	char *namekey;
	uint32_t namelen;

	namelen = strlen("NAME") + strlen(name) + 1;
	namekey = malloc(namelen);
	CODA_ASSERT(namekey);

	strcpy(namekey, "NAME");
	strcpy(&namekey[4], name);

	return namekey;
}

void PDB_db_write(PDB_HANDLE h, int32_t id, char *name, void *data, size_t size)
{
	uint32_t netid;
	char *namekey;
	int rc;

	CODA_ASSERT(id && name && data);

	netid = H2DB_ID(id);

        rc = rwcdb_insert(&h->main, (char *)&netid, sizeof(netid),
			  (char *)data, size);
	CODA_ASSERT(rc == 1);

	namekey = malloc_name(name);
	rc = rwcdb_insert(&h->main, namekey, strlen(namekey),
			  (char *)&netid, sizeof(netid));
	free(namekey);

	CODA_ASSERT(rc == 1);

	if (id > 0) PDB_db_update_maxids(h, id, 0, PDB_MAXID_SET);
	else        PDB_db_update_maxids(h, 0, id, PDB_MAXID_SET);

	/* This frees the memory allocated in pdb_pack */
	free(data);
}


void PDB_db_read(PDB_HANDLE h, int32_t id, const char *name, void **data,
		 size_t *size)
{
	uint32_t realid, dlen, dpos;
	int rc;

	*data = NULL;
	*size = 0;

	realid = H2DB_ID(id);
	if ( name ) {
		char *namekey = malloc_name(name);

		rc = rwcdb_find(&h->main, namekey, strlen(namekey));
		free(namekey);
		if (rc != 1) return;

		dlen = rwcdb_datalen(&h->main);
		if (dlen != sizeof(uint32_t)) return;

		dpos = rwcdb_datapos(&h->main);
                if (rwcdb_read(&h->main, (char *)&realid, dlen, dpos) == -1)
		    return;
	}
		
	rc = rwcdb_find(&h->main, (char *)&realid, sizeof(uint32_t));
	if (rc != 1) return;

	dlen = rwcdb_datalen(&h->main);
	if (dlen == sizeof(uint32_t))
	    return;

	*data = malloc(dlen);
	if (!*data) return;

	dpos = rwcdb_datapos(&h->main);
	if (rwcdb_read(&h->main, *data, dlen, dpos) == -1) {
	    free(*data);
	    *data = NULL;
	    return;
	}
	*size = dlen;
	return;
}


void PDB_db_delete(PDB_HANDLE h, int32_t id, char *name)
{
	uint32_t realid = H2DB_ID(id);

	rwcdb_delete(&h->main, (char *)&realid, sizeof(uint32_t));

	PDB_db_delete_xfer(h, name);
}


void PDB_db_delete_xfer(PDB_HANDLE h, char *name)
{
	char *namekey;

	if (!name) return;

	namekey = malloc_name(name);
	rwcdb_delete(&h->main, namekey, strlen(namekey));
	free(namekey);
}


int PDB_db_exists(void) 
{
	struct stat buf;
	return (stat(PDB_MAIN, &buf) == 0);
}


void PDB_db_compact(PDB_HANDLE h) 
{
	return;
}

int PDB_setupdb(void)
{
	PDB_HANDLE h;

	if (PDB_db_exists())
	    return 0;

	h = PDB_db_open(O_RDWR);
	PDB_db_update_maxids(h, 0, 0, PDB_MAXID_SET);
	PDB_db_close(h);

	return 0;
}

