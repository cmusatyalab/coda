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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/update/updateclnt.cc,v 4.17 1998/11/24 15:34:46 jaharkes Exp $";
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
/*								*/
/*  updateclnt.c  - Client to update server data bases			*/
/*								*/
/*  Function  	- This is the client to update file servers dbs	*/
/*		  It checks every waitinterval (default 30 secs)	*/
/*		  to see if any of the file listed in the files        */
/*		  file has changed.  It fetches any new files.         */
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <netinet/in.h>
#include "coda_assert.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <lock.h>
#include <lwp.h>
#include <rpc2.h>
#include <se.h>
#include "timer.h"
#include "sftp.h"
#include <map.h>
#include <portmapper.h>
#include <vice.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "update.h"
#include <volutil.h>
extern long VolUpdateDB(RPC2_Handle);

extern char *ViceErrorMsg(int errorCode);   /* should be in libutil */

#define UPDATENAME "updateclnt"
#define MONITORNAME "UpdateMonitor"
#define NEEDNEW 232
#define LISTNAME "files"
#define LISTLEN 4096

static void CheckLibStructure();
static int CheckDir(char *prefix, int mode);
static int CheckFile(char *fileName, int mode);
static void ProcessArgs(int argc, char ** argv);
static void ReConnect();
static void SetDebug();
static void ResetDebug();
static void Terminate();
static void SetCheck();
static void SwapLog();
static void U_InitRPC();
static int U_BindToServer(char *fileserver, RPC2_Handle *RPCid);

static int Rebind = 0;
static int Reexec = 0;
static int ReadOnlyAllowed = 0;
static int CheckAll = 1;

static RPC2_Unsigned operatorSecs = 0;
static RPC2_Integer operatorUsecs = 0;

static RPC2_Handle con;
static char host[256];
static int waitinterval = 30;	/* 5 min */
static int reps = 6;
static char pname[20];

static struct timeval  tp;
static struct timezone tsp;

static char s_hostname[100];
static char vkey[RPC2_KEYSIZE+1];	/* Encryption key for bind authentication */

int main(int argc, char **argv)
{
    struct timeval  time;
    FILE * file;
    int     i,
	    dirfd,
            len,

    rc = chdir("/vice/db");
    if ( rc ) {
	    perror("Cannot cd to /vice/db");
	    exit(1);
    }


    UtilDetach();
    *host = '\0';
    strcpy(pname, "coda_udpsrv");

    ProcessArgs(argc, argv);
    CheckLibStructure();

    gethostname(s_hostname, sizeof(s_hostname) -1);
    CODA_ASSERT(s_hostname != NULL);

    (void) signal(SIGQUIT, (void (*)(int))Terminate);
    (void) signal(SIGHUP, (void (*)(int))ResetDebug);
    (void) signal(SIGUSR1, (void (*)(int))SetDebug);
#ifndef __CYGWIN32__
    (void) signal(SIGXFSZ, (void (*)(int))SetCheck);
    (void) signal(SIGXCPU, (void (*)(int))SwapLog);
#endif

    len = strlen(argv[0]);
    if (len > strlen(UPDATENAME)) {
	for (i = 0; i < len; i++) {
	    *(argv[0] + i) = ' ';
	}
	strcpy(argv[0], UPDATENAME);
    }

    freopen("/vice/srv/UpdateClntLog", "a+", stdout);
    freopen("/vice/srv/UpdateClntLog", "a+", stderr);

    CODA_ASSERT(file = fopen("/vice/srv/updateclnt.pid", "w"));
    fprintf(file, "%d", getpid());
    fclose(file);
    RPC2_DebugLevel = SrvDebugLevel / 10;

    U_InitRPC();
    Rebind = 1;

    gettimeofday(&tp, &tsp);
    LogMsg(0, SrvDebugLevel, stdout, 
	   "Update Client pid = %d started at %s", 
	   getpid(), ctime((long *)&tp.tv_sec));

    time.tv_sec = waitinterval;
    time.tv_usec = 0;
    i = reps;

    while (1) {

	if (Rebind) {
	    ReConnect();
	    Rebind = 0;
	    i = reps;
	}

	dirfd = open("/vice",O_RDONLY,0);
	if(dirfd > 0)
	    flock(dirfd,LOCK_EX);
	else
	    dirfd = 0;

	if (CheckDir("/vice/db", 0644)) {
	    operatorSecs = 0;	/* if something changed time has elapsed */
	    /* signal file server to check data bases */
	    file = fopen("/vice/srv/pid", "r");
	    if (file == NULL) {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "Fopen failed for file server pid file with %s\n",
		       ViceErrorMsg(errno));
	    } else {
		RPC2_Handle rpcid;
		if (U_BindToServer(s_hostname, &rpcid) == RPC2_SUCCESS) {
		    if (VolUpdateDB(rpcid) == RPC2_SUCCESS) {
			LogMsg(0, SrvDebugLevel, stdout, 
			       "Notifying fileserver of database updates\n");
		    } else {
			LogMsg(0, SrvDebugLevel, stdout, 
			       "VolUpdateDB failed\n");
		    }
		} else {
		    LogMsg(0, SrvDebugLevel, stdout, 
			   "Bind to server for database update failed\n");
		}
		RPC2_Unbind(rpcid);
	    }
	}
	if (operatorSecs > 0) {
	    gettimeofday(&tp, &tsp);
	    if ((tp.tv_sec < operatorSecs) || 
		(tp.tv_sec > (operatorSecs + 2))) {
		tp.tv_sec = operatorSecs + 1;
		tp.tv_usec = operatorUsecs;
		LogMsg(0, SrvDebugLevel, stdout,
		    "Time between servers differs, "
		    "use `xntpd' to keep the time synchronized");
		/*
		LogMsg(0, SrvDebugLevel, stdout, 
		       "Settime to %s", ctime((long *)&tp.tv_sec));
		settimeofday(&tp, &tsp);
		*/
	    }
	}

	if(dirfd) {
	    close(dirfd);
	    dirfd = 0;
	}

	if (Reexec) {
	    RPC2_Unbind(con);
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "Binaries have changed, Restart\n");
	    exit(0);
	}

	CheckAll = 1;

	IOMGR_Select(0, 0, 0, 0, &time);
    }
}

