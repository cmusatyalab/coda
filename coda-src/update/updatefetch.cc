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


/*  updatefetch.cc - Client fetch files from the server  */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <netinet/in.h>
#include "coda_assert.h"
#include "coda_string.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>

#include <portmapper.h>
#include <map.h>
#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/sftp.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "update.h"
#include <volutil.h>

extern char *ViceErrorMsg(int errorCode);   /* should be in libutil */

static int FetchFile(char *, char *, int);
static void Connect();
static void PrintHelp();
static void ProcessArgs(int argc, char **argv);
static void U_InitRPC();
static int U_BindToServer(char *fileserver, RPC2_Handle *RPCid);

static char *LocalFileName = NULL, *RemoteFileName = NULL;

static RPC2_Handle con;
static char host[256];
static char *pname = "coda_udpsrv";

/*static struct timeval  tp;
static struct timezone tsp; */

static char s_hostname[100];
static RPC2_EncryptionKey vkey;	/* Encryption key for bind authentication */

int main(int argc, char **argv)
{
    struct timeval  time;
    FILE * file = NULL;
    int rc;

    host[0] = '\0';

    ProcessArgs(argc, argv);

    gethostname(s_hostname, sizeof(s_hostname) -1);
    CODA_ASSERT(s_hostname != NULL);

    RPC2_DebugLevel = SrvDebugLevel;

    /* initialize RPC2 and the tokens */
    U_InitRPC();

    /* connect to updatesrv */
    Connect();

    /* make sure we can open LocalFileName for writing */
    file = fopen(LocalFileName, "w");
    CODA_ASSERT(file);
    fclose(file);

    /* off we go */
    rc = FetchFile(RemoteFileName, LocalFileName, 0600);
    if ( rc ) {
      fprintf(stderr, "%s failed with %s\n", argv[0], ViceErrorMsg((int) rc));
    }
    return rc;

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
	    if (!strcmp(argv[i], "-r"))
	      RemoteFileName = argv[++i];
	else
	    if (!strcmp(argv[i], "-l"))
		LocalFileName = argv[++i];
	else
	    if (!strcmp(argv[i], "-q"))
		strcpy(pname, argv[++i]);
	else {
	    PrintHelp();
	    exit(-1);
	}
    }
    if ( host[0] == '\0' || (!LocalFileName) || (!RemoteFileName) ) {
        PrintHelp();
        exit(-1);
    }
}

static void PrintHelp(){
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "usage: updatefetch -h serverhostname");
	    LogMsg(0, SrvDebugLevel, stdout, 
		   "-r remotefile -l localfile");
	    LogMsg(0, SrvDebugLevel, stdout, 
		   " [-d debuglevel]  [-q  port]\n");
}

static int FetchFile(char *RemoteFileName, char *LocalFileName, int mode)
{
    struct stat buff;
    RPC2_Unsigned time, newtime, currentsecs;
    RPC2_Integer currentusecs;
    long     rc;
    SE_Descriptor sed;

    time = 0; /* tell server to just ship the file, without checking on times */
    
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.SeekOffset = 0;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    if (SrvDebugLevel > 0)
	sed.Value.SmartFTPD.hashmark = '#';
    else
	sed.Value.SmartFTPD.hashmark = '\0';
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0666;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, LocalFileName);

    rc = UpdateFetch(con, (RPC2_String)RemoteFileName, time, &newtime, &currentsecs, &currentusecs, &sed);

    if (rc) {
	unlink(LocalFileName);
	LogMsg(0, SrvDebugLevel, stdout, "Fetch failed with %s\n", ViceErrorMsg((int)rc));
    } 

    return(rc);
}


static void Connect()
{
    long     rc;
    RPC2_PortIdent sid;
    RPC2_SubsysIdent ssid;
    RPC2_HostIdent hid;
    RPC2_CountedBS dummy;
    long portmapid;
    long port;

    portmapid = portmap_bind(host);
    if ( !portmapid ) {
	    fprintf(stderr, "Cannot bind to rpc2portmap; exiting\n");
	    exit(1);
    }
    rc = portmapper_client_lookup_pbynvp(portmapid, (unsigned char *)"codaupdate", 0, 17, &port);

    if ( rc ) {
	    fprintf(stderr, "Cannot get port from rpc2portmap; exiting\n");
	    exit(1);
    }
    RPC2_Unbind(portmapid);

    hid.Tag = RPC2_HOSTBYNAME;
    strcpy(hid.Value.Name, host);
    sid.Tag = RPC2_PORTBYINETNUMBER;
    sid.Value.InetPortNumber = htons(port);
    ssid.Tag = RPC2_SUBSYSBYID;
    ssid.Value.SubsysId = SUBSYS_UPDATE;
    dummy.SeqLen = 0;


    RPC2_BindParms bparms;
    bzero((void *)&bparms, sizeof(bparms));
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;

    if ( rc = RPC2_NewBinding(&hid, &sid, &ssid, &bparms, &con)) {
        LogMsg(0, SrvDebugLevel, stdout, "Bind failed with %s\n", (char *)ViceErrorMsg((int)rc));
	exit (-1);
    }
}



static void U_InitRPC()
{
    PROCESS mylpid;
    FILE *tokfile;
    SFTP_Initializer sftpi;
    long rcode;

    /* store authentication key */
    tokfile = fopen(VolTKFile, "r");
    if ( !tokfile ) {
	    fprintf(stderr, "No tokenfile: %s\n", VolTKFile);
	    exit(1);
    }
    memset(vkey, 0, RPC2_KEYSIZE);
    fread(vkey, 1, RPC2_KEYSIZE, tokfile);
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

