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
#include <sys/stat.h>
#include <fcntl.h>
#include <coda_assert.h>
#include "pdb.h"
#include "prs.h"

void PDB_addToGroup(int32_t id, int32_t groupId)
{
	PDB_HANDLE h;
	PDB_profile r;
   
	/* sanity check arguments */
	CODA_ASSERT(PDB_ISGROUP(groupId) && (id != 0));

	h = PDB_db_open(O_RDWR);

	/* add id to the group's member list */
	PDB_readProfile(h, groupId, &r);
	CODA_ASSERT(r.id != 0);
	pdb_array_add(&(r.groups_or_members), id);
	PDB_writeProfile(h, &r);
	PDB_freeProfile(&r);
   
	/* add groupId to user's member_of list */
	PDB_readProfile(h, id, &r);
	CODA_ASSERT(r.id != 0);
	pdb_array_add(&(r.member_of), groupId);
	PDB_updateCpsSelf(h, &r);
	PDB_writeProfile(h, &r);
	PDB_freeProfile(&r);

	PDB_db_close(h);
}


void PDB_removeFromGroup(int32_t id, int32_t groupId)
{
	PDB_HANDLE h;
	PDB_profile r;
   
	/* sanity check arguments (groupId must be < 0) */
	CODA_ASSERT(PDB_ISGROUP(groupId) && PDB_ISUSER(id));

	h = PDB_db_open(O_RDWR);

	/* add id to the group's member list */
	PDB_readProfile(h, groupId, &r);
	CODA_ASSERT(r.id != 0);
	pdb_array_del(&(r.groups_or_members), id);
	PDB_writeProfile(h, &r);
	PDB_freeProfile(&r);
   
	/* add groupId to user's member_of list */
	PDB_readProfile(h, id, &r);
	CODA_ASSERT(r.id != 0);
	pdb_array_del(&(r.member_of), groupId);
	PDB_updateCps(h, &r);
	PDB_writeProfile(h, &r);
	PDB_freeProfile(&r);

	PDB_db_close(h);
}


void PDB_changeName(int32_t id, char *name)
{
	PDB_HANDLE h;
	PDB_profile r,p;
	pdb_array_off off;
	int32_t nextid;

	/* sanity check arguments */
	CODA_ASSERT(name);

	h = PDB_db_open(O_RDWR);

	/* add id to the group's member list */
	PDB_readProfile(h, id, &r);
	CODA_ASSERT(r.id != 0);
	PDB_db_delete_xfer(h, r.name);
	free(r.name);
	r.name = strdup(name);
	PDB_writeProfile(h, &r);

	/* Update everything is an owner of */
	if(PDB_ISUSER(id)){
		nextid = pdb_array_head(&(r.groups_or_members), &off);
		while(nextid != 0){
			PDB_readProfile(h, nextid, &p);
			CODA_ASSERT(r.id != 0);
			if(PDB_ISGROUP(p.id)){
				free(p.owner_name);
				p.owner_name = strdup(name);
				PDB_writeProfile(h, &p);
			}
			PDB_freeProfile(&p);
			nextid = pdb_array_next(&(r.groups_or_members), &off);
		}
	}

	PDB_freeProfile(&r);
	PDB_db_close(h);
}


void PDB_createUser(char *name, int32_t *newId)
{
	PDB_HANDLE h;
	int32_t maxId, minGroupId;
	PDB_profile r;
   
	/* sanity check arguments */
	CODA_ASSERT(name && (name[0] != '\0') && newId);
   
	/* MAKE SURE NO USER WITH THAT NAME ALREADY EXISTS */
	CODA_ASSERT(PDB_nameInUse(name) == 0);

	h=PDB_db_open(O_RDWR);
	
	/* add one to the highest user id -- that's our new user's id */
	PDB_db_maxids(h, &maxId, &minGroupId);
	r.id = maxId + 1;
	PDB_db_update_maxids(h, r.id, minGroupId, PDB_MAXID_SET);
   
	/* CREATE A NEW USER WITH SPECIFIED NAME, ALL OTHER FIELDS EMPTY */
   
	/* create the new user's profile */
	r.name = strdup(name);
	r.owner_id = 0;
	r.owner_name = NULL;
	pdb_array_init(&(r.member_of));
	pdb_array_init(&(r.cps));
	pdb_array_add(&(r.cps),r.id);
	pdb_array_init(&(r.groups_or_members));
   
	/* write the new user's information to the databases */
	PDB_writeProfile(h, &r);
	PDB_db_close(h);

	*newId = r.id;
	PDB_freeProfile(&r);
}


