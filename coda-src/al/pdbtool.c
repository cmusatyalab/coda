/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>
#include "pdb.h"
#include <parser.h>
#include "coda_string.h"
#include <coda_assert.h>
#include <codaconf.h>
#include <vice_file.h>

static const char *vicedir = NULL;

/* Check if correct number of arguments,Too Few=1,Too Many=2,Just Right=0 */
int check_args_num(int argc,int n)
{
	if(argc<n){
		printf("Too few arguments!\n");
		return 1;
	}
	else if(argc > n){
		printf("Too many arguments!\n");
		return 2;
	}
	else return 0;
}

/* Convert a name or id argument into a numerical id value */
int32_t get_id(char *n)
{
	int32_t id;
	
	/* try to interpret the passed argument as a numeric id */
	id = atoi(n);
	if(id != 0) return id;

	/* Attempt to find the argument as a name */
	PDB_lookupByName(n, &id);

	return id;
}

/* LOOKUP BY ID */
void tool_byNameOrId(int argc,char *argv[])
{
	PDB_profile sample;
	PDB_HANDLE h;
	int32_t arg1;

	if(check_args_num(argc,2)){
		printf("Usage: i idnum\nidnum\t\tnumber of user/group\n");
		return;
	}
	arg1 = get_id(argv[1]);
	if(arg1 == 0){
		printf("'%s' not found.\n", argv[1]);
		return;
	}
	h = PDB_db_open(O_RDONLY);
	PDB_readProfile(h, arg1, &sample);
	PDB_printProfile(stdout, &sample);
	PDB_freeProfile(&sample);
	PDB_db_close(h);
}

/* LIST EVERTHING */
void tool_list(int argc,char *argv[])
{
	int32_t id;
	PDB_profile sample;
	PDB_HANDLE h;
	int rc;

	if(check_args_num(argc,1)){
		printf("Usage: list\n");
		return;
	}

	h = PDB_db_open(O_RDONLY);
	while ( (rc = PDB_db_nextkey(h, &id))) {
		if ( rc == -1 )
			continue; 
		PDB_readProfile(h, id, &sample);
		PDB_printProfile(stdout, &sample);
		PDB_freeProfile(&sample);
	}
	PDB_db_close(h);
}


/* CREATE NEW USER */
void tool_newUser(int argc,char *argv[]){
	int32_t id;
	if(check_args_num(argc,2)){
		printf("Usage: nu name\nname\t\tname of new user\n");
		return;
	}
	PDB_createUser(argv[1], &id);
}


/* CREATE NEW USER WITH ID*/
void tool_newUser_Id(int argc,char *argv[]){
	char *s;
	int32_t id;
	int32_t arg2;
	if(check_args_num(argc,3)){
		printf("Usage: nui name id\nname\t\t"
		       "name of new user\nid\t\tid of new user\n");
		return;
	}
	arg2 = atoi(argv[2]);
	if(!PDB_ISUSER(arg2)){
		printf("Not a valid user-id (it needs to be > 0).\n");
		return;
	}
	PDB_lookupById((int32_t) arg2, &s);
	if(s != NULL){
		printf("ID alread used by \"%s\".\n",s);
		free(s);
		return;
	}
	PDB_createUser(argv[1], &id);
	if (id == 0){
		printf("Failed to creat user.\n");
		return;
	}
	if (id != arg2)
		PDB_changeId(id,arg2);
}


/* CREATE NEW USER */
void tool_changeName(int argc,char *argv[]){
	int32_t arg1;
	if(check_args_num(argc,3)){
		printf("Usage: cn id name\nid\t\t"
		       "id number of user\n\nname\t\tnew name of user\n");
		return;
	}
	arg1 = atoi(argv[1]);
	if(arg1 == 0){
		printf("Give numerical value.\n");
		return;
	}
	PDB_changeName(arg1, argv[2]);
}


/* CREATE NEW GROUP */
void tool_newGroup(int argc,char *argv[]){
	char *s;
	int32_t id;
	int32_t arg2;
	if(check_args_num(argc,3)){
		printf("Usage: ng name owner\nname\t\t"
		       "name of new group\nowner\t\t"
		       "id/name number of group owner\n");
		return;
	}
	arg2 = get_id(argv[2]);
	PDB_lookupById((int32_t) arg2, &s);
	if(!PDB_ISUSER(arg2) || s == NULL){
		printf("Owner must be a valid username/id, %s not found!\n",
		       argv[2]);
		if (s) free(s);
		return;
	}
	free(s);
	PDB_createGroup(argv[1], arg2, &id);
}


