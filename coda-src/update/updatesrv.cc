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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include "coda_assert.h"
#include <signal.h>
#include <fcntl.h>
#include "coda_string.h"
#include "coda_flock.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <map.h>
#include <portmapper.h>
#include <rpc2/se.h>
extern void SFTP_SetDefaults (SFTP_Initializer *initPtr);
extern void SFTP_Activate (SFTP_Initializer *initPtr);
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <vice_file.h>
#include <codaconf.h>
#include <coda_config.h>
#include "update.h"
#include "getsecret.h"

#define UPDSRVNAME "updatesrv"
extern char *ViceErrorMsg(int errorCode);   /* should be in libutil */

static void ReadConfigFile();

static void SetDebug();
static void ResetDebug();
static void Terminate();
static void ServerLWP(int *Ident);

static char *prefix = NULL;

static struct timeval  tp;
static struct timezone tsp;

static char *serverconf = SYSCONFDIR "/server"; /* ".conf" */
static char *vicedir = NULL;

void
ReadConfigFile()
{
    char    confname[MAXPATHLEN];

    /* don't complain if config files are missing */
    codaconf_quiet = 1;

    /* Load configuration file to get vice dir. */
    sprintf (confname, "%s.conf", serverconf);
    (void) conf_init(confname);

    CONF_STR(vicedir,		"vicedir",	   "/vice");

    vice_dir_init(vicedir, 0);
}

struct flist {
    char  *name;
    flist *next;
};
flist * namelist = NULL;
int checknames = 1;

void
ReadExportList()
{
    FILE * exportf;
    char   rdline[MAXPATHLEN];
    flist *entry;

    exportf = fopen (vice_sharedfile("db/files.export"), "r");
    if (!exportf) {
	checknames = 0;
	return;
    }
    while (fgets(rdline, MAXPATHLEN, exportf)) {
       if (rdline[strlen(rdline)-1] == '\n')
	 rdline[strlen(rdline)-1] = 0;
       LogMsg(3, SrvDebugLevel, stdout, "Export file: '%s'", rdline);
       entry = new flist;
       entry->name = strdup(rdline);
       entry->next = namelist;
       namelist = entry;
    }
    fclose (exportf);
}

int
InList (char *name)
{   flist *entry = namelist;

    LogMsg(3, SrvDebugLevel, stdout, "InList Lookup: '%s'", name);
    while (entry) {
         LogMsg(3, SrvDebugLevel, stdout, "InList strcmp: '%s'", entry->name);
        if (strcmp(entry->name,name) == 0)
	    return 1;
	entry = entry->next;
    }
    return 0;
}
    