static void ProcessArgs(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-d"))
	    SrvDebugLevel = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-h"))
		strcpy(host, argv[++i]);
	else
	    if (!strcmp(argv[i], "-q"))
		strcpy(pname, argv[++i]);
	else
	    if (!strcmp(argv[i], "-w"))
		waitinterval = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-r"))
		reps = atoi(argv[++i]);
	else {
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "usage: update [-d (debug level)] ");
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "[-h (operator console hostname)] [-q (port name)]");
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "[-r (reps of w for long wait time)] [-w (short wait time)]\n");
	    exit(-1);
	}
	if ( host[0] == '\0' ) {
	    LogMsg(0, SrvDebugLevel, stdout, "No host given!\n");
	    exit(-1);
	}
    }
}


static void CheckLibStructure()
{
    struct stat lbuf;

    if((stat("/vice",&lbuf)) && (errno == ENOENT)) {
	printf("Creating /vice structure\n");
	mkdir("/vice",0755);
	mkdir("/vice/db",0755);
	mkdir("/vice/srv",0755);
	mkdir("/vice/vol",0755);
	mkdir("/vice/spool",0755);
    }
    else {
	if((stat("/vice/db",&lbuf)) && (errno == ENOENT)) {
	    printf("Creating /vice/db\n");
	    mkdir("/vice/db",0755);
	}
	if((stat("/vice/srv",&lbuf)) && (errno == ENOENT)) {
	    printf("Creating /vice/srv\n");
	    mkdir("/vice/srv",0755);
	}
	if((stat("/vice/vol",&lbuf)) && (errno == ENOENT)) {
	    printf("Creating /vice/vol\n");
	    mkdir("/vice/vol",0755);
	}
	if((stat("/vice/spool",&lbuf)) && (errno == ENOENT)) {
	    printf("Creating /vice/spool\n");
	    mkdir("/vice/spool",0755);
	}
    }
}