void tool_newDefGroup(int argc, char **argv)
{
	int32_t ownerid, id;
	char *colon = NULL;
	if(check_args_num(argc,2)){
		printf("Usage: ng name\nname\t\t"
		       "owner:name - name of new group\n");
		return;
	}
	
	colon = strchr(argv[1], ':');
	if ( !colon ) {
		printf("Name must be of the form: owner:group\n");
		return;
	}

	*colon = '\0';
	PDB_lookupByName(argv[1], &ownerid);
	*colon = ':';

	if ( !ownerid ) {
		printf("Owner must exist!\n");
		return;
	}
		
	PDB_createGroup(argv[1], ownerid, &id);
	printf("Created %s with id %d\n", argv[1], id);

}




/* LOOK UP A USER OR GROUP NAME */
void tool_lookup(int argc,char *argv[]){
	int32_t id;
	if(check_args_num(argc,2)){
		printf("Usage: l id\nid\t\tid number of user/group\n");
		return;
	}
	PDB_lookupByName(argv[1], &id);
	printf("%s's id is %d\n",argv[1],id);
}

/* CLONE A USER */
void tool_clone(int argc,char *argv[]){
	int32_t id;
	long arg2;
	if(check_args_num(argc,3)){
		printf("Usage: cu name id\nname\t\t"
		       "name of new user\nid\t\tid number of user to clone\n");
		return;
	}
	arg2 = atol(argv[2]);
	if(arg2 == 0){
		printf("Give numerical value.\n");
		return;
	}
	PDB_cloneUser(argv[1], arg2, &id);
}

/* ADD SOMEONE (USER OR GROUP) TO A GROUP */
void tool_addtoGroup(int argc,char *argv[]){
	char *s;
	long arg1, arg2;
	if(check_args_num(argc,3)){
		printf("Usage: ag group user\ngroup\t\t"
		       "id or name of group to add to\nuser\t\t"
		       "id or name of user to add\n");
		return;
	}
	arg1 = get_id(argv[1]);
	PDB_lookupById((int32_t) arg1, &s);
	if(!PDB_ISGROUP(arg1) || s == NULL){
		printf("No group %s found!\n", argv[1]);
		if (s) free(s);
		return;
	}
	free(s);
	arg2 = get_id(argv[2]);
	PDB_lookupById((int32_t) arg2, &s);
	if(s == NULL){
		printf("No user or group %s found!\n", argv[2]);
		if (s) free(s);
		return;
	}
	free(s);
	PDB_addToGroup(arg2,arg1);
}

/* REMOVE SOMEONE (USER OR GROUP) TO A GROUP */
void tool_removefromGroup(int argc,char *argv[]){
	int32_t arg1, arg2;
	if(check_args_num(argc,3)){
		printf("Usage: rg group user\ngroup\t\t"
		       "id or name of group to remove from\nuser\t\t"
		       "id or name of user to remove\n");
		return;
	}
	arg1 = get_id(argv[1]);
	if(!PDB_ISGROUP(arg1)) {
		printf("No group %s found!\n", argv[1]);
		return;
	}
	arg2 = get_id(argv[2]);
	if(arg2 == 0) {
		printf("No user or group %s found!\n", argv[2]);
		return;
	}
	PDB_removeFromGroup(arg2, arg1);
}

/* DELETE USER OR GROUP */
void tool_delete(int argc,char *argv[]){
	char *s;
	long arg1;
	if(check_args_num(argc,2)){
		printf("Usage: d id/name\n"
		       "id/name\t\tid or name of user/group\n");
		return;
	}
	arg1 = get_id(argv[1]);
	PDB_lookupById((int32_t) arg1, &s);
	if(s == NULL){
		printf("%s not found!\n",argv[1]);
		return;
	}
	free(s);
	if(PDB_ISGROUP(arg1))
		PDB_deleteGroup(arg1);
	else
		PDB_deleteUser(arg1);
}