int main(int argc, char **argv)
{
    char    sname[20];
    char    errmsg[MAXPATHLEN];
    FILE * file;
    int     i;
    int     lwps = 2;
    PROCESS parentPid, serverPid;
    RPC2_PortIdent port1;
    RPC2_SubsysIdent server;
    SFTP_Initializer sftpi;
    int rc;
    long portmapid;
    struct stat statbuf;
    char *miscdir;

    /* process the command line arguments */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d"))
	    /* perhaps there should be a UpdateDebugLevel? */
	    SrvDebugLevel = atoi(argv[++i]);  
	else if (!strcmp(argv[i], "-l"))
	    lwps = atoi(argv[++i]);
	else if (!strcmp(argv[i], "-p")) {
	    prefix = argv[++i];
	} else if (!strcmp(argv[i], "-q")) {
	    fprintf(stderr, "Old argument -q to update srv\n");
	    ++i;
	} else {
	    fprintf(stderr, "Bad argument %s to update srv\n", 
		    argv[i]);
	    fprintf(stderr, "Usage: updatesrv [-p prefix"
		    "-d (debug level)]) [-l (number of lwps)]\n");
	    exit(1);
	}
    }

    ReadConfigFile();
    ReadExportList();

    miscdir = vice_sharedfile("misc");
    if (stat(miscdir, &statbuf) == -1 && errno == ENOENT) {
	fprintf(stderr, "creating missing directory '%s'\n", miscdir);
	mkdir(miscdir, 0755);
    }

    rc = chdir(miscdir);
    if ( rc ) {
        snprintf (errmsg, MAXPATHLEN, "Could not chdir to %s",
		  vice_sharedfile("misc"));
	perror(errmsg);
	exit(1);
    }

    UtilDetach();
    
    (void) signal(SIGHUP, (void (*)(int))ResetDebug);
    (void) signal(SIGUSR1, (void (*)(int))SetDebug);
    (void) signal(SIGQUIT, (void (*)(int))Terminate);
    
    freopen("UpdateSrvLog","a+",stdout);
    freopen("UpdateSrvLog","a+",stderr);
    fprintf(stderr, "Updatesrv started!\n");

    if (!prefix)
      prefix = strdup(vice_sharedfile(NULL));

    if (chdir(prefix)) {
	snprintf (errmsg, MAXPATHLEN, "Could not chdir to %s", prefix);
	perror (errmsg);
	exit(-1);
    }

    LogMsg(0, SrvDebugLevel, stdout, "Update Server working directory %s",
	   prefix);
    
    file = fopen(vice_sharedfile("misc/updatesrv.pid"), "w");
    if ( !file ) {
        snprintf (errmsg, MAXPATHLEN, "Error in writing %s",
		  vice_sharedfile("misc/updatesrv.pid"));
	perror(errmsg);
	exit(1);
    }
    fprintf(file, "%d", getpid());
    fclose(file);

    RPC2_DebugLevel = SrvDebugLevel / 10;

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY - 1, &parentPid) == LWP_SUCCESS);

    port1.Tag = RPC2_PORTBYINETNUMBER;
    port1.Value.InetPortNumber = 0;

    SFTP_SetDefaults(&sftpi);
    sftpi.PacketSize = 1024;
    sftpi.WindowSize = 16;
    sftpi.SendAhead = 4;
    sftpi.AckPoint = 4;
    SFTP_Activate(&sftpi);
    RPC2_Trace = 0;
    tp.tv_sec = 80;
    tp.tv_usec = 0;
    CODA_ASSERT(RPC2_Init(RPC2_VERSION, 0, &port1, 6, &tp) == RPC2_SUCCESS);
    RPC2_enableReaping = 1;

    /* register the port with the portmapper */
    fprintf(stderr, "Attempting to bind with rpc2portmap\n");
    portmapid = portmap_bind("localhost");
    if ( !portmapid ) {
	    fprintf(stderr, "Cannot bind to rpc2portmap; exiting\n");
	    return 1;
    }
    rc = portmapper_client_register_sqsh(portmapid, 
					 (unsigned char *) "codaupdate", 
					 0, 17,
					 ntohs(port1.Value.InetPortNumber));

    if ( rc ) {
	    fprintf(stderr, "Cannot register with rpc2portmap; exiting, rc = %i \n", rc);
	    return 1;
    }
    RPC2_Unbind(portmapid); 

    server.Tag = RPC2_SUBSYSBYID;
    server.Value.SubsysId = SUBSYS_UPDATE;
    CODA_ASSERT(RPC2_Export(&server) == RPC2_SUCCESS);

    for (i = 0; i < lwps; i++) {
	sprintf(sname, "ServerLWP-%d", i);
	CODA_ASSERT(LWP_CreateProcess((PFIC)ServerLWP, 
				 32 * 1024, LWP_MAX_PRIORITY - 1,
				 (char *)&i, sname, &serverPid) 
	       == LWP_SUCCESS);
    }
    gettimeofday(&tp, &tsp);
    LogMsg(0, SrvDebugLevel, stdout, 
	   "Update Server started %s", ctime((long *)&tp.tv_sec));

    CODA_ASSERT(LWP_WaitProcess((char *)&parentPid) == LWP_SUCCESS);

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
	   "Set Debug On level = %d, RPC level = %d",
	   SrvDebugLevel, RPC2_DebugLevel);
}


static void ResetDebug()
{
    RPC2_DebugLevel = SrvDebugLevel = 0;
    LogMsg(0, SrvDebugLevel, stdout, "Reset Debug levels to 0");
}


static void Terminate()
{
    LogMsg(0, SrvDebugLevel, stdout, "Exiting updateclnt");
    exit(0);
}

static long Update_GetKeys(RPC2_Integer authtype, RPC2_CountedBS *cident,
			   RPC2_EncryptionKey sharedsecret,
			   RPC2_EncryptionKey sessionkey)
{
    unsigned int i;

    /* Block unauthenticated connections */
    if (!cident) return -1;

    if (GetSecret(vice_sharedfile("db/update.tk"), sharedsecret) == -1)
	return -1;

    memset(sessionkey, 0, RPC2_KEYSIZE);
    for (i = 0; i < RPC2_KEYSIZE / sizeof(int); i++)
	((int *)sessionkey)[i] = rpc2_NextRandom(NULL);
    
    return 0;
}

static long Update_AuthFail(RPC2_Integer authtype,
			    RPC2_CountedBS *cident,
			    RPC2_Integer encryptiontype,
			    RPC2_HostIdent *host,
			    RPC2_PortIdent *port)
{
    LogMsg(0, SrvDebugLevel, stdout, "Access from %s failed with bad token",
	   inet_ntoa(host->Value.InetAddress));
    return 0;
}

