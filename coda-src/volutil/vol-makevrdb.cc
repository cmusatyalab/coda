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
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <voltypes.h>
#include <vrdb.h>
#include <vice.h>
#include <rpc2/errors.h>
#include <cvnode.h>
#include <volume.h>


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
    CODA_ASSERT(vre);
    while (fgets(line, sizeof(line), vrlist) != NULL) {
	lineno++;
	int servercount;
	if (sscanf(line, "%s %lx %d %lx %lx %lx %lx %lx %lx %lx %lx %lx",
		   vre->key, &vre->volnum, &servercount,
		   &vre->ServerVolnum[0], &vre->ServerVolnum[1],
		   &vre->ServerVolnum[2], &vre->ServerVolnum[3],
		   &vre->ServerVolnum[4], &vre->ServerVolnum[5],
		   &vre->ServerVolnum[6], &vre->ServerVolnum[7],
		   &vre->addr) != 12 || strlen(vre->key) >= V_MAXVOLNAMELEN) {
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
