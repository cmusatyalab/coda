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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-makevrdb.cc,v 4.2 1997/02/26 16:04:10 rvb Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <nfs.h>
#include <vrdb.h>
#include <vice.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>

#define VRDB_PATH "/vice/db/VRDB"
#define VRDB_TEMP "/vice/db/VRDB.new"



/*
  BEGIN_HTML
  <a name="S_VolMakeVRDB"><strong>Rebuild the VRDB from the file listed as parameter</strong></a> 
  END_HTML
*/
// This routine parses a text file, VRList, and translates it into a binary format. 
// The format of the text file is:
//      <Group-volname, Group-volid, VSGsize, RWVol0, ... , RWVol7, VSGAddr>

long S_VolMakeVRDB(RPC2_Handle rpcid, RPC2_String formal_infile) {
    /* To keep C++ 2.0 happy */
    char *infile = (char *)formal_infile;
    FILE *vrlist = NULL;
    int err = 0;
    vrent *vre = NULL;
    char line[500];
    int lineno = 0;
    int fd = 0;

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolMakeVRDB; infile %s", infile);
    
    fd = open(VRDB_TEMP, O_TRUNC|O_WRONLY|O_CREAT, 0644);
    if (fd == -1) {
	LogMsg(0, VolDebugLevel, stdout,
	       "S_VolMakeVRDB:  Unable to create %s; aborted", VRDB_TEMP);
	err = VFAIL;
	goto Exit;
    }
    
    vrlist = fopen(infile, "r");
    if (vrlist == NULL) {
	LogMsg(0, VolDebugLevel, stdout, 
	       "S_VolMakeVRDB: unable to open file %s", infile);
	err = VFAIL;
	goto Exit;
    }
    vre = new vrent();
    assert(vre);
    while (fgets(line, sizeof(line), vrlist) != NULL) {
	lineno++;
	int servercount;
	if (sscanf(line, "%32s %x %d %x %x %x %x %x %x %x %x %x",
		   vre->key, &vre->volnum, &servercount,
		   &vre->ServerVolnum[0], &vre->ServerVolnum[1],
		   &vre->ServerVolnum[2], &vre->ServerVolnum[3],
		   &vre->ServerVolnum[4], &vre->ServerVolnum[5],
		   &vre->ServerVolnum[6], &vre->ServerVolnum[7],
		   &vre->addr) != 12) {
	    LogMsg(0, VolDebugLevel, stdout, "Bad input line(%d): %s", lineno, line);
	    LogMsg(0, VolDebugLevel, stdout, "makevrdb aborted");
	    err = VFAIL;
	    goto Exit;
	}
	vre->nServers = servercount;
	vre->Canonicalize();
	vre->hton();
	
	if (write(fd, vre, sizeof(struct vrent)) != sizeof(struct vrent)) {
	    LogMsg(0, VolDebugLevel, stdout, "write error on input line(%d): %s", lineno, line);
	    LogMsg(0, VolDebugLevel, stdout, "makevrdb aborted");
	    err = VFAIL;
	    goto Exit;
	}
    }

    /* Make temporary VRDB permanent. */
    if (rename(VRDB_TEMP, VRDB_PATH) == -1) {
	LogMsg(0, VolDebugLevel, stdout, 
	       "Unable to rename %s to %s; new vrdb not created",
	       VRDB_TEMP, VRDB_PATH);
	err = 1;
    }
    else
	LogMsg(0, VolDebugLevel, stdout, "VRDB created, %d entries", lineno);
    
    /* Tell fileserver to read in new database. */
    CheckVRDB();
  Exit:
    if (vrlist) fclose(vrlist);
    if (fd > 0) close(fd);
    if (vre) delete vre;
    return(err ? VFAIL : 0);
}
