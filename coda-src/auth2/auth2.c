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


/*
auth2.c -- authentication server for ViceII.

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include <prs.h>
#include <al.h>
#include "auth2.h"
#include "auth2.common.h"
#ifdef CODAAUTH
#include "pwdefines.h"
#include "pwsupport.h"
#endif	/* CODAAUTH */

#include <codaconf.h>
#include <vice_file.h>

static char *serverconf = SYSCONFDIR "/server"; /* ".conf" */
static char *vicedir = NULL;

extern int AL_DebugLevel;


static void InitGlobals(int argc, char **argv);
static void InitLog();
static void InitSignals();
static void ResetDebug();
static void SetDebug();
static void CheckSignal();
static void Terminate();
static void InitRPC();
static void HandleRPCError(int rCode, RPC2_Handle connId);
static void InitAl();
static void CheckTokenKey();
long GetKeys(RPC2_Integer *AuthenticationType, RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);	/* multiplex to other functions */

void LogFailures(RPC2_Integer AuthenticationType, RPC2_CountedBS *cIdent, RPC2_Integer eType, RPC2_HostIdent *pHost, RPC2_PortIdent *pPort);	/* to log authentication failures */

int GetViceId(RPC2_CountedBS *cIdent);	/* must be post-name conversion */

RPC2_EncryptionKey TokenKey;	/* Used for encrypting server tokens;
				    modified by SetKeys() routine; changed periodically  */
int TokenTime = 0;	/* last modified time on TokenKey file	*/
int AuthTime = 0;	/* last modified time for PDB		*/
static char *Auth2TKFile = NULL;	/* name of token key file */
static int AUTime = 0;			/* used to tell if binaries have changed */

#ifdef KERBEROS5
static char *krb5_keytab_file = NULL;
#endif  /* KERBEROS5 */
#if defined(KERBEROS4) || defined(KERBEROS5)
static char *krb_realm = NULL;
#endif

#ifdef HAVE_NDBM
#define CODADB vice_sharedfile("db/prot_users.dir")
#else
#define CODADB vice_sharedfile("db/prot_users.db")
#endif

static int CheckOnly = 0;	/* only allow password checking at this server */
static int DoRedirectLog = 1;	/* set to zero by -r switch on command line */

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


int main(int argc, char **argv)
{
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    register int rc;
    struct stat buff;
    char errmsg[MAXPATHLEN];

    ReadConfigFile();

    Auth2TKFile = strdup(vice_sharedfile("db/auth2.tk"));
    rc = chdir(vice_sharedfile("auth2"));
    if ( rc ) {
	    snprintf (errmsg, MAXPATHLEN, "Error: cannot chdir to %s",
		      vice_sharedfile("auth2"));
	    perror(errmsg);
	    exit(1);
    }
    InitGlobals(argc, argv);
    if ( ! AuthDebugLevel ) 
	    UtilDetach();
    InitLog();
    InitSignals();
    InitRPC();
    InitAl();

#ifdef CODAAUTH
    InitPW(PWFIRSTTIME);
#endif	/* CODAAUTH */

#ifdef KERBEROS4
    Krb4Init(krb_realm);
#endif

#ifdef KERBEROS5
    Krb5Init(krb_realm, krb5_keytab_file);
#endif
    
    LogMsg(-1, 0, stdout, "Server successfully started\n");

    while(TRUE) {
	cid = 0;
	rc = RPC2_GetRequest(NULL, &cid, &reqbuffer, NULL,
				(long (*)())GetKeys, RPC2_XOR,
				(long (*)())LogFailures);
	if (rc < RPC2_WLIMIT) {
		HandleRPCError(rc, cid);
		continue;
	}

	if(stat(CODADB, &buff)) {
	    printf("stat for prot_users.db failed\n");
	    fflush(stdout);
	} else {
	    if(AuthTime != buff.st_mtime)
		InitAl();
	}
	if ((rc = auth2_ExecuteRequest(cid, reqbuffer, (SE_Descriptor *)0)) 
	    < RPC2_WLIMIT)
		HandleRPCError(rc, cid);
    }
    return(0);
}


