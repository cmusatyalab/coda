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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#ifndef _VLDB_H_
#define _VLDB_H_ 1

#include <stdint.h>

/* Note: this structure happens to be 64 bytes long which isn't real important.
 * But it seemed like a nice number, and the code currently does use a shift. */

#define MAXVOLTYPES	5	/* Maximum number of different types of
				   volumes, each of which can be associated
				   with the current volume */
/* (defined in vcrcommon.rpc2) VSG_MEMBERS 8 -* Maximum number of servers
				   that can be recorded in the vldb as serving
				   a single volume */
struct vldb {
    char key[33];		/* Name or volume id, in ascii, null terminated
				 */
    byte hashNext;		/* Number of entries between here and next hash
				   entry for same hash. 0 is the last */
    byte volumeType;		/* Volume type, as defined in vice.h  (RWVOL,
				   ROVOL, BACKVOL) */
    byte nServers;		/* Number of servers that have this volume */
    uint32_t volumeId[MAXVOLTYPES]; /* *NETORDER* Corresponding volume of
					    each type + 2 extra unused */
    byte serverNumber[VSG_MEMBERS];/* Server number for each server claiming
				      to know about this volume */
};

#define LOG_VLDBSIZE	 6	/* Assume the structure is 64 bytes */

/* Header takes up entry #0.  0 is not a legit hash code */

struct vldbHeader {
    uint32_t magic;			/* *NETORDER* Magic number */
    uint32_t hashSize;		/* *NETORDER* Size to use for hash calculation (see HashString) */
};

#define VLDB_MAGIC 0xABCD4321


#define N_SERVERIDS 256		/* Not easy to change--maximum number of servers */

#include <vice_file.h>

#define VLDB_PATH 	vice_sharedfile("db/VLDB")
#define VLDB_TEMP 	vice_sharedfile("db/VLDB.new")
#define BACKUPLIST_PATH vice_file("vol/BackupList")

extern struct vldb *VLDBLookup(char *key);
extern int VLDBPrint();

#endif /* _VLDB_H_ */
