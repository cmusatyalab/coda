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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/update/updatesrv.cc,v 4.14 1998/09/29 16:38:10 braam Exp $";
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



/************************************************************************/
/*									*/
/*  updatesrv.c	- Server Update main loop				*/
/*									*/
/*  Function  	- This is the server to update file servers dbs.	*/
/*		  The server is run on a central place.  It accepts	*/
/*		  only one type of request - UpdateFetch.  On a fetch	*/
/*		  it checks to see if the input time is the same as	*/
/*		  the mtime on the file.  If it is it just returns. 	*/
/*		  If the time is different it transfers the file	*/
/*		  back to the requestor.				*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <signal.h>
#include <string.h>



#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <map.h>
#include <portmapper.h>
#include <se.h>
extern void SFTP_SetDefaults (SFTP_Initializer *initPtr);
extern void SFTP_Activate (SFTP_Initializer *initPtr);
#include <vice.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "update.h"

#define UPDSRVNAME "updatesrv"
extern char *ViceErrorMsg(int errorCode);   /* should be in libutil */

static void SetDebug();
static void ResetDebug();
static void Terminate();
static void ServerLWP(int *Ident);

static char prefix[1024];

static struct timeval  tp;
static struct timezone tsp;


int main(int argc, char **argv)
{
    char    sname[20];
    char    pname[20];
    FILE * file;
    int     i, len;
    int     lwps = 2;
    int     badParm = 1;
    PROCESS parentPid, serverPid;
    RPC2_PortalIdent portal1, *portallist[1];
    RPC2_SubsysIdent server;
    SFTP_Initializer sftpi;
    int rc;
    long portmapid;
    
    rc = chdir("/vice/srv");
    if ( rc ) {
	    perror("Cannot cd to /vice/srv");
	    exit(1);
    }



    UtilDetach();

    strcpy(pname,"coda_udpsrv");

    for (i = 1; i < argc; i++) {
	    if (!strcmp(argv[i], "-d"))
		    /* perhaps there should be a UpdateDebugLevel? */
		    SrvDebugLevel = atoi(argv[++i]);  
	    else if (!strcmp(argv[i], "-l"))
		    lwps = atoi(argv[++i]);
	    else if (!strcmp(argv[i], "-p")) {
		    badParm = 0;
		    strcpy(prefix, argv[++i]);
	    } else 	if (!strcmp(argv[i], "-q")) {
		    badParm = 0;
		    strcpy(pname, argv[++i]);
	    } else 	{
		    fprintf(stderr, "Bad argument %s to update srv\n", 
			    argv[i]);
		    fprintf(stderr, "Usage: updatesrv [-p prefix"
			    "-d (debug level)]) [-l (number of lwps)]");
		    exit(1);
	    }
    }

    (void) signal(SIGHUP, (void (*)(int))ResetDebug);
    (void) signal(SIGUSR1, (void (*)(int))SetDebug);
    (void) signal(SIGQUIT, (void (*)(int))Terminate);
    
    if (chdir(prefix)) {
	    perror("could not chdir to prefix directory");
	    exit(-1);
    }

    freopen("UpdateSrvLog","a+",stdout);
    freopen("UpdateSrvLog","a+",stderr);

    file = fopen("/vice/srv/updatesrv.pid", "w");
    if ( !file ) {
	    perror("Error writing /vice/srv/updatesrv.pid");
	    exit(1);
    }
    fprintf(file, "%d", getpid());
    fclose(file);

    RPC2_DebugLevel = SrvDebugLevel / 10;

    assert(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY - 1, &parentPid) == LWP_SUCCESS);

    portal1.Tag = RPC2_PORTALBYINETNUMBER;
    portal1.Value.InetPortNumber = 0;

    SFTP_SetDefaults(&sftpi);
    sftpi.PacketSize = 1024;
    sftpi.WindowSize = 16;
    sftpi.SendAhead = 4;
    sftpi.AckPoint = 4;
    SFTP_Activate(&sftpi);
    RPC2_Trace = 0;
    tp.tv_sec = 80;
    tp.tv_usec = 0;
    assert(RPC2_Init(RPC2_VERSION, 0, &portal1, 6, &tp) == RPC2_SUCCESS);

    /* register the port with the portmapper */
    portmapid = portmap_bind("localhost");
    if ( !portmapid ) {
	    fprintf(stderr, "Cannot bind to rpc2portmap; exiting\n");
	    return 1;
    }
    rc = portmapper_client_register_sqsh(portmapid, 
					 (unsigned char *) "codaupdate", 
					 0, 17,  portal1.Value.InetPortNumber);

    if ( rc ) {
	    fprintf(stderr, "Cannot register with rpc2portmap; exiting\n");
	    return 1;
    }
    RPC2_Unbind(portmapid); 

    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_UPDATE;
    assert(RPC2_Export(&server) == RPC2_SUCCESS);

    for (i = 0; i < lwps; i++) {
	sprintf(sname, "ServerLWP-%d", i);
	assert(LWP_CreateProcess((PFIC)ServerLWP, 
				 32 * 1024, LWP_MAX_PRIORITY - 1,
				 (char *)&i, sname, &serverPid) 
	       == LWP_SUCCESS);
    }
    gettimeofday(&tp, &tsp);
    LogMsg(0, SrvDebugLevel, stdout, 
	   "Update Server started %s", ctime((long *)&tp.tv_sec));

    assert(LWP_WaitProcess((char *)&parentPid) == LWP_SUCCESS);

}


