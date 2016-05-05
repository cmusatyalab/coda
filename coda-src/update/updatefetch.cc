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
                           none currently

#*/


/*  updatefetch.cc - Client fetch files from the server  */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
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

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/sftp.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <codaconf.h>
#include <vice_file.h>
#include <coda_getservbyname.h>
#include "update.h"
#include "getsecret.h"

extern char *ViceErrorMsg(int errorCode);   /* should be in libutil */

static int FetchFile(char *, char *, int);
static void Connect();
static void PrintHelp();
static void ProcessArgs(int argc, char **argv);
static void U_InitRPC();

static char *LocalFileName = NULL, *RemoteFileName = NULL;

static RPC2_Handle con;
static char host[256];
static RPC2_Integer port;

/*static struct timeval  tp;
static struct timezone tsp; */

static char s_hostname[100];

static void
ReadConfigFile()
{
    const char *vicedir;

    /* Load configuration file to get vice dir. */
    codaconf_init("server.conf");

    vicedir = codaconf_lookup("vicedir", "/vice");
    vice_dir_init(vicedir);
}

int main(int argc, char **argv)
{
    FILE * file = NULL;
    int rc;

    host[0] = '\0';

    ReadConfigFile();

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
            if (!strcmp(argv[i], "-port"))
                port = atoi(argv[++i]);
	else {
	    PrintHelp();
	    exit(-1);
	}
    }
    if (host[0] == '\0' || (!LocalFileName) || (!RemoteFileName)) {
	PrintHelp();
	exit(-1);
    }
}

static void PrintHelp()
{
    LogMsg(0, SrvDebugLevel, stdout, "usage: updatefetch -h serverhostname");
    LogMsg(0, SrvDebugLevel, stdout, "-r remotefile -l localfile");
    LogMsg(0, SrvDebugLevel, stdout, " [-d debuglevel]  [-port serverport]\n");
}

static int FetchFile(char *RemoteFileName, char *LocalFileName, int mode)
{
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
    static struct secret_state state = { 0, };
    long     rc;
    RPC2_SubsysIdent ssid;
    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_CountedBS cident;
    RPC2_EncryptionKey secret;
    char hostname[64];

    if (!port) {
	struct servent *s = coda_getservbyname("codasrv-se", "udp");
	port = ntohs(s->s_port);
    }

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, host);
    pident.Tag = RPC2_PORTBYINETNUMBER;
    pident.Value.InetPortNumber = htons(port);
    ssid.Tag = RPC2_SUBSYSBYID;
    ssid.Value.SubsysId = SUBSYS_UPDATE;

    RPC2_BindParms bparms;
    memset((void *)&bparms, 0, sizeof(bparms));
    bparms.SecurityLevel = RPC2_AUTHONLY;
    bparms.EncryptionType = RPC2_XOR;
    bparms.SideEffectType = SMARTFTP;

    gethostname(hostname, 63); hostname[63] = '\0';
    cident.SeqBody = (RPC2_ByteSeq)&hostname;
    cident.SeqLen = strlen(hostname) + 1;
    bparms.ClientIdent = &cident;

    GetSecret(vice_config_path("db/update.tk"), secret, &state);
    bparms.SharedSecret = &secret;

    rc = RPC2_NewBinding(&hident, &pident, &ssid, &bparms, &con);
    if (rc) {
	LogMsg(0, SrvDebugLevel, stdout, "Bind failed with %s\n", (char *)ViceErrorMsg((int)rc));
	exit (-1);
    }
}



static void U_InitRPC()
{
    PROCESS mylpid;
    SFTP_Initializer sftpi;
    long rcode;
    RPC2_Options options;

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
    sftpi.PacketSize = 1024;
    sftpi.WindowSize = 16;
    sftpi.SendAhead = 4;
    sftpi.AckPoint = 4;
    SFTP_Activate(&sftpi);

    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    rcode = RPC2_Init(RPC2_VERSION, &options, NULL, -1, 0);
    if (rcode != RPC2_SUCCESS) {
	LogMsg(0, SrvDebugLevel, stdout, "RPC2_Init failed with %s\n", RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}