static void ServerLWP(int *Ident)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer * myrequest;
    RPC2_Handle mycid;
    RPC2_PeerInfo peer;
    long     rc;
    int     lwpid;

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = SUBSYS_UPDATE;
    lwpid = *Ident;
    LogMsg(0, SrvDebugLevel, stdout,"Starting Update Worker %d", lwpid);

    while (1) {
	if ((rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, 0,
				  Update_GetKeys, RPC2_XOR,
				  Update_AuthFail))
		== RPC2_SUCCESS) {

	    RPC2_GetPeerInfo(mycid, &peer);

	    if (peer.SecurityLevel == RPC2_OPENKIMONO) {
                LogMsg(0, SrvDebugLevel, stdout,
                       "Receiving unauthenticated request %d, "
                       "update rpc2 library!", myrequest->Header.Opcode);
                RPC2_Unbind(mycid);
                continue;
            }

            LogMsg(1, SrvDebugLevel, stdout,"Worker %d received request %d",
                   lwpid, myrequest->Header.Opcode);
            rc = update_ExecuteRequest(mycid, myrequest, 0);

	    if (rc)
	    {
		LogMsg(0, SrvDebugLevel, stdout,
		       "Request %d from %s:%d failed: %s",
		       myrequest->Header.Opcode,
		       inet_ntoa(peer.RemoteHost.Value.InetAddress),
		       ntohs(peer.RemotePort.Value.InetPortNumber),
		       ViceErrorMsg((int)rc));

		if(rc <= RPC2_ELIMIT)
		    RPC2_Unbind(mycid);
	    }
	} else
	    LogMsg(0, SrvDebugLevel, stdout,
		   "RPC2_GetRequest failed with %s",ViceErrorMsg((int)rc));
    }
}

int AccessAllowed(char *name)
{
    char *p = name;

    /* Do we need to check? */
    if (!checknames) return 1;

    /* Any `..' in the name, deny access! */
    while (1) {
	p = strchr(p, '.');
	if (!p) break;
	if (*(++p) == '.') return 0;
    }

    /* If the file is in the export list, allow */
    if (InList(name)) return 1;

    /* No access granted */
    return 0;
}

long UpdateFetch(RPC2_Handle RPCid, RPC2_String FileName, 
		 RPC2_Unsigned Time, RPC2_Unsigned *NewTime, 
		 RPC2_Unsigned *CurrentSecs, RPC2_Integer *CurrentUsecs, 
		 SE_Descriptor *File)
{
    long    rc = 0;		/* return code to caller */
    SE_Descriptor sid;		/* sid to use to transfer */
    char    name[MAXPATHLEN];	/* area to hold the name */
    struct stat buff;		/* buffer for stat */
    int     len;

    rc = 0;

    len = strlen((char *)FileName) + 1;
    if (len > MAXPATHLEN) len = MAXPATHLEN;
    memcpy(name, FileName, len);
    name[MAXPATHLEN-1] = '\0';

    LogMsg(1, SrvDebugLevel, stdout, "UpdateFetch file = %s, Time = %d",
	    name, Time);

    *NewTime = 0;

    if (!AccessAllowed((char *)name)) {
        LogMsg(0, SrvDebugLevel, stdout, "Access denied to file %s.", name);
	rc = CEACCES;
	goto Final;
    }

    if (stat(name, &buff) != 0) {
	rc = ENOENT;
	goto Final;
    }

    *NewTime = buff.st_mtime;
    if ((buff.st_mode & S_IFMT) == S_IFREG && Time != *NewTime)
    {
	memset(&sid, 0, sizeof(SE_Descriptor));
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

	if ((rc = RPC2_InitSideEffect(RPCid, &sid))) {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "InitSideEffect failed %s", ViceErrorMsg((int)rc));
	    if (rc <= RPC2_ELIMIT) {
		goto Final;
	    }
	}

	if ((rc = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS))) {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "CheckSideEffect failed %s", ViceErrorMsg((int)rc));
	    if (rc <= RPC2_ELIMIT) {
		goto Final;
	    }
	}
    }

Final:
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
    RPC2_PeerInfo peer;
    RPC2_GetPeerInfo(cid, &peer);

    LogMsg(0, SrvDebugLevel, stdout, "Connection from %s:%d.",
	   inet_ntoa(peer.RemoteHost.Value.InetAddress),
	   ntohs(peer.RemotePort.Value.InetPortNumber));

    return 0;
}