void PDB_cloneUser(char *name, int32_t cloneid, int32_t *newId)
{
	PDB_HANDLE h;
	int32_t nextid, maxId, minGroupId;
	PDB_profile r, p;
	pdb_array_off off;

	/* sanity check arguments */
	CODA_ASSERT(name && (name[0] != '\0') && newId && (cloneid != 0));
   
	/* MAKE SURE NO USER WITH THAT NAME ALREADY EXISTS */
	CODA_ASSERT(PDB_nameInUse(name) == 0);

	h=PDB_db_open(O_RDWR);

	/* Read the profile we are cloning */
	PDB_readProfile(h, cloneid, &r);
	CODA_ASSERT(r.id != 0);
	
	/* add one to the highest user id -- that's our new user's id */
	PDB_db_maxids(h, &maxId, &minGroupId);
	r.id = maxId + 1;
	PDB_db_update_maxids(h, r.id, minGroupId, PDB_MAXID_SET);
   
	/* CREATE A NEW USER WITH SPECIFIED NAME, ALL OTHER FIELDS EMPTY */
   
	/* create the new user's profile */
	free(r.name);
	r.name = strdup(name);

	/* MAKE THE NEW USER A MEMBER OF THE SAME GROUPS AS THE OLD USER */
	nextid = pdb_array_head(&(r.member_of), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(r.id != 0);
		pdb_array_add(&(p.groups_or_members), r.id);
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.member_of), &off);
	}
   
	PDB_updateCps(h, &r);

	/* write the new user's information to the databases */
	PDB_writeProfile(h, &r);
	PDB_db_close(h);

	*newId = r.id;
	PDB_freeProfile(&r);
}


void PDB_deleteUser(int32_t id)
{
	PDB_HANDLE h;
	PDB_profile r,p;
	int32_t nextid;
	pdb_array_off off;
	
	h=PDB_db_open(O_RDWR);
	PDB_readProfile(h, id, &r);
	if(r.id == 0){
		PDB_db_close(h);
		return;
	}
	PDB_deleteProfile(h, &r);

	/* Remove from groups */
	nextid = pdb_array_head(&(r.member_of), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(p.id != 0);
		pdb_array_del(&(p.groups_or_members), id);
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.member_of), &off);
	}
	PDB_freeProfile(&r);
	PDB_db_close(h);
}


void PDB_createGroup(char *name, int32_t owner, int32_t *newGroupId)
{
	PDB_HANDLE h;
	int32_t maxId, minGroupId;
	PDB_profile r, p;
   
	/* sanity check arguments */
	CODA_ASSERT(name && (name[0] != '\0') && newGroupId
		    && PDB_ISUSER(owner));
   
	/* MAKE SURE NO GROUP WITH THAT NAME ALREADY EXISTS */
	CODA_ASSERT(PDB_nameInUse(name) == 0);

	h=PDB_db_open(O_RDWR);

	PDB_readProfile(h, owner, &p);
	CODA_ASSERT(p.id != 0);
	
	/* subtract one from the lowest group id -- that's new group's id */
	PDB_db_maxids(h, &maxId, &minGroupId);
	r.id = minGroupId - 1;
	*newGroupId = r.id;
	PDB_db_update_maxids(h, maxId, r.id, PDB_MAXID_SET);
   
   	/* CREATE A NEW GROUP WITH SPECIFIED NAME AND OWNER */
	
	/* create the new group's profile */
	r.name = strdup(name);
	r.owner_id = owner;
	r.owner_name = strdup(p.name);
	pdb_array_init(&(r.member_of));
	pdb_array_init(&(r.cps));
	pdb_array_add(&(r.cps),r.id);
	pdb_array_init(&(r.groups_or_members));
	pdb_array_add(&(r.groups_or_members),owner);
	/* write the new group's information to the databases */
	PDB_writeProfile(h, &r);
   
	/* add the new group's id to owner's groups_or_members list */
	pdb_array_add(&(p.groups_or_members), r.id);
	pdb_array_add(&(p.member_of), r.id);
	pdb_array_add(&(p.cps), r.id);
	PDB_writeProfile(h, &p);

	PDB_freeProfile(&r);
	PDB_freeProfile(&p);
	PDB_db_close(h);
}


