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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/auth2/auth2.cc,v 4.4 1998/01/10 18:36:59 braam Exp $";
#endif /*_BLURB_*/





#/*
#
#                         IBM COPYRIGHT NOTICE
#
#                          Copyright (C) 1986
#             International Business Machines Corporation
#                         All Rights Reserved
#
#This  file  contains  some  code identical to or derived from the 1986
#version of the Andrew File System ("AFS"), which is owned by  the  IBM
#Corporation.    This  code is provded "AS IS" and IBM does not warrant
#that it is free of infringement of  any  intellectual  rights  of  any
#third  party.    IBM  disclaims  liability of any kind for any damages
#whatsoever resulting directly or indirectly from use of this  software
#or  of  any  derivative work.  Carnegie Mellon University has obtained
#permission to distribute this code, which is based on Version 2 of AFS
#and  does  not  contain the features and enhancements that are part of
#Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
#supported by Transarc Corporation, Pittsburgh, PA.
#
#*/


/*
auth2.c -- authentication server for ViceII.

*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <ctype.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include <prs.h>
#include <al.h>
#include "auth2.h"

extern int AL_DebugLevel;


PRIVATE void InitGlobals(int argc, char **argv);
PRIVATE void InitLog();
PRIVATE void InitSignals();
PRIVATE void ResetDebug();
PRIVATE void SetDebug();
PRIVATE void CheckSignal();
PRIVATE void Terminate();
PRIVATE void InitRPC();
PRIVATE void HandleRPCError(int rCode, RPC2_Handle connId);
PRIVATE void InitAl();
PRIVATE void CheckTokenKey();
PRIVATE long PWGetKeys(RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey);	/* secure RPC key lookup routine */
PRIVATE void LogFailures(RPC2_CountedBS *cIdent, RPC2_Integer eType, RPC2_HostIdent *pHost, RPC2_PortalIdent *pPortal);	/* to log authentication failures */

PRIVATE void InitPW();
PRIVATE void BuildPWArray(char *fileBuf);
PRIVATE void EnlargePW(int newSize);
PRIVATE void AppendPW(int vId, RPC2_EncryptionKey eKey, char *otherInfo, int agentId);
PRIVATE int LockParent(char *fName, int lockType);
PRIVATE int IsAUser(int viceId);
PRIVATE int IsADeletedUser(int viceId);
PRIVATE int IsAdministrator(struct UserInfo *pU);
PRIVATE int BogusKey(RPC2_EncryptionKey x);
PRIVATE int GetViceId(RPC2_CountedBS *cIdent);
PRIVATE char *GetVname(int id, char *s);

PRIVATE int AdminID;	/* group Id of system administrators */
PRIVATE RPC2_EncryptionKey TokenKey;	/* Used for encrypting server tokens;
				    modified by SetKeys() routine; changed periodically  */
PRIVATE int TokenTime = 0;	/* last modified time on TokenKey file	*/
PRIVATE int AuthTime = 0;	/* last modified time for PDB		*/
/*char DefKey[RPC2_KEYSIZE] = {'\146','\154','\141','\155','\151','\156','\147','\157'}; */
PRIVATE char DefKey[RPC2_KEYSIZE] = {'\144','\162','\163','\145','\165','\163','\163','\040'};
PRIVATE RPC2_EncryptionKey FileKey;	/* unsigned char causes initialization probs */
				/* Key used for encrypting password file; used primarily to
				to prevent accidental revelation of clear passwords to system
				administrators */
PRIVATE RPC2_EncryptionKey NullKey;	/* always full of zeros */
PRIVATE RPC2_EncryptionKey DeleteKey;	/* always full of ones: set in InitAl() */
PRIVATE char *PWFile = "/vice/db/auth2.pw";	/* name of password file */
PRIVATE char *TKFile = "/vice/db/auth2.tk";	/* name of token key file */
PRIVATE RPC2_EncryptionKey *PWArray = NULL;	/* pointer to array of passwords indexed by ViceId */
PRIVATE int AUTime = 0;			/* used to tell if binaries have changed */
PRIVATE int PWTime = 0;			/* used to tell if pw file has changed	*/
PRIVATE int PWLen = 0;	/* no of entries in PWArray */
PRIVATE int PWCount = 0;

