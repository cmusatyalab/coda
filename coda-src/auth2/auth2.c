/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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

/*
auth2.c -- authentication server for ViceII.

*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/signal.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/rpc2_addrinfo.h>

#ifdef __FreeBSD__
#include <limits.h>
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <prs.h>
#include <al.h>
#include "auth2.h"
#include "auth2.common.h"

#include "pwdefines.h"
#include "pwsupport.h"

#include <codaconf.h>
#include <vice_file.h>
#include <coda_getservbyname.h>
#include "codatoken.h"

#define MAXNUMCLIENT 10

static const char *vicedir = NULL;

extern int AL_DebugLevel;

static void InitGlobals(int argc, char **argv);
static void ReopenLog();
static void InitSignals();
static void ResetDebug();
static void SetDebug();
static void CheckSignal();
static void Terminate();
static void InitRPC();
static void HandleRPCError(int rCode, RPC2_Handle connId);
static void CheckTokenKey();
long GetKeys(RPC2_Integer *AuthenticationType, RPC2_CountedBS *cIdent,
             RPC2_EncryptionKey hKey,
             RPC2_EncryptionKey sKey); /* multiplex to other functions */

long LogFailures(RPC2_Integer AuthenticationType, RPC2_CountedBS *cIdent,
                 RPC2_Integer eType, RPC2_HostIdent *pHost,
                 RPC2_PortIdent *pPort); /* to log authentication failures */

int GetViceId(RPC2_CountedBS *cIdent); /* must be post-name conversion */

static uint8_t auth2key[AUTH2KEYSIZE];
static char *Auth2TKFile = NULL; /* name of token key file */

static int CheckOnly     = 0; /* only allow password checking at this server */
static int DoRedirectLog = 1; /* set to zero by -r switch on command line */

struct UserInfo *client[MAXNUMCLIENT];
int client_idx;

void ReadConfigFile(void)
{
    /* Load configuration file to get vice dir. */
    codaconf_init("server.conf");

    CODACONF_STR(vicedir, "vicedir", "/vice");

    vice_dir_init(vicedir);
}

int main(int argc, char **argv)
{
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    int rc;
    char errmsg[MAXPATHLEN];

    ReadConfigFile();

    Auth2TKFile = strdup(vice_config_path("db/auth2.tk"));
    rc          = chdir(vice_config_path("auth2"));
    if (rc) {
        snprintf(errmsg, MAXPATHLEN, "Error: cannot chdir to %s",
                 vice_config_path("auth2"));
        perror(errmsg);
        exit(EXIT_FAILURE);
    }
    InitGlobals(argc, argv);
    if (!AuthDebugLevel)
        UtilDetach();

    ReopenLog();

    printf("Starting  Auth Server......\n");
    fflush(stdout);

    InitSignals();
    InitRPC();

    if ((rc = AL_Initialize(AL_VERSION)) != 0) {
        LogMsg(-1, 0, stdout, "AL_Initialize failed with %d", rc);
        exit(EXIT_FAILURE);
    }

    InitPW(PWFIRSTTIME);

    LogMsg(-1, 0, stdout, "Server successfully started");

    client_idx = 0;
    memset(client, 0, sizeof(client));

    while (TRUE) {
        cid = 0;
        rc  = RPC2_GetRequest(NULL, &cid, &reqbuffer, NULL, GetKeys, RPC2_XOR,
                              LogFailures);
        if (rc < RPC2_WLIMIT) {
            HandleRPCError(rc, cid);
            continue;
        }

        { /* drop unauthenticated requests on the floor */
            RPC2_PeerInfo peer;
            RPC2_GetPeerInfo(cid, &peer);
            CODA_ASSERT(peer.RemoteHost.Tag == RPC2_HOSTBYADDRINFO);
            RPC2_freeaddrinfo(peer.RemoteHost.Value.AddrInfo);

            if (peer.SecurityLevel == RPC2_OPENKIMONO) {
                LogMsg(0, SrvDebugLevel, stdout,
                       "Receiving unauthenticated request %d, "
                       "update rpc2 library!",
                       reqbuffer->Header.Opcode);
                RPC2_Unbind(cid);
                continue;
            }
        }

        if ((rc = auth2_ExecuteRequest(cid, reqbuffer, (SE_Descriptor *)0)) <
            RPC2_WLIMIT)
            HandleRPCError(rc, cid);
    }
    return (0);
}

static int readoneline(const char *file, char *buf, size_t size)
{
    FILE *f;
    size_t len;

    /* read first line from file
     * fail if the file cannot be opened or is empty */
    f = fopen(file, "r");
    if (f == NULL || fgets(buf, size, f) == NULL) {
        return -1;
    }
    fclose(f);

    /* strip trailing newline */
    len = strlen(buf);
    if (buf[len] == '\n')
        buf[len] = '\0';

    return 0;
}