static void InitGlobals(int argc, char **argv)
    /* Set globals from command line args */
{
    register int i;
    int	len;
    struct stat buff;

    len = strlen(argv[0]);
    for (i=0; i<len; i++)
	*(argv[0]+i) = ' ';
    strcpy(argv[0], "auth2");
    for (i = 1; i < argc; i++) {
	if (argv[i] == 0) continue;	/* ignore NULL parms; this allows
				    arguments to be passed in via authmon */
	if (strcmp(argv[i], "-x") == 0 && i < argc - 1)
	    {
	    AuthDebugLevel = atoi(argv[++i]);
	    RPC2_DebugLevel = AL_DebugLevel = AuthDebugLevel/10;
	    continue;
	    }
	if (strcmp(argv[i], "-r") == 0)
	    {
	    DoRedirectLog = 0;
	    continue;
	    }

	if (strcmp(argv[i], "-chk") == 0)
	    {
	    CheckOnly = 1;
	    continue;
	    }

#ifdef CODAAUTH
	if (strcmp(argv[i], "-p") == 0 && i < argc - 1)
	    {
	    PWFile = argv[++i];
	    continue;
	    }
#endif	/* PWDCODADB */

#ifdef KERBEROS5
	if (strcmp(argv[i], "-keytab") == 0 && i < argc - 1)
	    {
	    krb5_keytab_file = argv[++i];
	    continue;
	    }
#endif  /* KERBEROS5 */

#if defined(KERBEROS4) || defined(KERBEROS5)
	if (strcmp(argv[i], "-realm") == 0 && i < argc - 1)
	    {
	    krb_realm = argv[++i];
	    continue;
	    }
#endif

	if (strcmp(argv[i], "-tk") == 0 && i < argc - 1)
	    {
	    Auth2TKFile = argv[++i];
	    continue;
	    }
	    
#ifdef CODAAUTH
	if (strcmp(argv[i], "-fk") == 0 && i < argc - 1)
	    {
	    strncpy((char *)FileKey, argv[++i], RPC2_KEYSIZE);
				/* key to be used in decrypting
                                   passwords in PWFile */
	    continue;
	    }
#endif	/* CODAAUTH */

	fprintf(stderr, "Usage: auth2 [-r] [-chk] [-x debuglevel] ");

#ifdef CODAAUTH
	fprintf(stderr, "[-p pwfile] [-fk filekey] ");
#endif	/* CODAAUTH */

	fprintf(stderr, "[-tk tokenkey] ");

	fprintf(stderr, "\n");

	exit(-1);
    }    

    CheckTokenKey();

    AUTime = buff.st_mtime;
}


static void InitLog()
    {
    if (DoRedirectLog)
	{
	freopen("AuthLog","a+",stdout);
	freopen("AuthLog","a+",stderr);
	}

    printf("Starting  Auth Server......\n");
    fflush(stdout);
    }


static void InitSignals()
    {
    FILE *file;
    (void) signal(SIGHUP, (void (*)(int))ResetDebug);
    (void) signal(SIGUSR1, (void (*)(int))SetDebug);
#ifndef __CYGWIN32__
    (void) signal(SIGXCPU, (void (*)(int))CheckSignal);
#endif
    (void) signal(SIGTERM, (void (*)(int))Terminate);

    if ((file = fopen("pid","w")) == NULL)
	{
	perror("pid");
	exit(-1);
	}
    fprintf(file,"%d",(int) getpid());
    fclose(file);
    }


static void ResetDebug()
    {
    AuthDebugLevel = RPC2_DebugLevel = AL_DebugLevel = 0;
    LogMsg(-1, 0, stdout, "Debug levels reset to 0");
    }


static void SetDebug()
    {
    if (AuthDebugLevel == 0) AuthDebugLevel = 1;
    else AuthDebugLevel *= 5;
    
    RPC2_DebugLevel = AL_DebugLevel = AuthDebugLevel/10;
    LogMsg(-1, 0, stdout, "Debug levels reset:  Auth = %d    RPC2 = %d    AL = %d", AuthDebugLevel, RPC2_DebugLevel, AL_DebugLevel);
    }


static void Terminate()
    {
    LogMsg(-1, 0, stdout, "Terminate signal received .......quitting");
    exit(0);
    }


static void CheckSignal()
    {
    LogMsg(-1, 0, stdout, "Check signal received ...... ignored");
    }


static void InitRPC()
    {
    PROCESS mylpid;
    RPC2_Integer rc;
    RPC2_PortIdent port;
    RPC2_SubsysIdent subsysid;
    struct timeval tout;

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
    
    tout.tv_sec = 15;
    tout.tv_usec = 0;

    port.Tag = RPC2_PORTBYNAME;
    strcpy(port.Value.Name, AUTH_SERVICE);
    if ((rc = RPC2_Init(RPC2_VERSION, 0, &port, -1, &tout)) != RPC2_SUCCESS) {
	LogMsg(-1, 0, stdout, "RPC2_Init failed with %s", RPC2_ErrorMsg(rc));
	exit(-1);
    }
    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = AUTH_SUBSYSID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    }


static void HandleRPCError(int rCode, RPC2_Handle connId)
    {
    fprintf(stderr, "auth2: %s", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0) RPC2_Unbind(connId);
    }