#define PDB "/vice/db/vice.pdb"
#define PCF "/vice/db/vice.pcf"

PRIVATE int CheckOnly = 0;	/* only allow password checking at this server */

struct UserInfo	
    {/* per-connection info */
    int ViceId;	/* from NewConnection */
    int HasQuit; /* TRUE iff Quit() was received on this connection */
    PRS_InternalCPS *UserCPS;
    int LastUsed;	/* timestamped at each RPC call; for gc'ing */
    };

PRIVATE int DoRedirectLog = 1;	/* set to zero by -r switch on command line */


int main(int argc, char **argv)
{
    RPC2_PacketBuffer *reqbuffer;
    RPC2_Handle cid;
    register int rc;
    struct stat buff;

    InitGlobals(argc, argv);
    InitLog();
    InitSignals();
    InitRPC();
    InitAl();
    PWLen = 100; /* length of PW array; this is an initial guess, may increase below */
    PWArray = (RPC2_EncryptionKey *)malloc(PWLen*RPC2_KEYSIZE);
    assert(PWArray != NULL);
    InitPW();
    
    LogMsg(-1, 0, stdout, "Server successfully started\n");

    while(TRUE) {
	cid = 0;
	if ((rc = RPC2_GetRequest(NULL, &cid, &reqbuffer, NULL, 
				  (long (*)())PWGetKeys, RPC2_XOR, 
				  (long (*)())LogFailures)) 
	    < RPC2_WLIMIT)
		HandleRPCError(rc, cid);

	if(stat(PDB, &buff)) {
	    printf("stat for vice.pdb failed\n");
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


PRIVATE void InitGlobals(int argc, char **argv)
    /* Set globals from command line args */
{
    register int i;
    int	len;
    struct stat buff;

    len = strlen(argv[0]);
    for (i=0; i<len; i++)
	*(argv[0]+i) = ' ';
    strcpy(argv[0], "auth2");
    bcopy(DefKey, FileKey, RPC2_KEYSIZE);
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

	if (strcmp(argv[i], "-p") == 0 && i < argc - 1)
	    {
	    PWFile = argv[++i];
	    continue;
	    }

	if (strcmp(argv[i], "-tk") == 0 && i < argc - 1)
	    {
	    TKFile = argv[++i];
	    continue;
	    }
	    
	if (strcmp(argv[i], "-fk") == 0 && i < argc - 1)
	    {
	    strncpy((char *)FileKey, argv[++i], RPC2_KEYSIZE);
				/* key to be used in decrypting
                                   passwords in PWFile */
	    continue;
	    }
	/* The following parameter is ignored, it is use by authmon
           and may come */
	/* through on the input parameters */
	if(strcmp(argv[i], "-d") == 0)
	    {
	    i++;
	    continue;
	    }

	printf("Usage: auth2 [-r] [-chk] [-x debuglevel] [-p pwfile] [-tk tokenkey] [-fk filekey]\n");
	exit(-1);
    }    

    CheckTokenKey();
    AUTime = buff.st_mtime;
}


PRIVATE void InitLog()
    {
    if (DoRedirectLog)
	{
	freopen("AuthLog","a+",stdout);
	freopen("AuthLog","a+",stderr);
	}

    printf("Starting  Auth Server......\n");
    fflush(stdout);
    }


PRIVATE void InitSignals()
    {
    FILE *file;
    (void) signal(SIGHUP, (void (*)(int))ResetDebug);
    (void) signal(SIGTSTP, (void (*)(int))SetDebug);
#ifndef __CYGWIN32__
    (void) signal(SIGXCPU, (void (*)(int))CheckSignal);
#endif
    (void) signal(SIGTERM, (void (*)(int))Terminate);

    if ((file = fopen("pid","w")) == NULL)
	{
	perror("pid");
	exit(-1);
	}
    fprintf(file,"%d",getpid());
    fclose(file);
    }


PRIVATE void ResetDebug()
    {
    AuthDebugLevel = RPC2_DebugLevel = AL_DebugLevel = 0;
    LogMsg(-1, 0, stdout, "Debug levels reset to 0");
    }


PRIVATE void SetDebug()
    {
    if (AuthDebugLevel == 0) AuthDebugLevel = 1;
    else AuthDebugLevel *= 5;
    
    RPC2_DebugLevel = AL_DebugLevel = AuthDebugLevel/10;
    LogMsg(-1, 0, stdout, "Debug levels reset:  Auth = %d    RPC2 = %d    AL = %d", AuthDebugLevel, RPC2_DebugLevel, AL_DebugLevel);
    }


PRIVATE void Terminate()
    {
    LogMsg(-1, 0, stdout, "Terminate signal received .......quitting");
    exit(0);
    }


PRIVATE void CheckSignal()
    {
    LogMsg(-1, 0, stdout, "Check signal received ...... ignored");
    }


PRIVATE void InitRPC()
    {
    PROCESS mylpid;
    RPC2_Integer rc;
    RPC2_PortalIdent portalid, *portallist[1];
    RPC2_SubsysIdent subsysid;

    assert(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    portalid.Tag = RPC2_PORTALBYNAME;
    strcpy(portalid.Value.Name, AUTH_SERVICE);
    portallist[0] = &portalid;
/*    assert (RPC2_Init(RPC2_VERSION, 0, portallist, 1, -1, NULL) == RPC2_SUCCESS); */
    if ((rc = RPC2_Init(RPC2_VERSION, 0, portallist, 1, -1, NULL)) != RPC2_SUCCESS) {
	LogMsg(-1, 0, stdout, "RPC2_Init failed with %s", RPC2_ErrorMsg(rc));
	exit(-1);
    }
    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = AUTH_SUBSYSID;
    assert(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    }


PRIVATE void HandleRPCError(int rCode, RPC2_Handle connId)
    {
    fprintf(stderr, "auth2: %s", RPC2_ErrorMsg(rCode));
    if (rCode < RPC2_FLIMIT && connId != 0) RPC2_Unbind(connId);
    }


PRIVATE void InitAl()
    {
    register int i;
    RPC2_Integer rc;
    char *admin = "system:administrators";	/* assert dies on literals */
    struct stat buff;

    assert(stat(PDB, &buff) == 0);
    AuthTime = buff.st_mtime;
/*    assert(AL_Initialize(AL_VERSION, PDB, PCF) == 0); */
    if ((rc = AL_Initialize(AL_VERSION, PDB, PCF)) != 0) {
	LogMsg(-1, 0, stdout, "AL_Initialize failed with %d", rc);
	exit(-1);
    }
    assert(AL_NameToId(admin, &AdminID) == 0);
    for (i = 0; i < RPC2_KEYSIZE; i++)
	DeleteKey[i] = 0xff;
    }


PRIVATE void CheckTokenKey()
    {
    struct stat statbuf;
    FILE *tf;
    char mykey[RPC2_KEYSIZE+1];

    if(stat(TKFile, &statbuf))
	{
	perror("stat failed for token key file");
	exit(-1);
	}
    if(TokenTime != statbuf.st_mtime) 
	{
	if ((tf = fopen(TKFile, "r")) == NULL)
	    {
	    perror(TKFile);
	    exit(-1);
	    }
	fgets(mykey, RPC2_KEYSIZE+1, tf);
	bcopy(mykey, TokenKey, RPC2_KEYSIZE);
	TokenTime = statbuf.st_mtime;
	fclose(tf);
	}
    }


PRIVATE long PWGetKeys(RPC2_CountedBS *cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
/* cIdent:  body assumed to be an integer representing a ViceId in network order; converted to host order  */
/* hKey:    key to do handshake */
/* sKey:    key to use for remainder of connection's duration */

    /*  Coughs up keys for authentication handshake between auth server and client.
	Looks up PWArray for this. Makes sure user exists and has not been
	    deleted (i.e., excluded from logging in)
	The cIdent field may specify the client in Vice user name form or
	as an integer, ViceId.  The SeqBody field of cIdent is always a
	string.  If it is a number, it is interpreted as a ViceId.
	Else it is interpreted as a Vice user name.
    */
    {
    int vid;
    register int i;
    struct stat statbuff;

    LogMsg(0, AuthDebugLevel, stdout, "In PWGetKeys()");
    vid = GetViceId(cIdent);
    LogMsg(0, AuthDebugLevel, stdout, "\tvid = %d", vid);
    if (vid < 0) return(-1);
    if (!IsAUser(vid) || IsADeletedUser(vid)) return(-1);

    if(!stat(PWFile, &statbuff))
	{
	if(PWTime != statbuff.st_mtime)
	    {
	    InitPW();
	    }
	}

    bcopy(PWArray[vid], hKey, RPC2_KEYSIZE);
    rpc2_Decrypt((char *)hKey, (char *)hKey, RPC2_KEYSIZE, (unsigned char *)FileKey, RPC2_XOR);
    for (i = 0; i < RPC2_KEYSIZE; i++)
	sKey[i] = rpc2_NextRandom(NULL);	/* random session key */
    return(0);
    }


PRIVATE void LogFailures(RPC2_CountedBS *cIdent, RPC2_Integer eType, RPC2_HostIdent *pHost, RPC2_PortalIdent *pPortal)
    {
    unsigned it;
    assert(pHost->Tag == RPC2_HOSTBYINETADDR);
    it = pHost->Value.InetAddress;
    *(cIdent->SeqBody+cIdent->SeqLen-1) = 0;  /* just to make sure */
    LogMsg(-1, 0, stdout, "Authentication failed for \"%s\" from %d.%d.%d.%d",
	 cIdent->SeqBody, (it>>24)&0xff, (it>>16)&0xff, (it>>8)&0xff, it&0xff);

    }


/* ============= Bodies of RPC routines follow =============  */

long S_AuthNewConn(RPC2_Handle cid, RPC2_Integer seType, RPC2_Integer secLevel, RPC2_Integer encType, RPC2_CountedBS *cIdent)
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

extern hton_SecretToken(SecretToken *);
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
    struct UserInfo *p;
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
	printf("AuthChangePasswd(%d, \"", viceId);
	for (i = 0; i < RPC2_KEYSIZE; i++)
	    printf("%c", newPasswd[i]);
	printf("\")");
	}

    /* Do not allow if this is a read only server	*/
    if(CheckOnly)
	return(AUTH_READONLY);

    /* Ensure it's a system administrator or the user himself */
    RPC2_GetPrivatePointer(cid, (char **)&p);
    if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
    p->LastUsed = time(0);
    if (viceId != p->ViceId && !IsAdministrator(p))
	{
	char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
	LogMsg(-1, 0, stdout, "AuthChangePassd() attempt on %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
	return(AUTH_DENIED);	
	}

    if (!IsAUser(viceId) || IsADeletedUser(viceId)) return(AUTH_FAILED);
    
    /* Make the change */
    if (BogusKey(newPasswd)) return(AUTH_BADKEY);
    AppendPW(viceId, newPasswd, "", p->ViceId);
    return (AUTH_SUCCESS);
    }


long S_AuthNewUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey initKey, RPC2_String otherInfo)
    {
    struct UserInfo *p;

    /* Do not allow if this is a read only server	*/
    if(CheckOnly)
	return(AUTH_READONLY);

    /* make sure it's a system administrator */
    RPC2_GetPrivatePointer(cid, (char **)&p);
    if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
    p->LastUsed = time(0);
    if (!IsAdministrator(p))
	{
	char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
	LogMsg(-1, 0, stdout, "AuthNewUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
	return(AUTH_DENIED);
	}
    if (IsAUser(viceId)) return(AUTH_FAILED);

    /* make the change */
    if (BogusKey(initKey)) return(AUTH_BADKEY);
    AppendPW(viceId, initKey, (char *)otherInfo, p->ViceId);
    return(AUTH_SUCCESS);
    }


long S_AuthDeleteUser(RPC2_Handle cid, RPC2_Integer viceId)
    {
    struct UserInfo *p;

    /* Do not allow if this is a read only server	*/
    if(CheckOnly)
	return(AUTH_READONLY);

    /* make sure it's a system administrator */
    RPC2_GetPrivatePointer(cid, (char **)&p);
    if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
    p->LastUsed = time(0);
    if (!IsAdministrator(p))
	{
	char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
	LogMsg(-1, 0, stdout, "AuthDeleteUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
	return(AUTH_DENIED);
	}
    if (!IsAUser(viceId)) return(AUTH_FAILED);


    /* make the change */

    AppendPW(viceId, DeleteKey, "", p->ViceId);
    return(AUTH_SUCCESS);
    }


long S_AuthChangeUser(RPC2_Handle cid, RPC2_Integer viceId, RPC2_EncryptionKey newKey, RPC2_String otherInfo)
    {
    int i;
    struct UserInfo *p;
    
    if (AuthDebugLevel)
	{
	printf("AuthChangeUser(%d, \"", viceId);
	for (i = 0; i < RPC2_KEYSIZE; i++)
	    printf("%c", newKey[i]);
	printf("\", \"%s\")\n", (char *)otherInfo);
	}

    /* Do not allow if this is a read only server	*/
    if(CheckOnly)
	return(AUTH_READONLY);

    /* make sure it's a system administrator */
    RPC2_GetPrivatePointer(cid, (char **)&p);
    if (p == NULL || p->HasQuit == TRUE) return(AUTH_FAILED);
    p->LastUsed = time(0);
    if (!IsAdministrator(p))
	{
	char buf1[PRS_MAXNAMELEN], buf2[PRS_MAXNAMELEN];
	LogMsg(-1, 0, stdout, "AuthChangeUser() attempt on  %s by %s denied", GetVname(viceId, buf1), GetVname(p->ViceId, buf2));
	return(AUTH_DENIED);
	}
    if (!IsAUser(viceId)) return(AUTH_FAILED);


    /* make the change */
    if (BogusKey(newKey)) return(AUTH_BADKEY);
    AppendPW(viceId, newKey, (char *)otherInfo, p->ViceId);
    return(AUTH_SUCCESS);

    }


long S_AuthNameToId(RPC2_Handle cid, RPC2_String sUser, RPC2_Integer *sId)
    {
    *sId = -1;
    if (AL_NameToId((char *)sUser, (int *)sId) < 0)
	return(AUTH_FAILED);
    else return(AUTH_SUCCESS);
    }
    

PRIVATE void InitPW()
    /* Reads in contents of PWFile and builds sorted list of passwords in PWArray.
	Frees existing storage corr to PWArray
    */
    {
    int fd, pfd;
    struct stat stbuf;
    char *fbuf;

    pfd = LockParent(PWFile, LOCK_SH);

    if ((fd = open(PWFile, O_RDONLY, 0)) < 0
	|| flock(fd, LOCK_SH) < 0	/* locking is merely superstitious */
	|| fstat(fd, &stbuf))
	{
	perror(PWFile);
	abort();
	}
    assert((fbuf = (char *)malloc(1+stbuf.st_size)) != NULL);
    assert(stbuf.st_size == read(fd, fbuf, stbuf.st_size));	/* read entire file in */
    PWTime = stbuf.st_mtime;	/* time on file to check for changes */
    fbuf[stbuf.st_size] = 0;	/* sentinel to stop sscanf() later */
    flock(fd, LOCK_UN);  close(fd);
    flock(pfd, LOCK_UN); close(pfd);

    BuildPWArray(fbuf);
    free(fbuf);
    }


PRIVATE void BuildPWArray(char *fileBuf)
    /* fileBuf:	pointer to in-core contents of PWFile */
    /* parses the file buffer and builds up PWArray; sets PWLen and PWCount */
    {
    char *nextline, *kk;
    int thisid, i;
    RPC2_EncryptionKey thiskey;
    char holder[3];

    PWCount = 0;  /* no of valid entries in PWArray */
    bzero((char *)PWArray, PWLen*RPC2_KEYSIZE);
    nextline = fileBuf;	/* index into start of next line  in fbuf */

    while (TRUE)
	{
	if (index(nextline, '\t') == NULL) break;
	thisid = atoi(nextline);
	assert((kk = index(nextline, '\t')) != NULL);
	kk++;
	for (i = 0; i < RPC2_KEYSIZE; i++)
	    {
	    int x;
	    holder[0] = *(kk++);
	    holder[1] = *(kk++);
	    holder[2] = '\0';
	    sscanf(holder, "%2x", &x);
	    thiskey[i] = x;
	    }
	    
	if (AuthDebugLevel)
	    {
	    printf("%d\t", thisid);
	    for (i = 0; i < RPC2_KEYSIZE; i++)
		printf("%02x", thiskey[i]);
	    printf("\n"); fflush(stdout);
	    }
	if (thisid >= PWLen)
	    EnlargePW(2*thisid);	/* factor of 2 to prevent frequent realloc() */

	bcopy(thiskey, PWArray[thisid], RPC2_KEYSIZE);
	PWCount++;
	assert((nextline = index(nextline, '\n')) != NULL);
	nextline++;	/* safe, since fileBuf is NULL-terminated */
	}
    }


PRIVATE void EnlargePW(int newSize)
    /* Makes PWArray capable of holding at least newSize entries */
    {
    if (newSize < PWLen) return;	/* turkey! */
    LogMsg(0, AuthDebugLevel, stdout, "Reallocing from %d to %d", PWLen, newSize);
    PWArray = (RPC2_EncryptionKey *)realloc((char *)PWArray, newSize*RPC2_KEYSIZE);
    assert(PWArray != NULL);
    bzero(PWArray[PWLen], (newSize-PWLen)*RPC2_KEYSIZE);
    PWLen = newSize;
    }


PRIVATE void AppendPW(int vId, RPC2_EncryptionKey eKey, char *otherInfo, int agentId)
    /* eKey:	not yet encrypted with FileKey!! */
    /* 
	Appends a line to PWFile for user whose ViceId is vId.
	Logs each change with the ViceId and timestamp of the agent performing the change.
	Also updates PWArray, enlarging it if necessary.

	A periodic offline program should be run to squish old entries.
    */
    {
    int fd, pfd, i;
    char buf[100], *bnext;
    RPC2_EncryptionKey tempkey;
    struct stat buff;
    long cl;

    /* Encrypt the key first */
    rpc2_Encrypt((char *)eKey, (char *)tempkey, RPC2_KEYSIZE, (char *)FileKey, RPC2_XOR);

    /* Update PWArray after enlarging it */
    if (vId >= PWLen) EnlargePW(2*vId);
    bcopy(tempkey, PWArray[vId], RPC2_KEYSIZE);	/* overwrite existing key value */

    /* Build an image of the line to be appended */
    sprintf(buf, "%d\t", vId);
    bnext = &buf[strlen(buf)];
    for (i = 0; i < RPC2_KEYSIZE; i++)
	{
	int x = tempkey[i];
	sprintf(bnext, "%02x", x);
	bnext += 2;
	}
    sprintf(bnext, "\t%s", otherInfo);
    bnext += strlen(otherInfo);
    cl = time(0);
    sprintf(bnext, "\t# By %d at %s", agentId, ctime(&cl));
    
    /* Now lock parent and append the line */
    pfd = LockParent(PWFile, LOCK_EX);
    if ((fd = open(PWFile, O_WRONLY|O_APPEND, 0)) < 0
	|| flock(fd, LOCK_EX) < 0
	|| write(fd, buf, strlen(buf)) < 0)
	{
	perror(PWFile);
	abort();
	}

    /* Update PWTime because we have the current version of the file */
    fstat(fd, &buff);
    PWTime = buff.st_mtime;

    /* Unlock and quit */
    flock(fd, LOCK_UN); close(fd);
    flock(pfd, LOCK_UN); close(pfd);
    }


PRIVATE int LockParent(char *fName, int lockType)
    /* lockType:    LOCK_SH or LOCK_EX */
    /* Locks parent directory of fName and returns fd on parent.
	Used for syncing with mvdb */
    {
    char parent[1000];
    int pfd;

    if (fName[0] != '/')
	{
	getwd(parent);
	strcat(parent, "/");
	strcat(parent, fName);
	}
    else strcpy(parent, fName);
    *rindex(parent, '/') = 0;
		/* parent surely has at least one '/' by now */
	
    if ((pfd = open(parent, O_RDONLY, 0)) < 0
	|| flock(pfd, lockType) < 0)
	{
	perror(parent);
	abort();
	}
    return(pfd);
    }


PRIVATE int IsAUser(int viceId)
    /* Returns TRUE iff viceId corr to a Vice user (==> non-zero key) */
    {
    if (viceId < 0 || viceId >= PWLen) return (FALSE);	
    if (bcmp(PWArray[viceId], NullKey, RPC2_KEYSIZE) == 0) return (FALSE);
    return(TRUE);
    }


PRIVATE int IsADeletedUser(int viceId)
    /* Returns TRUE iff viceId corr to a deleted Vice user(==> key of all 1's) */
    {
    if (viceId < 0 || viceId >= PWLen) return (FALSE);
    if (bcmp(PWArray[viceId], DeleteKey, RPC2_KEYSIZE) == 0) return (TRUE);
    return(FALSE);
    }


PRIVATE int IsAdministrator(struct UserInfo *pU)
    /* Returns TRUE iff the CPS corr to pU is that of a system administrator.
	Obtains the CPS for pU if necessary */
    {
    if (pU->UserCPS == NULL)
	if (AL_GetInternalCPS(pU->ViceId, &pU->UserCPS) != 0)
	    return(FALSE);
    if (AL_IsAMember(AdminID, pU->UserCPS) != 0)
	    return(FALSE);
    return(TRUE);
    }


PRIVATE int BogusKey(RPC2_EncryptionKey x)
    /* Returns TRUE iff x would result in NullKey or DeleteKey when encrypted with FileKey.
	FALSE otherwise */
    {
    RPC2_EncryptionKey temp;
    rpc2_Encrypt((char *)x, (char *)temp, RPC2_KEYSIZE, (char *)FileKey, RPC2_XOR);
    if (bcmp(temp, NullKey, RPC2_KEYSIZE) == 0) return(TRUE);
    if (bcmp(temp, DeleteKey, RPC2_KEYSIZE) == 0) return(TRUE);
    return(FALSE);
    }


PRIVATE int GetViceId(RPC2_CountedBS *cIdent)
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

    
PRIVATE char *GetVname(int id, char *s)
    {
    if (AL_IdToName(id, s) < 0)
	sprintf(s, "%d (unknown Vice id)", id);
    return(s);
    }