static void SetDebug()
{

    if (SrvDebugLevel > 0) {
	SrvDebugLevel *= 5;
    }
    else {
	SrvDebugLevel = 1;
    }
    RPC2_DebugLevel = SrvDebugLevel/10;
    LogMsg(0, SrvDebugLevel, stdout, 
	   "Set Debug On level = %d, RPC level = %d\n",
	   SrvDebugLevel, RPC2_DebugLevel);
}


static void ResetDebug()
{
    RPC2_DebugLevel = SrvDebugLevel = 0;
    LogMsg(0, SrvDebugLevel, stdout, "Reset Debug levels to 0\n");
}


static void Terminate()
{
    LogMsg(0, SrvDebugLevel, stdout, "Exiting updateclnt\n");
    exit(0);
}


static void ServerLWP(int *Ident)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer * myrequest;
    RPC2_Handle mycid;
    long     rc;
    int     lwpid;

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = SUBSYS_UPDATE;
    lwpid = *Ident;
    LogMsg(0, SrvDebugLevel, stdout,"Starting Update Worker %d\n", lwpid);

    while (1) {
	if ((rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 0, 0, 
				  RPC2_XOR, 0))
		== RPC2_SUCCESS) {
	    LogMsg(1, SrvDebugLevel, stdout,
		   "Worker %d received request %d\n", 
		   lwpid, myrequest->Header.Opcode);

	    rc = update_ExecuteRequest(mycid, myrequest, 0);
	    if (rc) {
		LogMsg(0, SrvDebugLevel, stdout,
		       "file.c request %d failed: %s\n",
		       myrequest->Header.Opcode, ViceErrorMsg((int)rc));
		if(rc <= RPC2_ELIMIT) {
		    RPC2_Unbind(mycid);
		}
	    }
	} else {
	    LogMsg(0, SrvDebugLevel, stdout,
		   "RPC2_GetRequest failed with %s\n",ViceErrorMsg((int)rc));
	}
    }
}


long UpdateFetch(RPC2_Handle RPCid, RPC2_String FileName, 
		 RPC2_Unsigned Time, RPC2_Unsigned *NewTime, 
		 RPC2_Unsigned *CurrentSecs, RPC2_Integer *CurrentUsecs, 
		 SE_Descriptor *File)
{
    long    rc;			/* return code to caller */
    SE_Descriptor sid;		/* sid to use to transfer */
    char    name[1024];		/* area to hold the name */
    char    dirname[1024];	/* area to hold the directory name */
    struct stat buff;		/* buffer for stat */
    int     fd = 0;
    char    * end;

    rc = 0;

    LogMsg(1, SrvDebugLevel, stdout, "UpdateFetch file = %s, Time = %d\n",
	    FileName, Time);

/*    strcpy(name, prefix); */
/*    strcat(name, FileName); */
    strcpy(name, (char *)FileName);
    if (stat(name, &buff)) {
	*NewTime = 0;
	goto Final;
    }
    *NewTime = buff.st_mtime;
    if (((buff.st_mode & S_IFMT) == S_IFREG) &&
	    (Time != *NewTime)) {
	strcpy(dirname, name);
	end = rindex(dirname, '/');
	if(end) *end = '\0';
	fd = open(dirname, O_RDONLY, 0);
	if(fd <= 0) {
	    perror("open for directory failed");
	    fd = 0;
	} else {
	    flock(fd, LOCK_SH);
	}
	sid.Tag = SMARTFTP;
	sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	sid.Value.SmartFTPD.SeekOffset = 0;
	if (SrvDebugLevel > 2) {
	    sid.Value.SmartFTPD.hashmark = '#';
	} else {
	    sid.Value.SmartFTPD.hashmark = 0;
	}
	sid.Value.SmartFTPD.Tag = FILEBYNAME;
	sid.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0;
	strcpy(sid.Value.SmartFTPD.FileInfo.ByName.LocalFileName, name);

	if (rc = RPC2_InitSideEffect(RPCid, &sid)) {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "InitSideEffect failed %s\n", ViceErrorMsg((int)rc));
	    if (rc <= RPC2_ELIMIT) {
		goto Final;
	    }
	}

	if (rc = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "CheckSideEffect failed %s\n", ViceErrorMsg((int)rc));
	    if (rc <= RPC2_ELIMIT) {
		goto Final;
	    }
	}
    }

Final:
    if(fd) {
	flock(fd, LOCK_UN);
	close(fd);
    }
    gettimeofday(&tp, &tsp);
    *CurrentSecs = tp.tv_sec;
    *CurrentUsecs = tp.tv_usec;
    LogMsg(2, SrvDebugLevel, stdout, 
	   "UpdateFetch returns %s newtime is %d at %s",
	   ViceErrorMsg((int)rc), *NewTime, ctime((long *)CurrentSecs));
    return(rc);
}


long UpdateNewConnection(RPC2_Handle cid, RPC2_Integer SideEffectType, 
			 RPC2_Integer SecurityLevel, 
			 RPC2_Integer EncryptionType, 
			 RPC2_Integer AuthType, 
			 RPC2_CountedBS *ClientIdent)
{
    LogMsg(0, SrvDebugLevel, stdout,
	   "New connection received %d, %d, %d\n", 0, 0, 0);
    /* at some point we should validate this connection as to its origin */
    return(0);
}