/* check if the contents of /vice/db/scm matches /vice/hostname */
static int IsSCM(void)
{
    const char *path;
    char scm[HOST_NAME_MAX + 1], host[HOST_NAME_MAX + 1];

    path = vice_config_path("db/scm");
    if (readoneline(path, scm, sizeof(scm))) {
        fprintf(stderr, "Unable to read '%s'\n", path);
        exit(EXIT_FAILURE);
    }

    path = vice_config_path("hostname");
    if (readoneline(path, host, sizeof(host))) {
        fprintf(stderr, "Unable to read '%s'\n", path);
        exit(EXIT_FAILURE);
    }

    return strcmp(scm, host) == 0;
}

static void InitGlobals(int argc, char **argv)
/* Set globals from command line args */
{
    int i;
    int len;

    len = strlen(argv[0]);
    for (i = 0; i < len; i++)
        *(argv[0] + i) = ' ';
    strcpy(argv[0], "auth2");
    for (i = 1; i < argc; i++) {
        if (argv[i] == 0)
            continue; /* ignore NULL parms; this allows
				    arguments to be passed in via authmon */
        if (strcmp(argv[i], "-x") == 0 && i < argc - 1) {
            AuthDebugLevel  = atoi(argv[++i]);
            RPC2_DebugLevel = AL_DebugLevel = AuthDebugLevel / 10;
            continue;
        }
        if (strcmp(argv[i], "-r") == 0) {
            DoRedirectLog = 0;
            continue;
        }

        if (strcmp(argv[i], "-chk") == 0) {
            CheckOnly = 1;
            continue;
        }

        if (strcmp(argv[i], "-p") == 0 && i < argc - 1) {
            PWFile = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-tk") == 0 && i < argc - 1) {
            Auth2TKFile = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-fk") == 0 && i < argc - 1) {
            strncpy((char *)FileKey, argv[++i], RPC2_KEYSIZE);
            /* key to be used in decrypting
                                   passwords in PWFile */
            continue;
        }

        fprintf(stderr, "Usage: auth2 [-r] [-chk] [-x debuglevel] ");
        fprintf(stderr, "[-p pwfile] [-fk filekey] [-tk tokenkey]\n");

        exit(EXIT_FAILURE);
    }

    if (!CheckOnly && !IsSCM()) {
        fprintf(stderr,
                "Looks like we are not SCM, switching to read-only mode.\n");
        CheckOnly = 1;
    }

    CheckTokenKey();
}

static void ReopenLog(void)
{
    if (DoRedirectLog) {
        freopen("AuthLog", "a+", stdout);
        freopen("AuthLog", "a+", stderr);
    }
}

static void InitSignals(void)
{
    FILE *file;
    (void)signal(SIGHUP, (void (*)(int))ReopenLog);
    (void)signal(SIGUSR1, (void (*)(int))SetDebug);
    (void)signal(SIGUSR2, (void (*)(int))ResetDebug);
#ifndef __CYGWIN32__
    (void)signal(SIGXCPU, (void (*)(int))CheckSignal);
#endif
    (void)signal(SIGTERM, (void (*)(int))Terminate);

    if ((file = fopen("pid", "w")) == NULL) {
        perror("pid");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%d", (int)getpid());
    fclose(file);
}

static void ResetDebug(void)
{
    AuthDebugLevel = RPC2_DebugLevel = AL_DebugLevel = 0;
    LogMsg(-1, 0, stdout, "Debug levels reset to 0");
}

static void SetDebug(void)
{
    if (AuthDebugLevel == 0)
        AuthDebugLevel = 1;
    else
        AuthDebugLevel *= 5;

    RPC2_DebugLevel = AL_DebugLevel = AuthDebugLevel / 10;
    LogMsg(-1, 0, stdout,
           "Debug levels reset:  Auth = %d    RPC2 = %d    AL = %d",
           AuthDebugLevel, RPC2_DebugLevel, AL_DebugLevel);
}

static void Terminate(void)
{
    int i;
    struct UserInfo *ui;

    for (i = 0; i < MAXNUMCLIENT; i++) {
        if (!client[i])
            continue;

        ui        = client[i];
        client[i] = NULL;

        RPC2_Unbind(ui->handle);
        if (ui->UserCPS)
            AL_FreeCPS(&ui->UserCPS);
        free(ui);
    }

    LogMsg(-1, 0, stdout, "Terminate signal received .......quitting");
    exit(EXIT_SUCCESS);
}

static void CheckSignal(void)
{
    LogMsg(-1, 0, stdout, "Check signal received ...... ignored");
}

static void InitRPC(void)
{
    PROCESS mylpid;
    RPC2_Integer rc;
    RPC2_PortIdent port;
    RPC2_Options options;
    RPC2_SubsysIdent subsysid;
    struct timeval tout;

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY - 1, &mylpid) ==
                LWP_SUCCESS);

    tout.tv_sec  = 15;
    tout.tv_usec = 0;

    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    struct servent *s         = coda_getservbyname(AUTH_SERVICE, "udp");
    port.Tag                  = RPC2_PORTBYINETNUMBER;
    port.Value.InetPortNumber = s->s_port;

    if ((rc = RPC2_Init(RPC2_VERSION, &options, &port, -1, &tout)) !=
        RPC2_SUCCESS) {
        LogMsg(-1, 0, stdout, "RPC2_Init failed with %s", RPC2_ErrorMsg(rc));
        exit(EXIT_FAILURE);
    }
    subsysid.Tag            = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = AUTH_SUBSYSID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
}

