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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/auth2/Attic/au.cc,v 4.2 1997/02/26 16:02:30 rvb Exp $";
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
au.c -- authentication client program

*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <lwp.h>
#include <rpc2.h>
#include <se.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <prs.h>
#include "auth2.h"

#ifdef NOTUSED
extern int SetHost(int write, int index, char *AuthHost);
extern void GetVSTAB();
#endif

int main(int argc, char **argv);

#ifdef NOTUSED
PRIVATE int DoBinding(char *viceName, char *vicePasswd);
PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost);
#endif

PRIVATE void SetGlobals(int argc, char **argv);
PRIVATE int GetVid(char *s, int *id);

PRIVATE int DebugLevel;

PRIVATE int IAmMaster;	/* TRUE iff this is the master auth server */
PRIVATE char *DefAuthHost = NULL;
PRIVATE char *AuthPortal = AUTH_SERVICE;

PRIVATE int GetTokensFlag;
PRIVATE int ChangeUserFlag;
PRIVATE int NewUserFlag;
PRIVATE int DeleteUserFlag;
PRIVATE int ChangePasswordFlag;

PRIVATE int MyViceId;
PRIVATE char MyViceName[PRS_MAXNAMELEN];
PRIVATE char MyPassword[100];
PRIVATE RPC2_Handle AuthCid;


int main(int argc, char **argv)
    {
    register int rc;
    
    SetGlobals(argc, argv);
    U_InitRPC();
    printf("Your Vice name: ");
    gets(MyViceName);
    strcpy(MyPassword, getpass("Your password: "));

#ifdef NOTUSED
    rc = DoBinding(MyViceName, MyPassword);
#else
    rc = U_BindToServer(DefAuthHost, MyViceName, MyPassword, &AuthCid);
#endif

    printf("RPC2_Bind() --> %s\n", RPC2_ErrorMsg(rc));
    if (rc < RPC2_ELIMIT) exit(-1);

    if (ChangePasswordFlag)
	{
	char buf[100];
	RPC2_EncryptionKey ek;
	int rc;
	int cpid;
	char cname[PRS_MAXNAMELEN];

	printf("User name: ");
	gets(cname);
	printf("New password: ");
	gets(buf);
	bzero(ek, RPC2_KEYSIZE);
	strncpy((char *)ek, buf, RPC2_KEYSIZE);
	
	if (GetVid(cname, &cpid) < 0) goto Done;

	rc = AuthChangePasswd(AuthCid, cpid, ek);
	if (rc < 0)
	    printf("AuthChangePasswd() --> %s\n", RPC2_ErrorMsg(rc));
	else printf("AuthChangePasswd() --> %s\n", U_AuthErrorMsg(rc));
	}
	
    if (DeleteUserFlag)
	{
	int vid;
	char cname[PRS_MAXNAMELEN];

	printf("Vice user to delete: ");
	gets(cname);

	if (GetVid(cname, &vid) < 0) goto Done;

	rc = AuthDeleteUser(AuthCid, vid);
	if (rc < 0)
	    printf("AuthDeleteUser() --> %s\n", RPC2_ErrorMsg(rc));
	else printf("AuthDeleteUser() --> %s\n", U_AuthErrorMsg(rc));
	}

    if (ChangeUserFlag || NewUserFlag)
	{
	char buf[100];
	RPC2_EncryptionKey ek;
	int rc, cpid;
	char otherinfo[200], cname[PRS_MAXNAMELEN];

	printf("Vice user: ");
	gets(cname);
	printf("New password: ");
	gets(buf);
	bzero(ek, RPC2_KEYSIZE);
	strncpy((char *)ek, buf, RPC2_KEYSIZE);
	printf("New info: ");
	gets(otherinfo);
	
	if (GetVid(cname, &cpid) < 0) goto Done;
	if (ChangeUserFlag)
	    {
	    rc = AuthChangeUser(AuthCid, cpid, ek, (RPC2_String)otherinfo);
	    if (rc < 0)
		printf("AuthChangeUser() --> %s\n", RPC2_ErrorMsg(rc));
	    else printf("AuthChangeUser() --> %s\n", U_AuthErrorMsg(rc));
	    }
	else
	    {
	    rc = AuthNewUser(AuthCid, cpid, ek, (RPC2_String)otherinfo);
	    if (rc < 0)
		printf("AuthNewUser() --> %s\n", RPC2_ErrorMsg(rc));
	    else printf("AuthNewUser() --> %s\n", U_AuthErrorMsg(rc));
	    
	    }
	}
	
Done:
    RPC2_Unbind(AuthCid);
    return(0);
    }


