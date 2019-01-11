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

#include <stdlib.h>
#include "coda_string.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <coda_assert.h>
#include "pdb.h"

void pdb_pack(PDB_profile *r, void **data, size_t *size)
{
    int32_t *tmp;
    int off, len, tmpsize;

    tmpsize = 2 + /* id + strlen(name) */
              2 + /* owner id + strlen(owner_name) */
              pdb_array_size(&r->member_of) + 1 + pdb_array_size(&r->cps) + 1 +
              pdb_array_size(&r->groups_or_members) + 1;

    /* WTF we pad 4 times as much as needed for the strings!!!
	 * too late to transparently change now. */
#if 1
#define ALIGN(x) (x)
#else
/* we really want to pad our strings like this... it saves about 120KiB
 * for 5000 users with 8 letter names */
#define ALIGN(x) (((x) + sizeof(int32_t) - 1) / sizeof(int32_t))
#endif
    if (r->name)
        tmpsize += ALIGN(strlen(r->name));
    if (r->owner_name)
        tmpsize += ALIGN(strlen(r->owner_name));

    tmp = (int32_t *)malloc(tmpsize * sizeof(int32_t));
    CODA_ASSERT(tmp);

    /* Pack the id and name */
    tmp[0] = htonl(r->id);
    if (r->name != NULL) {
        /* Have to store length because may be zero length */
        len    = strlen(r->name) * sizeof(char);
        tmp[1] = htonl(len);
        /* Convert to network layer while copying */
        memcpy((char *)&tmp[2], r->name, len);
        off = 2 + ALIGN(len);
    } else {
        tmp[1] = 0;
        off    = 2;
    }

    /* Pack the owner id and name */
    tmp[off++] = htonl(r->owner_id);
    if (r->owner_name != NULL) {
        len = strlen(r->owner_name) * sizeof(char);
        /* Have to store length because may be zero length */
        tmp[off++] = htonl(len);
        memcpy((char *)&tmp[off], r->owner_name, len);
        off += ALIGN(len);
    } else
        tmp[off++] = 0;

    /* Pack the lists */
    off += pdb_array_pack(&tmp[off], &(r->member_of));
    off += pdb_array_pack(&tmp[off], &(r->cps));
    off += pdb_array_pack(&tmp[off], &(r->groups_or_members));

    CODA_ASSERT(off == tmpsize);

    *data = (char *)tmp;
    *size = tmpsize * sizeof(int32_t);
}

void pdb_unpack(PDB_profile *r, void *data, size_t size)
{
    int32_t *tmp = (int32_t *)data;
    unsigned int off, len;

    if (size == 0) {
        r->id = 0;
        if (data)
            free(data);
        return;
    }
    /* Unpack the id and name */
    r->id = ntohl(tmp[0]);
    off   = 1;
    len   = ntohl(tmp[off]);
    off++;
    r->name = malloc(len + 1);
    memcpy(r->name, (char *)&tmp[off], len);
    r->name[len] = '\0';
    off += ALIGN(len);

    /* Unpack the owner id and name */
    r->owner_id = ntohl(tmp[off]);
    off++;
    len = ntohl(tmp[off]);
    off++;
    r->owner_name = malloc(len + 1);
    memcpy(r->owner_name, (char *)&tmp[off], ntohl(tmp[off - 1]));
    r->owner_name[len] = '\0';
    off += ALIGN(len);

    /* Unpack the lists */
    off += pdb_array_unpack(&tmp[off], &(r->member_of));
    off += pdb_array_unpack(&tmp[off], &(r->cps));
    off += pdb_array_unpack(&tmp[off], &(r->groups_or_members));

    CODA_ASSERT(off <= size);

    /* Here we free the data allocated by PDB_db_read */
    if (data)
        free(data);
}
