#ifndef PDB_PDB_H
#define PDB_PDB_H

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

#include <sys/types.h>
#include "pdbarray.h"


#define PDB_ISUSER(x) ((x) > 0)
#define PDB_ISGROUP(x) ((x) < 0)

#define PDB_MAXID_SET   (0)
#define PDB_MAXID_FORCE (1)


typedef struct PDB_HANDLE_S *PDB_HANDLE; 

typedef struct {
   int32_t id;
   char *name;
   int32_t owner_id;         /* used only for groups */
   char *owner_name;       /* used only for groups */
   pdb_array member_of;
   pdb_array cps;
   pdb_array groups_or_members;

   /*struct acl *pdbacl; */
} PDB_profile;


/* VHL functions */
void PDB_addToGroup(int32_t id, int32_t groupId);
void PDB_removeFromGroup(int32_t id, int32_t groupId);
void PDB_changeName(int32_t id, char *name);
void PDB_createUser(char *name, int32_t *newId);
void PDB_cloneUser(char *name, int32_t cloneid, int32_t *newId);
void PDB_deleteUser(int32_t id);
void PDB_createGroup(char *name, int32_t owner, int32_t *newGroupId);
void PDB_deleteGroup(int32_t groupId);
void PDB_lookupByName(char *name, int32_t *id);
void PDB_lookupById(int32_t id, char **name);
int PDB_nameInUse(char *name);
void PDB_changeId(int32_t oldid, int32_t newid);

/* internal packing functions */
void pdb_pack(PDB_profile *r, void **data);
void pdb_unpack(PDB_profile *r, void *data);

/* core PDB_ profile functions */
void PDB_freeProfile(PDB_profile *r);
void PDB_writeProfile(PDB_HANDLE h, PDB_profile *r);
void PDB_readProfile(PDB_HANDLE h, int32_t id, PDB_profile *r);
void PDB_readProfile_byname(PDB_HANDLE h, char *name, PDB_profile *r);
void PDB_deleteProfile(PDB_HANDLE h, PDB_profile *r);
void PDB_printProfile(FILE *out, PDB_profile *r);
void PDB_updateCpsSelf(PDB_HANDLE h, PDB_profile *r);
void PDB_updateCpsChildren(PDB_HANDLE h, PDB_profile *r);
void PDB_updateCps(PDB_HANDLE h, PDB_profile *r);

/* interface to the bottom end */
PDB_HANDLE PDB_db_open(int mode);
void PDB_db_close(PDB_HANDLE h);
void PDB_db_maxids(PDB_HANDLE h, int32_t *uid, int32_t *gid);
void PDB_db_update_maxids(PDB_HANDLE h, int32_t uid, int32_t gid, int mode);
void PDB_db_write(PDB_HANDLE h, int32_t id, char *name, void *buf);
void *PDB_db_read(PDB_HANDLE h, int32_t id, char *name);
void PDB_db_delete(PDB_HANDLE h, int32_t id, char *name);
void PDB_db_delete_xfer(PDB_HANDLE h, char *name);
int PDB_db_exists(void);
void PDB_db_compact(PDB_HANDLE h);
int PDB_setupdb(void);
int PDB_db_nextkey(PDB_HANDLE h, int *id);


#endif