static void InitAl()
{
    RPC2_Integer rc;
    struct stat buff;

    if ((rc = AL_Initialize(AL_VERSION)) != 0) {
	LogMsg(-1, 0, stdout, "AL_Initialize failed with %d", rc);
	exit(-1);
    }
    CODA_ASSERT(stat(CODADB, &buff) == 0);
    AuthTime = buff.st_mtime;
}


static void CheckTokenKey()
    {
    struct stat statbuf;
    FILE *tf;

    if(stat(Auth2TKFile, &statbuf))
	{
	perror("stat failed for token key file");
	exit(-1);
	}
    if(TokenTime != statbuf.st_mtime) 
	{
	if ((tf = fopen(Auth2TKFile, "r")) == NULL)
	    {
	    perror(Auth2TKFile);
	    exit(-1);
	    }
	memset(TokenKey, 0, RPC2_KEYSIZE);
	fread(TokenKey, 1, RPC2_KEYSIZE, tf);
	TokenTime = statbuf.st_mtime;
	fclose(tf);
	}
    }

long GetKeys(RPC2_Integer *AuthenticationType, RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
	int returnval, vid;

	switch (*AuthenticationType)
	{
		case	AUTH_METHOD_NULL:
			/* we don't like this */
				return -1;

#ifdef CODAAUTH
		case	AUTH_METHOD_CODAUSERNAME:
			/* use coda password database */
				returnval = PWGetKeys(cIdent, hKey, sKey);
				break;
#endif	/* CODAAUTH */

		case	AUTH_METHOD_CODATOKENS:
			/* cannot retrieve tokens using tokens! */
				return -1;

		case	AUTH_METHOD_PK:
			/* just a reserved constant, thanks */
				return -1;

#ifdef KERBEROS4
		case	AUTH_METHOD_KERBEROS4:
				returnval = Krb4GetKeys(cIdent, hKey, sKey);
				break;
#endif	/* KERBEROS4 */

#ifdef KERBEROS5
		case	AUTH_METHOD_KERBEROS5:
				returnval = Krb5GetKeys(cIdent, hKey, sKey);
				break;
#endif

		default:
			/* unrecognized auth type */
				return -1;
	}

	if (returnval == 0)
	{
		
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


void LogFailures(RPC2_Integer AuthenticationType, RPC2_CountedBS *cIdent, RPC2_Integer eType, RPC2_HostIdent *pHost, RPC2_PortIdent *pPort)
    {
    CODA_ASSERT(pHost->Tag == RPC2_HOSTBYINETADDR);
    *(cIdent->SeqBody+cIdent->SeqLen-1) = 0;  /* just to make sure */
    LogMsg(-1, 0, stdout, "Authentication failed for \"%s\" from %s",
	 cIdent->SeqBody, inet_ntoa(pHost->Value.InetAddress));
    }


/* ============= Bodies of RPC routines follow =============  */

long S_AuthNewConn(RPC2_Handle cid, RPC2_Integer seType, RPC2_Integer secLevel, RPC2_Integer encType, RPC2_Integer AuthType, RPC2_CountedBS *cIdent)
    {
    struct UserInfo *p;
    int vid;

    vid = GetViceId(cIdent);
    LogMsg(0, AuthDebugLevel, stdout, "AuthNewConn(0x%x, %d, %d, %d, %d)",
	cid, seType, secLevel, encType, vid);
    p = (struct UserInfo *) malloc(sizeof(struct UserInfo));
    RPC2_SetPrivatePointer(cid, (char *)p);
    p->ViceId = vid;
    p->HasQuit = FALSE;
    p->UserCPS = NULL;
    p->LastUsed = time(0);
    return(0);
    }


long S_AuthQuit(RPC2_Handle cid)
    {
    struct UserInfo *p;
    RPC2_GetPrivatePointer(cid, (char **)&p);
    p->HasQuit = TRUE;
    return(0);
    }

extern void hton_SecretToken(SecretToken *);
long S_AuthGetTokens(RPC2_Handle cid, EncryptedSecretToken est, ClearToken *cToken)
    {
    int i;
    struct UserInfo *ui;
    SecretToken sToken;

    RPC2_GetPrivatePointer(cid, (char **)&ui);
    if (ui == NULL || ui->HasQuit == TRUE) return(AUTH_FAILED);
    ui->LastUsed = time(0);

    /* First build clear token */
    cToken->AuthHandle = -1;	/* not in use right now */
    for (i = 0; i < RPC2_KEYSIZE; i++)
	cToken->HandShakeKey[i] = rpc2_NextRandom(NULL) & 0xff;
    cToken->BeginTimestamp = 0;
    cToken->EndTimestamp = time(0) + 60*60*25; /* valid for 25 hours */
    cToken->ViceId = ui->ViceId;
    
    /* Then build secret token */
    memset(sToken.MagicString, '\0', sizeof(AuthMagic));
    strncpy((char *)sToken.MagicString, (char *)AUTH_MAGICVALUE, sizeof(AuthMagic));
    sToken.AuthHandle = cToken->AuthHandle;
    sToken.Noise1 = rpc2_NextRandom(NULL);
    sToken.ViceId = cToken->ViceId;
    sToken.BeginTimestamp = cToken->BeginTimestamp;
    sToken.Noise2 = rpc2_NextRandom(NULL);
    sToken.EndTimestamp = cToken->EndTimestamp;
    sToken.Noise3 = rpc2_NextRandom(NULL);
    bcopy(cToken->HandShakeKey, sToken.HandShakeKey, RPC2_KEYSIZE);
    sToken.Noise4 = rpc2_NextRandom(NULL);
    hton_SecretToken(&sToken);
    CheckTokenKey();
    rpc2_Encrypt((char *)&sToken, (char *)est, sizeof(SecretToken), (char *)TokenKey, RPC2_XOR);

    return(0);
    }


long S_AuthChangePasswd (RPC2_Handle cid, RPC2_Integer viceId, 
			 RPC2_String Passwd)
{

    int i;
    RPC2_EncryptionKey newPasswd;
    int len;

    bzero(newPasswd, sizeof(newPasswd));
    if (strlen(Passwd) < RPC2_KEYSIZE) 
	    len = strlen(Passwd);
    else
	    len = RPC2_KEYSIZE;
    bcopy(Passwd, newPasswd, len); 

    if (AuthDebugLevel)
	{
	printf("AuthChangePasswd(%ld, \"", viceId);
	for (i = 0; i < RPC2_KEYSIZE; i++)
	    printf("%c", newPasswd[i]);
	printf("\")");
	}

    /* Do not allow if this is a read only server	*/
    if(CheckOnly)
	return(AUTH_READONLY);

#ifdef CODAAUTH
	return PWChangePasswd(cid, viceId, Passwd);
#else	/* CODAAUTH */
	return (AUTH_FAILED);
#endif	/* !CODAAUTH */

}


long S_AuthNewUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey initKey, RPC2_String otherInfo)
{

#ifdef CODAAUTH
        return PWNewUser(cid, viceId, initKey, otherInfo);
#else   /* CODAAUTH */
        return (AUTH_FAILED);
#endif  /* !CODAAUTH */

}

extern int IsAdministrator(struct UserInfo *pU);
extern char *GetVname(int id, char *s);
extern int IsAUser(int viceId);

long S_AuthDeleteUser(RPC2_Handle cid, RPC2_Integer viceId)
{
	struct UserInfo *p;

	/* Do not allow if this is a read only server	*/
	if(CheckOnly)
		return(AUTH_READONLY);
	
	/* make sure it's a system administrator */
	RPC2_GetPrivatePointer(cid, (char **)&p);
	if (p == NULL || p->HasQuit == TRUE) 
		return(AUTH_FAILED);
	p->LastUsed = time(0);
	if (!IsAdministrator(p)) {
		char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
		LogMsg(-1, 0, stdout, "AuthDeleteUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
		return(AUTH_DENIED);
	}
	if (!IsAUser(viceId)) 
		return(AUTH_FAILED);
	
    /* make the change */
#ifdef CODAAUTH
        return PWDeleteUser(cid, viceId);
#else   /* CODAAUTH */
        return (AUTH_FAILED);
#endif  /* !CODAAUTH */

}

long S_AuthChangeUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey newKey, RPC2_String otherInfo)
{

#ifdef CODAAUTH
        return PWChangeUser(cid, viceId, newKey, otherInfo);
#else   /* CODAAUTH */
        return (AUTH_FAILED);
#endif  /* !CODAAUTH */

}

long S_AuthNameToId(RPC2_Handle cid, RPC2_String sUser, RPC2_Integer *sId)
    {
    *sId = -1;
    if (AL_NameToId((char *)sUser, (int *)sId) < 0)
	return(AUTH_FAILED);
    else return(AUTH_SUCCESS);
    }
    

int GetViceId(RPC2_CountedBS *cIdent)
    /* Interprets cIdent as a user id string or user name string and returns
	the Vice Id corr to this client.  Returns -1 if a non-existent
	client is specified.
    */
    {
    int vid;

    if (cIdent->SeqLen <= 0) return(-1);
    *(cIdent->SeqBody+cIdent->SeqLen-1) = 0;  /* just to make sure */
    if (isdigit(*cIdent->SeqBody)) return(atoi((char *)cIdent->SeqBody));
    else 
	if (AL_NameToId((char *)cIdent->SeqBody, &vid) < 0)
	    return(-1);
	else return(vid);
    }

    