static int CheckDir(char *prefix, int mode)
{
    static char list[LISTLEN],
	name[256],
	newname[1024];
    int     i,
	j,
	fd,
	nfd,
	len,
	rc = 0;
    struct stat buff;

    LogMsg(1, SrvDebugLevel, stdout, "Checking %s\n", prefix);
    if ((!(CheckFile(prefix, 0755))) && !CheckAll)
	return(0);

    LogMsg(1, SrvDebugLevel, stdout, 
	   "Directory %s changed, check files\n", prefix);

    strcpy(newname, prefix);
    strcat(newname, "/");
    strcat(newname, LISTNAME);

    if (CheckFile(newname, 0644))
	LogMsg(0, SrvDebugLevel, stdout, "Updated %s\n", newname);

    fd = open(newname, O_RDONLY, 0666);
    if (fd <= 0) {
	perror("Open failed for list data set");
	return(0);
    }
    if (fstat(fd, &buff)) {
	perror("Stat failed for list data set");
	return(0);
    }

    for (len = LISTLEN; len == LISTLEN;) {
	if ((len = read(fd, list, LISTLEN)) <= 0)
	    break;
	for (i = j = 0; i < len; i++) {
	    if (list[i] == '\n' || list[i] == '\0') {
		name[j] = '\0';
		if (j > 0) {
		    if (name[0] == '-') {
			strcpy(newname, prefix);
			strcat(newname, "/");
			strcat(newname, &name[1]);
			LogMsg(0, SrvDebugLevel, stdout, "Removing %s\n", newname);
			unlink(newname);
		    }
		    else {
			strcpy(newname, prefix);
			strcat(newname, "/");
			strcat(newname, name);
			if (CheckFile(newname, mode)) {
			    rc = 1;
			    LogMsg(0, SrvDebugLevel, stdout, "Updated %s\n", newname);
			    if ((strcmp(prefix, "/vice/bin") == 0)) {
				if (strcmp(name, "updateclnt") == 0) {
				    Reexec = 1;
				}
				if (strcmp(name, "file") == 0) {
				    nfd = open("/vice/srv/NEWFILE", O_CREAT + O_RDWR, 0666);
				    close(nfd);
				}
			    }
			}
		    }
		}
		j = 0;
	    }
	    else {
		name[j++] = list[i];
	    }
	}
    }

    close(fd);
    return(rc);
}


/* fileName is the full pathname */
static int CheckFile(char *fileName, int mode)
{
    static char oldname[1024];
    static char tmpname[1024];
    struct stat buff;
    struct timeval  times[2];
    RPC2_Unsigned time, newtime, currentsecs;
    RPC2_Integer currentusecs;
    long     rc;
    SE_Descriptor sed;

    if (stat(fileName, &buff)) {
	time = 0;
    }
    else {
	time = buff.st_mtime;
    /* do not update if owner cannot write and it is a normal file */
	if (!(buff.st_mode & S_IWRITE) && (buff.st_mode & S_IFREG) && ReadOnlyAllowed) {
	    LogMsg(0, SrvDebugLevel, stdout, "%s write protected against owner, not updated\n",fileName);
	    return(0);
	}
    }

    strcpy(tmpname, fileName);
    strcat(tmpname, ".UPD");
    strcpy(oldname, fileName);
    strcat(oldname, ".BAK");

    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.SeekOffset = 0;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    if (SrvDebugLevel > 0)
	sed.Value.SmartFTPD.hashmark = '#';
    else
	sed.Value.SmartFTPD.hashmark = '\0';
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0666;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, tmpname);

    rc = UpdateFetch(con, (RPC2_String)fileName, time, &newtime, &currentsecs, &currentusecs, &sed);

    if (rc) {
	operatorSecs = 0;
	unlink(tmpname);
	LogMsg(0, SrvDebugLevel, stdout, "Fetch failed with %s\n", ViceErrorMsg((int)rc));
	if (rc <= RPC2_ELIMIT) {
	    Rebind = 1;
	    return(0);
	}
    }
    else {
	if (currentsecs > newtime) {
	    operatorSecs = currentsecs;
	    operatorUsecs = currentusecs;
	}
	else {
	    operatorSecs = 0;
	}
    }

    if ((newtime != 0) && (time != newtime)) {
	rc = NEEDNEW;
	times[0].tv_usec = times[1].tv_usec = 0;
	times[0].tv_sec = times[1].tv_sec = newtime;
	if (((buff.st_mode & S_IFMT) == S_IFREG) || (time == 0)) {
	    unlink(oldname);
	    if ((time != 0) && (rename(fileName, oldname))) {
		LogMsg(0, SrvDebugLevel, stdout, "rename %s to %s failed: %s\n", fileName, oldname, ViceErrorMsg(errno));
	    }
	    if (rename(tmpname, fileName)) {
		LogMsg(0, SrvDebugLevel, stdout, "rename %s to %s failed: %s\n", tmpname, fileName, ViceErrorMsg(errno));
	    }
	}
	chmod(fileName, mode);
	if (utimes(fileName, times)) {
	    perror("utimes failed with");
	}
    }
    else
	rc = 0;

    return(rc);
}