/* UPDATE USER OR GROUP */
void tool_update(int argc,char *argv[]){
	long arg1;
	PDB_HANDLE h;
	PDB_profile p;
	if(check_args_num(argc,2)){
		printf("Usage: u id/name\n"
		       "id/name\t\tid or name of user/group\n");
		return;
	}
	h = PDB_db_open(O_RDWR);
	arg1 = get_id(argv[1]);
	if(arg1 == 0){
		printf("%s not found.\n", argv[1]);
		return;
	}
	PDB_readProfile(h, arg1, &p);
	if(p.id != 0) {
		PDB_updateCps(h, &p);
		PDB_freeProfile(&p);
	}

	PDB_db_close(h);
}

/* COMPACT DATABASES */
void tool_compact(int argc,char *argv[])
{
	PDB_HANDLE h;

	if(check_args_num(argc,1)){
		printf("Usage: cm\n");
		return;
	}
	/* fix database consistency bugs */
	PDB_bugfixes();

	h = PDB_db_open(O_RDWR);
	PDB_db_compact(h);
	PDB_db_close(h);
}

/* SHOW MAXIDS */
void tool_get_maxids(int argc,char *argv[]){
      PDB_HANDLE h;
      int maxuid, maxgid;
      if(check_args_num(argc,1)){
              printf("Usage: get_maxids\n");
      }
      h = PDB_db_open(O_RDONLY);
      PDB_db_maxids(h, &maxuid, &maxgid);
      PDB_db_close(h);
      printf("maxuid %d maxgid %d\n", maxuid, maxgid);
}

/* SET MAXIDS */
void tool_maxids(int argc,char *argv[])
{
	PDB_HANDLE h;
	long arg1, arg2;
	char *end;

	if(check_args_num(argc,3)){
		printf("Usage: maxids usermax groupmin\n");
		return;
	}
	arg1 = strtol(argv[1], &end, 10);
	if(argv[1] == end || arg1 < 0){
		printf("User max should be a positive numerical value.\n");
		return;
	}
	arg2 = strtol(argv[2], &end, 10);
	if(argv[2] == end || arg2 > 0){
		printf("Group min should be a negative numerical value.\n");
		return;
	}
	h = PDB_db_open(O_RDWR);
	PDB_db_update_maxids(h, arg1, arg2, PDB_MAXID_FORCE);
	PDB_db_close(h);
}

/* CHANGE ID */
void tool_changeId(int argc,char *argv[])
{
	int32_t id;
	long arg2;
	if(check_args_num(argc,3)) {
		printf("Usage %s user/group newid\n", argv[0]);
		return;
	}
	id = get_id(argv[1]);
	if (id == 0) {
		printf("%s not found.\n", argv[1]);
		return;
	}
	arg2 = atol(argv[2]);
	if(arg2 == 0){
		printf("Give numerical value for newid.\n");
		return;
	}
	PDB_changeId(id,arg2);
}

