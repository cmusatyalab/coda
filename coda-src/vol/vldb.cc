#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vol/vldb.cc,v 4.3 1998/01/10 18:39:43 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <ctype.h>
#include <sys/param.h>
#ifdef __DELETEME__
#include <sys/fs.h>
#endif __DELETEME__
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#ifdef  __BSD44__
#include <sys/dir.h>
#include <fstab.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lock.h>
#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vice.h>
#include <srv.h>
#include "cvnode.h"
#include "volume.h"
#include "partition.h"
#include "vutil.h"
#include "vldb.h"


PRIVATE int VLDB_fd = -1;
PRIVATE int VLDB_size = 0;

struct vldb *VLDBLookup(char *key);

int VCheckVLDB() 
{
    struct vldbHeader header;

    LogMsg(19, VolDebugLevel, stdout, "Checking VLDB...");
    close(VLDB_fd);
    VLDB_fd = open(VLDB_PATH, O_RDONLY, 0);
    if (VLDB_fd == -1) {
	LogMsg(0, VolDebugLevel, stdout, "VCheckVLDB:  could not open VLDB");
	return (-1);
    }
    if (read(VLDB_fd, (char *)&header, sizeof(header)) != sizeof(header) || ntohl(header.magic) != VLDB_MAGIC) {
	LogMsg(0, VolDebugLevel, stdout, "VCheckVLDB:  bad VLDB!");
	close(VLDB_fd);
	VLDB_fd = -1;
	return (-1);
    }
    VLDB_size = ntohl(header.hashSize);
    return (0);
}

/* Lookup vldb record from a file */
struct vldb *VLDBLookup(char *key)
{
    private struct vldb VLDB_records[8];
    int rc;

    if (VLDB_size == 0) {
	LogMsg(0, VolDebugLevel, stdout, "VLDBLookup: VLDB_size unset. Calling VCheckVLDB()");
	rc = VCheckVLDB();
	if ( rc != 0 ) {
		LogMsg(0, VolDebugLevel, stdout, "VLDBLookup: No or bad vldb.");
		return 0;
	}
    }
    int index = HashString(key, VLDB_size);
    LogMsg(9, VolDebugLevel, stdout, "VLDBLookup: index = %d, VLDB_size = %d, LOG_VLDBSIZE = %d",
		    index, VLDB_size, LOG_VLDBSIZE);
    for (;;) {
        int n;
	register int i=0, nRecords=0;
	if (lseek(VLDB_fd, index << LOG_VLDBSIZE, L_SET) == -1) {
	    LogMsg(9, VolDebugLevel, stdout, "VLDBLookup: lseek failed for VLDB");
	    return 0;
	}
	n = read(VLDB_fd, (char *)VLDB_records, sizeof(VLDB_records));
	if (n <= 0) {
	    LogMsg(0, VolDebugLevel, stdout, "VLDBLookup: read failed for VLDB");
	    return 0;
	}
	LogMsg(29, VolDebugLevel, stdout, "VLDBLookup: read succeeded, n = %d", n>>LOG_VLDBSIZE);
	nRecords = (n>>LOG_VLDBSIZE);    
	for (i = 0; i<nRecords; ) {
	    register struct vldb *vldp = &VLDB_records[i];
	    if (vldp->key[0] == key[0] && strcmp(vldp->key, key) == 0) {
		LogMsg(39, VolDebugLevel, stdout, "VLDBLookup: found VLDB record, VID = %u type = %x, servers = %d, server0 = %d, server1 = %x, server2 = %x",
		    ntohl(vldp->volumeId[vldp->volumeType]), vldp->volumeType,
		    vldp->nServers, vldp->serverNumber[0],
		    vldp->serverNumber[1], vldp->serverNumber[2]);
		return vldp;
	    }
	    else {  /* key mismatch; generate log message */
		LogMsg(9, VolDebugLevel, stdout, "VLDBLookup: vldp->key = %s, key = %s", 
			vldp->key, key);
	    }
	    if (!vldp->hashNext) {
		LogMsg(0, VolDebugLevel, stdout, "VLDB_Lookup: no more records in VLDB");
		return 0;
	    }
	    i += vldp->hashNext;
	}
	index += i;
    }
}    


int VLDBPrint()
{
    struct vldb VLDB_records[8];

    if (VLDB_fd == -1) 
      if (VCheckVLDB() == -1)    /* Close and reopen the db file and reset */
	  return(-1);            /* the fd to after the header */

    if (lseek(VLDB_fd, 0, L_SET) == -1) {
	LogMsg(129, VolDebugLevel, stdout, "VLDBPrint:  lseek failed for VLDB!");
	close(VLDB_fd);
	VLDB_fd = -1;
	return (-1);
    }

    LogMsg(100, VolDebugLevel, stdout, "VLDBPrint: ");
    for (;;) {
        int n;
	register int i=0, nRecords=0;
	n = read(VLDB_fd, (char *)VLDB_records, sizeof(VLDB_records));
	if (n < 0) 
	    LogMsg(129, VolDebugLevel, stdout, "VLDBPrint: read failed for VLDB");
	if (n <= 0)
	    return 0;

	LogMsg(129, VolDebugLevel, stdout, "VLDBPrint: read succeeded, n = %d", n>>LOG_VLDBSIZE);
	nRecords = (n>>LOG_VLDBSIZE);    
	for (i = 0; i<nRecords; i++) {
	    register struct vldb *vldp = &VLDB_records[i];
	    if (ntohl(vldp->volumeId[vldp->volumeType]) != 0)
		LogMsg(100, VolDebugLevel, stdout, "VID = %x type = %x, servers = %d, server0 = %x, server1 = %x, server2 = %x",
		    ntohl(vldp->volumeId[vldp->volumeType]), vldp->volumeType,
		    vldp->nServers, vldp->serverNumber[0],
		    vldp->serverNumber[1], vldp->serverNumber[2]);
	  }	
      }
}    

