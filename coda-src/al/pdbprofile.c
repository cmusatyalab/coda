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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <coda_assert.h>
#include "pdb.h"
#include "prs.h"

void PDB_freeProfile(PDB_profile *r)
{
	/* sanity check arguments */
	CODA_ASSERT(r);

	if(r->id == 0) return;

	/* Set id's to 0 since we can't free the actual structure here */
	r->id=0;
	r->owner_id=0;

	/* free space used for name field */
	if (r->name != NULL)
		free(r->name);
   
	/* free space used for owner_name field */
	if (r->owner_name != NULL)
		free(r->owner_name);
      
	/* free space used by lists */   
	pdb_array_free(&(r->member_of));
	pdb_array_free(&(r->cps));
	pdb_array_free(&(r->groups_or_members));
}


void PDB_writeProfile(PDB_HANDLE h, PDB_profile *r)
{
	void *d;
   
	/* sanity check arguments */
	CODA_ASSERT(r && h);
   
	/* pack the record */
	pdb_pack(r, &d);

	PDB_db_write(h, r->id, r->name, d);

	free(d);
}


void PDB_readProfile(PDB_HANDLE h, int32_t id, PDB_profile *r)
{
	void *data;

	/* sanity check arguments */
	CODA_ASSERT(r && h);

	data=PDB_db_read(h, id, NULL);

	CODA_ASSERT(data);

	pdb_unpack(r,data);
}


void PDB_readProfile_byname(PDB_HANDLE h, char *name, PDB_profile *r)
{
	void *data;

	/* sanity check arguments */
	CODA_ASSERT(r && h);

	data=PDB_db_read(h, 0, name);

	CODA_ASSERT(data);
	
	pdb_unpack(r,data);
}


void PDB_deleteProfile(PDB_HANDLE h, PDB_profile *r)
{
	/* sanity check arguments */
	CODA_ASSERT(r && h);

	PDB_db_delete(h, r->id, r->name);
}


void PDB_printProfile(FILE *out, PDB_profile *r)
{
	char tmp[1024];

	if (r == NULL) {
		fprintf(out, "# You tried to print a NULL pointer\n");
		return;
	}
	if (r->id == 0){
		fprintf(out, "The record is empty\n");
		return;
	}

	/* print header and name */
	if(PDB_ISGROUP(r->id))
		fprintf(out, "GROUP %s OWNED BY %s\n", r->name, r->owner_name);
	else
		fprintf(out, "USER %s\n", r->name);
		
	fprintf(out, "  *  id: %d\n", r->id);
	if (PDB_ISGROUP(r->id))
		fprintf(out, "  *  owner id: %d\n", r->owner_id);
      
	if (r->member_of.size > 0) {
		pdb_array_snprintf(tmp, &(r->member_of), 1024);
		fprintf(out, "  *  belongs to groups: [ %s]\n",tmp);
	}
	else
		fprintf(out, "  *  belongs to no groups\n");

	if (r->cps.size > 0) {
		pdb_array_snprintf(tmp, &(r->cps), 1024);
		fprintf(out, "  *  cps: [ %s]\n",tmp);
	}
	else
		fprintf(out, "  *  has no cps\n");

	if (r->groups_or_members.size > 0) {
		pdb_array_snprintf(tmp, &(r->groups_or_members), 1024);
		if (PDB_ISGROUP(r->id))
			fprintf(out, "  *  has members: [ %s]\n",tmp);
		else
			fprintf(out, "  *  owns groups: [ %s]\n",tmp);
	}
	else {
		if (PDB_ISGROUP(r->id))
			fprintf(out, "  *  has no members\n");
		else
			fprintf(out, "  *  owns no groups\n");
	}
}


/* Updates the CPS entry of the given id */
void PDB_updateCpsSelf(PDB_HANDLE h, PDB_profile *r)
{
	PDB_profile p;
	int32_t nextid;
	pdb_array_off off;

	CODA_ASSERT(r != NULL);

	pdb_array_free(&(r->cps));
	/* Add the CPS of parents into list */
	nextid = pdb_array_head(&(r->member_of), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		if(p.id != 0)
			pdb_array_merge(&(r->cps), &(p.cps));
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r->member_of), &off);
	}
	/* Add self to list */
	pdb_array_add(&(r->cps), r->id);
}

/* Updates the CPS entries of the given id's children's CPS entries */
void PDB_updateCpsChildren(PDB_HANDLE h, PDB_profile *r)
{
	PDB_profile c;
	int32_t nextid;
	pdb_array_off off;

	CODA_ASSERT(r != NULL);

	/* Add the CPS of parents into list */
	nextid = pdb_array_head(&(r->groups_or_members), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &c);
		/* Resurse through all children */
		if(PDB_ISGROUP(c.id))
			PDB_updateCps(h, &c);
		else
			PDB_updateCpsSelf(h, &c);
		PDB_freeProfile(&c);
		nextid = pdb_array_next(&(r->groups_or_members), &off);
	}
}

/* Updates the CPS entries of the given id and the CPS entries of
   its children */
void PDB_updateCps(PDB_HANDLE h, PDB_profile *r)
{
	PDB_updateCpsSelf(h,r);
	PDB_updateCpsChildren(h,r);
}