/* dump/restore database contents */
void tool_ldif_export(int argc, char *argv[])
{
    int32_t id, i;
    PDB_profile rec;
    PDB_HANDLE h;
    FILE *ldiffile;
    char *s, *basedn;
    int rc, pass = 0;

    if (check_args_num(argc, 3)) {
	printf("Usage: ldif_export <ldif-file> <basedn>\n");
	return;
    }


    ldiffile  = fopen(argv[1], "w");
    basedn = argv[2];

again:

    h = PDB_db_open(O_RDONLY);
    while ((rc = PDB_db_nextkey(h, &id))) {
	if (rc == -1) continue;

	PDB_readProfile(h, id, &rec);
	{
	    if (rec.id == 0) continue;

	    if (PDB_ISUSER(rec.id)) {
		/* skip users during the second pass */
		if (pass == 1) continue;

		/* users are dumped as:
		 *
		 * dn: cn=<name>,$basedn
		 * objectClass: top
		 * objectClass: person
		 * objectClass: organizationalPerson
		 * objectClass: inetOrgPerson
		 * objectClass: codaAccount
		 * cn: <name>
		 * uid: <username>
		 * uidNumber: <userid>
		 *
		 * And we 'invent' 2 fields that are required by posixAccount,
		 * gidNumber: 65535
		 * homeDirectory: /coda/<realm>/usr/<username>
		 */
		fprintf(ldiffile, "dn: cn=%s,%s\n"
			"objectClass: top\n"
                        "objectClass: person\n"
			"objectClass: organizationalPerson\n"
			"objectClass: inetOrgPerson\n"
			"objectClass: posixAccount\n"
			"cn: %s\nuid: %s\nuidNumber: %d\n"
			"gidNumber: 65535\n"
			"homeDirectory: /coda/myrealm/usr/%s\n"
			"#mail: %s@mydomain\n\n",
			rec.name, basedn, rec.name, rec.name, rec.id,
			rec.name, rec.name);
	    } else {
		/* skip groups during the first pass */
		if (pass == 0) continue;

		/* groups and group members are dumped as follows:
		 *
		 * dn: cn=<groupname>,$basedn
		 * objectClass: top
		 * objectClass: groupOfNames
		 * objectClass: posixGroup
		 * cn: <groupname>
		 * gidNumber: -<groupid>
		 * owner: <ownerdn>
		 * member: <member1dn>
		 * member: <member2dn>
		 * ...
		 */

		PDB_lookupById(rec.owner_id, &s);
		CODA_ASSERT(s != NULL);
		fprintf(ldiffile,
			"dn: cn=%s,%s\n"
			"objectClass: top\n"
			"objectClass: groupOfNames\n"
			"objectClass: posixGroup\n"
			"#description:\n"
			"cn: %s\ngidNumber: %d\nowner: %s,%s\n",
			rec.name, basedn, rec.name, -rec.id, s, basedn);
		free(s);

		for (i = 0; i < rec.groups_or_members.size; i++) {
		    PDB_lookupById(rec.groups_or_members.data[i], &s);
		    if (s == NULL) continue;

		    fprintf(ldiffile, "member: cn=%s,%s\n", s, basedn);
		    free(s);
		}
		fprintf(ldiffile, "\n");
	    }
	}
	PDB_freeProfile(&rec);
    }
    PDB_db_close(h);

    /* we make second pass to dump the groups after the users */
    if (pass == 0) {
	    pass = 1;
	    goto again;
    }

    fclose(ldiffile);
}

/* dump/restore database contents */
void tool_export(int argc, char *argv[])
{
    int32_t id, i;
    PDB_profile rec;
    PDB_HANDLE h;
    FILE *userfile, *groupfile;
    char *s;
    int rc;

    if (check_args_num(argc, 3)) {
	printf("Usage: export <userfile> <groupfile>\n");
	return;
    }

    userfile  = fopen(argv[1], "w");
    groupfile = fopen(argv[2], "w");

    h = PDB_db_open(O_RDONLY);
    while ((rc = PDB_db_nextkey(h, &id))) {
	if (rc == -1) continue;

	PDB_readProfile(h, id, &rec);
	{
	    if (rec.id == 0) continue;

	    if (PDB_ISUSER(rec.id)) {
		/* users are dumped in an /etc/passwd like format
		 * "<username>:x:<userid>:500::/:" */
		fprintf(userfile, "%s:*:%d:500::/:\n", rec.name, rec.id);
	    } else {
		/* groups and group members are dumped in an /etc/group like
		 * format "<groupname>:x:<groupid>:<owner>[,<members>]*" */

		/* escape the :'s in the group names */
		s = rec.name; while ((s = strchr(s, ':')) != NULL) *s = '%';

		PDB_lookupById(rec.owner_id, &s);
		fprintf(groupfile, "%s:*:%d:%s", rec.name, rec.id, s);
		free(s);

		for (i = 0; i < rec.groups_or_members.size; i++) {
		    if (rec.groups_or_members.data[i] == rec.owner_id)
			continue;

		    PDB_lookupById(rec.groups_or_members.data[i], &s);
		    if (s == NULL) continue;

		    fprintf(groupfile, ",%s", s);
		    free(s);
		}
		fprintf(groupfile, "\n");
	    }
	}
	PDB_freeProfile(&rec);
    }
    PDB_db_close(h);

    fclose(userfile);
    fclose(groupfile);
}