static void ReConnect()
{
    long     rc;
    struct timeval  time;
    RPC2_PortIdent sid;
    RPC2_SubsysIdent ssid;
    RPC2_HostIdent hid;
    RPC2_CountedBS dummy;
    int     i;
    long portmapid;
    long port;

    if (con != 0) {
	LogMsg(0, SrvDebugLevel, stdout, "Unbinding\n");
	RPC2_Unbind(con);
    }

    for (i = 0; i < 100; i++) {
	    portmapid = portmap_bind(host);
	    if ( !portmapid ) {
		    fprintf(stderr, "Cannot bind to rpc2portmap; exiting\n");
		    IOMGR_Select(0, 0, 0, 0, &time);
		    continue;
	    }
	    rc = portmapper_client_lookup_pbynvp(portmapid, (unsigned char *)"codaupdate", 0, 17, &port);
	    
	    if ( rc ) {
		    fprintf(stderr, "Cannot get port from rpc2portmap; exiting\n");
		    RPC2_Unbind(portmapid);
		    IOMGR_Select(0, 0, 0, 0, &time);
		    continue;
	    } else 
		    RPC2_Unbind(portmapid);

	    
	    hid.Tag = RPC2_HOSTBYNAME;
	    strcpy(hid.Value.Name, host);
	    sid.Tag = RPC2_PORTBYINETNUMBER;
	    sid.Value.InetPortNumber = port;
	    ssid.Tag = RPC2_SUBSYSBYID;
	    ssid.Value.SubsysId= SUBSYS_UPDATE;
	    dummy.SeqLen = 0;
	    
	    time.tv_sec = waitinterval;
	    time.tv_usec = 0;
	    
	    RPC2_BindParms bparms;
	    bzero((void *)&bparms, sizeof(bparms));
	    bparms.SecurityLevel = RPC2_OPENKIMONO;
	    bparms.SideEffectType = SMARTFTP;
	    
	    if (rc = RPC2_NewBinding(&hid, &sid, &ssid, &bparms, &con)) {
		    LogMsg(0, SrvDebugLevel, stdout, 
			   "Bind failed with %s\n", 
			   (char *)ViceErrorMsg((int)rc));
		    if (rc < RPC2_ELIMIT)
			    IOMGR_Select(0, 0, 0, 0, &time);
		    else
			    break;
	    }
	    else
		    return ;
    }
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
    LogMsg(0, SrvDebugLevel, stdout, "Set Debug On level = %d, RPC level = %d\n",SrvDebugLevel, RPC2_DebugLevel);
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


static void SwapLog()
{
    struct timeval tv;
    struct timezone tz;

    if(rename("UpdateLog","UpdateLog.old")) {
	LogMsg(0, SrvDebugLevel, stdout, "Rename for UpdateLog failed with a %s\n", ViceErrorMsg(errno));
    }
    else {
	gettimeofday(&tv, &tz);
	LogMsg(0, SrvDebugLevel, stdout, "Moving UpdateLog to UpdateLog.old at %s", ctime((long *)&tv.tv_sec));
	freopen("UpdateLog","a+",stdout);
	freopen("UpdateLog","a+",stderr);
	LogMsg(0, SrvDebugLevel, stdout, "New UpdateLog started at %s", ctime((long *)&tv.tv_sec));
    }
}


static void SetCheck()
{
    CheckAll = 1;
    LogMsg(0, SrvDebugLevel, stdout, "Check all files at next interval\n");
}

static void U_InitRPC()
{
    PROCESS mylpid;
    FILE *tokfile;
    SFTP_Initializer sftpi;
    long rcode;

    /* store authentication key */
    tokfile = fopen(TKFile, "r");
    fscanf(tokfile, "%s", vkey);
    fclose(tokfile);

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
    sftpi.PacketSize = 1024;
    sftpi.WindowSize = 16;
    sftpi.SendAhead = 4;
    sftpi.AckPoint = 4;
    SFTP_Activate(&sftpi);
    rcode = RPC2_Init(RPC2_VERSION, 0, NULL, -1, 0);
    if (rcode != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout, "RPC2_Init failed with %s\n", RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}


static int U_BindToServer(char *fileserver, RPC2_Handle *RPCid)
{
 /* Binds to File Server on volume utility port on behalf of uName.
    Sets RPCid to the value of the connection id.    */

    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_SubsysIdent sident;
    long     rcode;

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, fileserver);
    pident.Tag = RPC2_PORTBYNAME;
    strcpy(pident.Value.Name, "codasrv");
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = UTIL_SUBSYSID;

    RPC2_BindParms bparms;
    bzero((void *)&bparms, sizeof(bparms));
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;

    LogMsg(9, SrvDebugLevel, stdout, "V_BindToServer: binding to host %s\n", fileserver);
    rcode = RPC2_NewBinding(&hident, &pident, &sident, &bparms, RPCid);
    if (rcode < 0 && rcode > RPC2_ELIMIT)
	rcode = 0;
    if (rcode == 0 || rcode == RPC2_NOTAUTHENTICATED || rcode == RPC2_NOBINDING)
	return(rcode);
    else {
	LogMsg(0, SrvDebugLevel, stdout, "RPC2_NewBinding to server %s failed with %s\n",
				fileserver, RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}