#ifdef NOTUSED

PRIVATE int DoBinding(char *viceName, char *vicePasswd)
    /* Binds to (AuthHost, AuthPortal) on behalf of viceId using vicePasswd
	as password.  Sets AuthCid to the value of the connection id.
    */
    	
{
    int rc, i = 0;
    char AuthHost[128];

    if (DefAuthHost) {
        rc = TryBinding(viceName, vicePasswd, DefAuthHost);
    } else {
        GetVSTAB();
        while (1) {
	    if (SetHost(1, i, AuthHost) == 0) {
                rc = TryBinding(viceName, vicePasswd, AuthHost);
                if (rc == 0 || rc == RPC2_NOTAUTHENTICATED)
		    return (rc);
                i++;
	    } else {
                return(rc);
	    }
	}
    }
    return(rc);
}


PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost)
{
    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_EncryptionKey hkey;
    RPC2_CountedBS cident;
    long rc;

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, AuthHost);
    pident.Tag = RPC2_PORTALBYNAME;
    strcpy(pident.Value.Name, AuthPortal);
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = htonl(AUTH_SUBSYSID);

    cident.SeqLen = 1+strlen(viceName);
    cident.SeqBody = (RPC2_ByteSeq)viceName;
    bzero(hkey, RPC2_KEYSIZE);
    bcopy(vicePasswd, hkey, RPC2_KEYSIZE);
    rc = RPC2_Bind(RPC2_SECURE, RPC2_XOR, &hident, &pident, &sident, NULL,
	&cident, hkey, &AuthCid);

    return (rc);
}
#endif

PRIVATE void SetGlobals(int argc, char **argv)
    /* Set globals from command line args */
    {
    register int i;
    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0)
	    {
	    DebugLevel++;
	    continue;
	    }

	if (strcmp(argv[i], "-m") == 0)
	    {
	    IAmMaster = TRUE;
	    continue;
	    }

	if (strcmp(argv[i], "-h") == 0 && i < argc - 1)
	    {
	    DefAuthHost = argv[++i];
	    continue;
	    }

	if (strcmp(argv[i], "-p") == 0 && i < argc - 1)
	    {
	    AuthPortal = argv[++i];
	    continue;
	    }

	if (strcmp(argv[i], "gt") == 0)
	    {
	    GetTokensFlag = TRUE;
	    break;
	    }
	if (strcmp(argv[i], "cu") == 0)
	    {
	    ChangeUserFlag = TRUE;
	    break;
	    }
	if (strcmp(argv[i], "nu") == 0)
	    {
	    NewUserFlag = TRUE;
	    break;
	    }
	if (strcmp(argv[i], "cp") == 0)
	    {
	    ChangePasswordFlag = TRUE;
	    break;
	    }
	if (strcmp(argv[i], "du") == 0)
	    {
	    DeleteUserFlag = TRUE;
	    break;
	    }

	printf("Usage: au [-x] [-h host] [-p portal]  {gt,cp,cu,nu,du}\n");
	exit(-1);
	}
    }


PRIVATE int GetVid(char *s, int *id)
    {
    int rc;
    rc = AuthNameToId(AuthCid, (RPC2_String)s, (RPC2_Integer *)id);
    if (rc == AUTH_SUCCESS) return(0);
    if (rc < 0)
	printf("AuthNameToId() --> %s\n", RPC2_ErrorMsg(rc));
    else   printf("AuthNameToId() --> %s\n", U_AuthErrorMsg(rc));
    return(-1);
    }