static void HandleRPCError(int rCode, RPC2_Handle connId)
{
    fprintf(stderr, "auth2: %s", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0)
        RPC2_Unbind(connId);
}

static void CheckTokenKey(void)
{
    struct stat statbuf;
    FILE *tf;
    RPC2_EncryptionKey TokenKey;
    static int TokenTime = 0; /* last modified time on token file */

    if (stat(Auth2TKFile, &statbuf)) {
        perror("stat failed for token key file");
        exit(EXIT_FAILURE);
    }
    if (TokenTime != statbuf.st_mtime) {
        if ((tf = fopen(Auth2TKFile, "r")) == NULL) {
            perror(Auth2TKFile);
            exit(EXIT_FAILURE);
        }
        memset(TokenKey, 0, RPC2_KEYSIZE);
        fread(TokenKey, 1, RPC2_KEYSIZE, tf);
        TokenTime = statbuf.st_mtime;
        fclose(tf);
        getauth2key(TokenKey, RPC2_KEYSIZE, auth2key);
    }
}

long GetKeys(RPC2_Integer *AuthenticationType, RPC2_CountedBS *cIdent,
             RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
    int returnval, vid;

    /* reject OPENKIMONO bindings */
    if (!cIdent)
        return -1;

    switch (*AuthenticationType) {
    case AUTH_METHOD_NULL:
    case AUTH_METHOD_CODATOKENS:
    case AUTH_METHOD_KERBEROS4:
    case AUTH_METHOD_KERBEROS5:
    case AUTH_METHOD_PK:
        /* we don't like this */
        return -1;
    case AUTH_METHOD_CODAUSERNAME:
        /* use coda password database */
        returnval = PWGetKeys(cIdent, hKey, sKey);
        break;
    default:
        /* unrecognized auth type */
        return -1;
    }

    if (returnval == 0) {
        /* verify has a VID */
        vid = GetViceId(cIdent);
        LogMsg(0, AuthDebugLevel, stdout, "\tvid = %d", vid);
        if (vid < 0)
            returnval = -1;

        /* If we wanted to check if it was a deleted user or such, we
		   would do that here.  That, however, relies on the pwsupport
		   library, so is not enabled. */
    }

    return returnval;
}

long LogFailures(RPC2_Integer AuthenticationType, RPC2_CountedBS *cIdent,
                 RPC2_Integer eType, RPC2_HostIdent *pHost,
                 RPC2_PortIdent *pPort)
{
    char name[64], host[RPC2_ADDRSTRLEN];
    int i, len;

    len = cIdent->SeqLen < 64 ? cIdent->SeqLen : 64;
    for (i = 0; i < len - 1; i++)
        name[i] = isprint(cIdent->SeqBody[i]) ? cIdent->SeqBody[i] : '?';
    for (; i < 64; i++)
        name[i] = '\0';

    CODA_ASSERT(pHost->Tag == RPC2_HOSTBYADDRINFO);
    RPC2_formataddrinfo(pHost->Value.AddrInfo, host, RPC2_ADDRSTRLEN);

    LogMsg(-1, 0, stdout, "Authentication failed for \"%s\" from %s", name,
           host);
    return 0;
}

/* ============= Bodies of RPC routines follow =============  */

long S_AuthNewConn(RPC2_Handle cid, RPC2_Integer seType, RPC2_Integer secLevel,
                   RPC2_Integer encType, RPC2_Integer AuthType,
                   RPC2_CountedBS *cIdent)
{
    int vid;
    struct UserInfo *ui;

    vid = GetViceId(cIdent);
    LogMsg(0, AuthDebugLevel, stdout, "AuthNewConn(0x%x, %d, %d, %d, %d)", cid,
           seType, secLevel, encType, vid);

    if (client[client_idx]) {
        ui                 = client[client_idx];
        client[client_idx] = NULL;

        RPC2_Unbind(ui->handle);
        if (ui->UserCPS)
            AL_FreeCPS(&ui->UserCPS);
        free(ui);
    }

    ui           = (struct UserInfo *)malloc(sizeof(*ui));
    ui->handle   = cid;
    ui->ViceId   = vid;
    ui->HasQuit  = FALSE;
    ui->UserCPS  = NULL;
    ui->LastUsed = time(NULL);

    RPC2_SetPrivatePointer(cid, (char *)ui);
    client[client_idx++] = ui;

    if (client_idx == MAXNUMCLIENT)
        client_idx = 0;

    return (0);
}