void tool_import(int argc, char *argv[])
{
    FILE *userfile, *groupfile;
    char user[64], group[64], owner[64], member[64], *s;
    int32_t user_id, group_id, owner_id, member_id, create_id;
    int rc;

    if (check_args_num(argc, 3)) {
	fprintf(stderr, "Usage: import <userfile> <groupfile>\n");
	return;
    }

    userfile = fopen(argv[1], "r");
    if (!userfile) {
	fprintf(stderr, "Can't open userfile '%s'\n", argv[1]);
	return;
    }

    groupfile = fopen(argv[2], "r");
    if (!groupfile) {
	fprintf(stderr, "Can't open groupfile '%s'\n", argv[2]);
	return;
    }

    /* recreate all users */
    while(1) {
	rc = fscanf(userfile, "%[^:]:%*[^:]:%d:%*[^\n]\n", user, &user_id);
	if (rc != 2) break;

	if (user_id == 0) {
	    printf("Userid 0 must be avoided, skipping entry for %s\n", user);
	    continue;
	}
	if (user_id < 0) {
	    printf("Skipping user %s with a negative id %d\n", user, user_id);
	    continue;
	}

	/* create user */
	PDB_lookupById(user_id, &s);
	if (s) {
	    printf("Duplicate user for id %d, found both %s and %s\n",
		   user_id, s, user);
	    free(s);
	    continue;
	}

	PDB_createUser(user, &create_id);
	PDB_changeId(create_id, user_id);
	printf("Created user %s, id %d\n", user, user_id);
    }
    fclose(userfile);
    
    /* recreate groups */
    while (1) {
	int c;
	rc = fscanf(groupfile, "%[^:]:%*[^:]:%d:%[^,\n]",
		    group, &group_id, owner);
	if (rc != 3) break;

	/* skip to the next newline */
	do {
	  c = fgetc(groupfile);
	} while (c != EOF && c != (int)'\n');

	if (group_id == 0) {
	    printf("Groupid 0 must be avoided, skipping entry for %s\n",
		   group);
	    continue;
	}

	/* restore the :'s in the group name */
	s = group; while ((s = strchr(s, '%')) != NULL) *s = ':';

	/* negate positive group ids, Coda groups are negative numbers */
	if (group_id > 0) {
	    group_id = -group_id;
	    /* assuming this is the /etc/group file, force owner to System */
	    strcpy(owner, "System");
	}

	/* create group */
	PDB_lookupByName(owner, &owner_id);
	if (owner_id == 0) {
	    printf("Group %s's owner %s cannot be found\n", group, owner);
	    continue;
	}
	if (!PDB_ISUSER(owner_id)) {
	    printf("Group %s's owner %s is a group but should be a user\n",
		   group, owner);
	    continue;
	}
	PDB_lookupById(group_id, &s);
	if (s) {
	    printf("Duplicate group for id %d, found both %s and %s\n",
		   group_id, s, group);
	    free(s);
	    continue;
	}
	PDB_createGroup(group, owner_id, &create_id);
	PDB_changeId(create_id, group_id);
	printf("Created group %s, id %d, owner %s\n", group, group_id, owner);
    }   

    /* Add group members. Groups can be members of another group, so that is
     * why we needed to create all the groups first */
    rewind(groupfile);
    while (1) {
	rc = fscanf(groupfile, "%[^:]:%*[^:]:%d:", group, &group_id);
	if (rc != 2) break;

	if (group_id == 0) continue;

	/* restore the :'s in the group name */
	s = group; while ((s = strchr(s, '%')) != NULL) *s = ':';

	if (group_id > 0) {
	    group_id = -group_id;
	} else {
	    /* skip the owner when the group_id is negative */
	    fscanf(groupfile, "%[^,\n]%*[,]", owner);
	}

	/* add group members */
	printf("Adding members to %s\n\t", group);
	while (fscanf(groupfile, "%[^,\n]%*[,]", member) == 1) {
	    /* restore the :'s in the name */
	    s = member; while ((s = strchr(s, '%')) != NULL) *s = ':';

	    PDB_lookupByName(member, &member_id);
	    if (member_id == 0) {
		printf("\nGroup %s's member %s cannot be found\n\t",
		       group, member);
		continue;
	    }
	    PDB_addToGroup(member_id, group_id);
	    printf(" %s", member);
	}
	fgetc(groupfile); /* eat the '\n' */
	printf("\n");
    }   
    fclose(groupfile);
}

void tool_source(int argc, char *argv[])
{
	char line[1024];
	char *nl;

	FILE *file = fopen(argv[1], "r");
	if ( !file ) {
	    perror("");
	    return;
	}
	while ( fgets(line, 1024, file) ) {
	    if ( (nl = strchr(line, '\n')) )
		*nl = '\0';
	    execute_line(line);
	}
}