void PDB_deleteGroup(int32_t id)
{
	PDB_HANDLE h;
	PDB_profile r,p;
	int32_t nextid;
	pdb_array_off off;
	
	h=PDB_db_open(O_RDWR);
	PDB_readProfile(h, id, &r);
	if(r.id == 0){
		PDB_db_close(h);
		return;
	}
	PDB_deleteProfile(h, &r);

	/* remove from owner's list */
	PDB_readProfile(h, r.owner_id, &p);
	CODA_ASSERT(p.id != 0);
	pdb_array_del(&(p.groups_or_members), id);
	PDB_writeProfile(h, &p);
	PDB_freeProfile(&p);

	/* remove from groups if memberof */
	nextid = pdb_array_head(&(r.member_of), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(p.id == 0);
		pdb_array_del(&(p.groups_or_members), id);
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.member_of), &off);
	}

	/* remove from members */
	nextid = pdb_array_head(&(r.groups_or_members), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(p.id != 0);
		pdb_array_del(&(p.groups_or_members), id);
		if(PDB_ISGROUP(p.id))
			PDB_updateCps(h, &p);
		else
			PDB_updateCpsSelf(h, &p);
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.groups_or_members), &off);
	}

	/* Now fix the owner's cps */
	PDB_readProfile(h, r.owner_id, &p);
	CODA_ASSERT(p.id != 0);
	PDB_updateCps(h, &p);
	PDB_writeProfile(h, &p);
	PDB_freeProfile(&p);

	PDB_freeProfile(&r);
	PDB_db_close(h);
}

void PDB_lookupByName(char *name, int32_t *id)
{
	PDB_HANDLE h;
	PDB_profile r;

	h = PDB_db_open(O_RDONLY);

	PDB_readProfile_byname(h, name, &r);
	*id = r.id;
	PDB_freeProfile(&r);
	
	PDB_db_close(h);
}


void PDB_lookupById(int32_t id, char **name)
{
	PDB_HANDLE h;
	PDB_profile r;

	h = PDB_db_open(O_RDONLY);

	PDB_readProfile(h, id, &r);
	if(r.id != 0)
		*name = strdup(r.name);
	else
		*name = NULL;
	PDB_freeProfile(&r);
	
	PDB_db_close(h);
}


int PDB_nameInUse(char *name)
{
	PDB_HANDLE h;
	void *r;
	PDB_profile p;

	h = PDB_db_open(O_RDONLY);

	r = PDB_db_read(h, 0, name);

	PDB_db_close(h);

	pdb_unpack(&p, r);

	return (p.id != 0);
}


void PDB_changeId(int32_t oldId, int32_t newId)
{
	PDB_HANDLE h;
	PDB_profile r,p;
	void *tmp;
	int32_t nextid;
	pdb_array_off off;

	if (oldId == newId) return;

	h = PDB_db_open(O_RDWR);

	tmp = PDB_db_read(h, newId, NULL);
	CODA_ASSERT(tmp);
	pdb_unpack(&r, tmp);
	CODA_ASSERT(r.id == 0);

	PDB_readProfile(h, oldId, &r);
	CODA_ASSERT(r.id != 0);

	/* Delete the old id */
	PDB_deleteProfile(h, &r);
	
	/* Change the id */
	r.id = newId;
	PDB_updateCpsSelf(h, &r);
	PDB_writeProfile(h, &r);

	if(PDB_ISGROUP(oldId)){
		/* Need to change owner info */
		PDB_readProfile(h, r.owner_id, &p);
		CODA_ASSERT(p.id != 0);
		pdb_array_del(&(p.groups_or_members), oldId);
		pdb_array_add(&(p.groups_or_members), newId);
		PDB_writeProfile(h, &p);
		PDB_db_update_maxids(h, 0, newId, PDB_MAXID_SET);
		PDB_freeProfile(&p);
	}
	else
		PDB_db_update_maxids(h, newId, 0, PDB_MAXID_SET);

	/* update groups is member of */
	nextid = pdb_array_head(&(r.member_of), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(p.id != 0);
		pdb_array_del(&(p.groups_or_members), oldId);
		pdb_array_add(&(p.groups_or_members), newId);
		/* Don't need CPS updates */
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.groups_or_members), &off);
	}

	/* update members */
	nextid = pdb_array_head(&(r.groups_or_members), &off);
	while(nextid != 0){
		PDB_readProfile(h, nextid, &p);
		CODA_ASSERT(p.id != 0);
		pdb_array_del(&(p.member_of), oldId);
		pdb_array_add(&(p.member_of), newId);
		if(PDB_ISGROUP(oldId)){
			if(PDB_ISGROUP(p.id))
				PDB_updateCps(h, &p);
			else
				PDB_updateCpsSelf(h, &p);
		}
		PDB_writeProfile(h, &p);
		PDB_freeProfile(&p);
		nextid = pdb_array_next(&(r.groups_or_members), &off);
	}

	PDB_db_close(h);
	PDB_freeProfile(&r);
}