long S_AuthQuit(RPC2_Handle cid)
{
    struct UserInfo *ui;
    RPC2_GetPrivatePointer(cid, (void *)&ui);
    if (ui)
        ui->HasQuit = TRUE;
    return (0);
}

long S_AuthGetTokens(RPC2_Handle cid, EncryptedSecretToken est,
                     ClearToken *cToken)
{
    struct UserInfo *ui;
    uint32_t lifetime;

    RPC2_GetPrivatePointer(cid, (void *)&ui);
    if (!ui || ui->HasQuit == TRUE)
        return (AUTH_FAILED);
    ui->LastUsed = time(NULL);

    CheckTokenKey();

    lifetime = 60 * 60 * 25; /* 25 hours */
    generate_CodaToken(auth2key, ui->ViceId, lifetime, cToken, est);

    return (0);
}

long S_AuthChangePasswd(RPC2_Handle cid, RPC2_Integer viceId,
                        RPC2_String Passwd)
{
    int i;
    RPC2_EncryptionKey newPasswd;
    int len;

    memset(newPasswd, 0, sizeof(newPasswd));
    len = strlen((char *)Passwd);
    if (len > RPC2_KEYSIZE)
        len = RPC2_KEYSIZE;
    memcpy(newPasswd, Passwd, len);

    if (AuthDebugLevel) {
        printf("AuthChangePasswd(%d, \"", viceId);
        for (i = 0; i < RPC2_KEYSIZE; i++)
            printf("%c", newPasswd[i]);
        printf("\")");
    }

    /* Do not allow if this is a read only server	*/
    if (CheckOnly)
        return (AUTH_READONLY);

    return PWChangePasswd(cid, viceId, Passwd);
}

long S_AuthNewUser(RPC2_Handle cid, RPC2_Integer viceId,
                   RPC2_EncryptionKey initKey, RPC2_String otherInfo)
{
    return PWNewUser(cid, viceId, initKey, otherInfo);
}

extern int IsAdministrator(struct UserInfo *pU);
extern char *GetVname(int id, char *s);
extern int IsAUser(int viceId);

long S_AuthDeleteUser(RPC2_Handle cid, RPC2_Integer viceId)
{
    struct UserInfo *ui;

    /* Do not allow if this is a read only server	*/
    if (CheckOnly)
        return (AUTH_READONLY);

    /* make sure it's a system administrator */
    RPC2_GetPrivatePointer(cid, (void *)&ui);
    if (!ui || ui->HasQuit == TRUE)
        return (AUTH_FAILED);
    ui->LastUsed = time(NULL);
    if (!IsAdministrator(ui)) {
        char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
        LogMsg(-1, 0, stdout, "AuthDeleteUser() attempt on  %s by %s denied",
               GetVname(viceId, buf1), GetVname(ui->ViceId, buf2));
        return (AUTH_DENIED);
    }
    if (!IsAUser(viceId))
        return (AUTH_FAILED);

    /* make the change */
    return PWDeleteUser(cid, viceId);
}

long S_AuthChangeUser(RPC2_Handle cid, RPC2_Integer viceId,
                      RPC2_EncryptionKey newKey, RPC2_String otherInfo)
{
    return PWChangeUser(cid, viceId, newKey, otherInfo);
}

long S_AuthNameToId(RPC2_Handle cid, RPC2_String sUser, RPC2_Integer *sId)
{
    *sId = -1;
    if (AL_NameToId((char *)sUser, (int *)sId) < 0)
        return (AUTH_FAILED);
    else
        return (AUTH_SUCCESS);
}

int GetViceId(RPC2_CountedBS *cIdent)
/* Interprets cIdent as a user id string or user name string and returns
	the Vice Id corr to this client.  Returns -1 if a non-existent
	client is specified.
    */
{
    int vid;

    if (cIdent->SeqLen <= 0)
        return (-1);
    *(cIdent->SeqBody + cIdent->SeqLen - 1) = 0; /* just to make sure */
    if (isdigit(*cIdent->SeqBody))
        return (atoi((char *)cIdent->SeqBody));
    else if (AL_NameToId((char *)cIdent->SeqBody, &vid) < 0)
        return (-1);
    else
        return (vid);
}