/* HELP */
void tool_help(int argc, char *argv[])
{
	if (argc > 1) {
	    Parser_help(argc, argv);
	    return;
	}

        printf("i <id/name>\t\t\tget info from database about ID/name\n");
	printf("nu <username>\t\t\tcreate a new user\n");
	printf("nui <username> <userid>\t\tcreate a new user with id\n");
	printf("ng <groupname> <ownerid/name>\tcreate a new group\n");
	printf("l <name>\t\t\tlook up an ID by name\n");
	printf("list\t\t\t\tlist all entries\n");
	printf("cu <newusername> <userid>\tclone a user\n");
	printf("ag <groupid/name> <id/name>\tadd a group or user to a group\n");
	printf("rg <groupid/name> <id/name>\tremove a group or user from a group\n");
	printf("d <id/name>\t\t\tdelete a user or a group\n");
	printf("cm\t\t\t\tcompact the database (RARE)\n");
	printf("ci <name> <newid>\t\tchange the Id of a user or group\n");
	printf("cn <id> <newname>\t\tchange the Name of a user or group\n");
	printf("u <id/name>\t\t\tupdate an id/name\n");
	printf("ids\t\t\t\tget the database maxids\n");
	printf("maxids <userid> <groupid>\tset the database maxids\n");
	printf("ldif_export <ldiffile> <basedn>\tdump the contents of the pdb database in LDIF format\n");
	printf("export <userfile> <groupfile>\tdump the contents of the pdb database\n");
	printf("import <userfile> <groupfile>\tread a dumped pdb database\n");
	printf("source <file>\t\t\tread commands from file\n");
	printf("exit\t\t\t\texit the pdbtool\n");
}

command_t pdbcmds[] =
{
	{"i", tool_byNameOrId, NULL, "get info from the database (by name or id)"},
	/* 'n' only for compatibility with pre-5.3 pdbtool */
	{"n", tool_byNameOrId, NULL, "get info from the database (by name or id)"},
	{"nu", tool_newUser, NULL, "create a new user"},
	{"nui", tool_newUser_Id, NULL, "create a new user with id"},
	{"ng", tool_newGroup, NULL, "create a new group"},
	{"newgroup", tool_newDefGroup, NULL, "create a new group"},
	{"l", tool_lookup, NULL, "look up an ID by name"},
	{"list", tool_list, NULL, "list all entries"},
	{"cu", tool_clone, NULL, "clone a user"},
	{"ag", tool_addtoGroup, NULL, "add a group or user to a group"},
	{"rg", tool_removefromGroup, NULL, "remove a group or user from a group"},
	{"d", tool_delete, NULL, "delete a user or a group"},
	{"cm", tool_compact, NULL, "compact the database (RARE)"},
	{"ci", tool_changeId, NULL, "change the Id of a user or group"},
	{"cn", tool_changeName, NULL, "change the Name of a user"},
	{"u", tool_update, NULL, "update an id"},
	{"ids", tool_get_maxids, NULL, "get the database maxids"},
	{"maxids", tool_maxids, NULL, "set the database maxids"},
	{"ldif_export", tool_ldif_export, NULL, "dump the contents of the database in LDIF format"},
	{"export", tool_export, NULL, "dump the contents of the database"},
	{"import", tool_import, NULL, "load the contents of the database"},
	{"source", tool_source, NULL, "read commands from file"},
	{"help", tool_help, NULL, "print help on commands"},
	{"quit", Parser_exit, NULL, "get me out of here"},
	{"exit", Parser_exit, NULL, "get me out of here"},
	{ NULL, NULL, NULL, NULL }
};


void ReadConfigFile(void)
{
    /* Load server.conf configuration file to get vice dir. */
    codaconf_init("server.conf");

    CODACONF_STR(vicedir, "vicedir", "/vice");

    vice_dir_init(vicedir);
}

int main(int argc, char **argv)
{
	int i;
	coda_assert_action = CODA_ASSERT_EXIT;

	ReadConfigFile();

	PDB_setupdb();
	Parser_init("pdbtool> ", pdbcmds);
	if ( argc == 1 )
		Parser_commands();
	else {
		char line[1024];
		strcpy(line, argv[1]);
		for (i = 2; i < argc; i++) {
		    strcat(line, " ");
		    strcat(line, argv[i]);
		}
		execute_line(line);
	}
	PDB_db_release();
	return 0;
}
