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

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#ifdef  __BSD44__
#include <fstab.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lock.h>
#include <lwp/lwp.h>
#include <util.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include "cvnode.h"
#include "volume.h"
#include "partition.h"
#include "vutil.h"
#include "vldb.h"


static int VLDB_fd = -1;
static int VLDB_size = 0;

struct vldb *VLDBLookup(char *key);

int VCheckVLDB() 
{
    struct vldbHeader header;

    VLog(19, "Checking VLDB...");
    close(VLDB_fd);
    VLDB_fd = open(VLDB_PATH, O_RDONLY, 0);
    if (VLDB_fd == -1) {
	VLog(0, "VCheckVLDB:  could not open VLDB");
	return (-1);
    }
    if (read(VLDB_fd, (char *)&header, sizeof(header)) != sizeof(header) || ntohl(header.magic) != VLDB_MAGIC) {
	VLog(0, "VCheckVLDB:  bad VLDB!");
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
    static struct vldb VLDB_records[8];
    int rc;

    if (VLDB_size == 0) {
	VLog(0, "VLDBLookup: VLDB_size unset. Calling VCheckVLDB()");
	rc = VCheckVLDB();
	if ( rc != 0 ) {
		VLog(0, "VLDBLookup: No or bad vldb.");
		return 0;
	}
    }
    int index = HashString(key, VLDB_size);
    VLog(9, "VLDBLookup: index = %d, VLDB_size = %d, LOG_VLDBSIZE = %d",
		    index, VLDB_size, LOG_VLDBSIZE);
    for (;;) {
        int n;
	int i=0, nRecords=0;
	if (lseek(VLDB_fd, index << LOG_VLDBSIZE, L_SET) == -1) {
	    VLog(9, "VLDBLookup: lseek failed for VLDB");
	    return 0;
	}
	n = read(VLDB_fd, (char *)VLDB_records, sizeof(VLDB_records));
	if (n <= 0) {
	    VLog(0, "VLDBLookup: read failed for VLDB");
	    return 0;
	}
	VLog(29, "VLDBLookup: read succeeded, n = %d", n>>LOG_VLDBSIZE);
	nRecords = (n>>LOG_VLDBSIZE);    
	for (i = 0; i<nRecords; ) {
	    struct vldb *vldp = &VLDB_records[i];
	    if (vldp->key[0] == key[0] && strcmp(vldp->key, key) == 0) {
		VLog(39, "VLDBLookup: found VLDB record, VID = %u type = %x, servers = %d, server0 = %d, server1 = %x, server2 = %x",
		    ntohl(vldp->volumeId[vldp->volumeType]), vldp->volumeType,
		    vldp->nServers, vldp->serverNumber[0],
		    vldp->serverNumber[1], vldp->serverNumber[2]);
		return vldp;
	    }
	    else {  /* key mismatch; generate log message */
		VLog(9, "VLDBLookup: vldp->key = %s, key = %s", 
			vldp->key, key);
	    }
	    if (!vldp->hashNext) {
		VLog(0, "VLDB_Lookup: no more records in VLDB");
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
	VLog(129, "VLDBPrint:  lseek failed for VLDB!");
	close(VLDB_fd);
	VLDB_fd = -1;
	return (-1);
    }

    VLog(100, "VLDBPrint: ");
    for (;;) {
        int n;
	int i=0, nRecords=0;
	n = read(VLDB_fd, (char *)VLDB_records, sizeof(VLDB_records));
	if (n < 0) 
	    VLog(129, "VLDBPrint: read failed for VLDB");
	if (n <= 0)
	    return 0;

	VLog(129, "VLDBPrint: read succeeded, n = %d", n>>LOG_VLDBSIZE);
	nRecords = (n>>LOG_VLDBSIZE);    
	for (i = 0; i<nRecords; i++) {
	    struct vldb *vldp = &VLDB_records[i];
	    if (ntohl(vldp->volumeId[vldp->volumeType]) != 0)
		VLog(100, "VID = %x type = %x, servers = %d, server0 = %x, server1 = %x, server2 = %x, key= %s",
		    ntohl(vldp->volumeId[vldp->volumeType]), vldp->volumeType,
		    vldp->nServers, vldp->serverNumber[0],
		    vldp->serverNumber[1], vldp->serverNumber[2], vldp->key);
	  }	
      }
}    

