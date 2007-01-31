/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
au.c -- authentication client program
*/


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/rpc2_addrinfo.h>
#include <rpc2/se.h>
#include <util.h>
#include <prs.h>
#include "auth2.h"
#include "auser.h"
#include "parse_realms.h"
#include "codaconf.h"

#ifdef __cplusplus
}
#endif


static void SetGlobals(int argc, char **argv);
static int GetVid(char *s, int *id);

static int DebugLevel;

static char *host;
static char *AuthPortal = AUTH_SERVICE;

static int ChangeUserFlag;
static int NewUserFlag;
static int DeleteUserFlag;
static int ChangePasswordFlag;

static RPC2_Handle AuthCid;


int main(int argc, char **argv)
{
    struct RPC2_addrinfo *srvs = NULL;
    char *realm = NULL;
    int rc;
    char MyViceName[PRS_MAXNAMELEN];
    char MyPassword[100];
    char cname[PRS_MAXNAMELEN];
    RPC2_EncryptionKey ek;

    memset(ek, 0, sizeof(ek));
    memset(cname, 0, sizeof(cname));
    memset(MyViceName, 0, sizeof(MyViceName));
    memset(MyPassword, 0, sizeof(MyPassword));    
    
    SetGlobals(argc, argv);

    U_InitRPC();

    printf("Your Vice name: ");
    fgets(MyViceName, sizeof(MyViceName), stdin);
    if (MyViceName[strlen(MyViceName)-1] == '\n' ){
	    MyViceName[strlen(MyViceName)-1] = '\0';
    }
    strncpy(MyPassword, getpass("Your password: "), sizeof(MyPassword));

    /* get realm information */
    SplitRealmFromName(MyViceName, &realm);
    codaconf_init("venus.conf");
    codaconf_init("auth2.conf");
    CODACONF_STR(realm, "realm", NULL);

    srvs = U_GetAuthServers(realm, host);
    rc = U_BindToServer(srvs, AUTH_METHOD_CODAUSERNAME, MyViceName, strlen(MyViceName)+1, MyPassword, strlen(MyPassword), &AuthCid, 1);
    RPC2_freeaddrinfo(srvs);

    printf("RPC2_Bind() --> %s\n", RPC2_ErrorMsg(rc));
    if (rc < RPC2_ELIMIT) exit(-1);

    if (ChangePasswordFlag) {
	char buf[100];
	int rc;
	int cpid;

	memset(buf, 0, sizeof(buf));

	printf("User name: ");
	fgets(cname, sizeof(cname), stdin);
	if ( cname[strlen(cname)-1] == '\n' ) 
		cname[strlen(cname)-1] = '\0';
	printf("New password: ");
	fgets(buf, sizeof(buf), stdin);
	if ( buf[strlen(buf)-1] == '\n' ) 
		buf[strlen(buf)-1] = '\0';
	
	strncpy((char *)ek, buf, RPC2_KEYSIZE);
	
	if (GetVid(cname, &cpid) < 0) goto Done;

	rc = AuthChangePasswd(AuthCid, cpid, ek);
	if (rc < 0)
	    printf("AuthChangePasswd() --> %s\n", RPC2_ErrorMsg(rc));
	else printf("AuthChangePasswd() --> %s\n", U_AuthErrorMsg(rc));
    }
	
    if (DeleteUserFlag) {
	int vid;

	printf("Vice user to delete: ");
	fgets(cname, sizeof(cname), stdin);
	if ( cname[strlen(cname)-1] == '\n' ) 
		cname[strlen(cname)-1] = '\0';

	if (GetVid(cname, &vid) < 0) 
		goto Done;

	rc = AuthDeleteUser(AuthCid, vid);
	if (rc < 0)
	    printf("AuthDeleteUser() --> %s\n", RPC2_ErrorMsg(rc));
	else 
	    printf("AuthDeleteUser() --> %s\n", U_AuthErrorMsg(rc));
    }

    if (ChangeUserFlag || NewUserFlag)	{
	char buf[100];
	int rc, cpid;
	char otherinfo[200];

	memset(otherinfo, 0, sizeof(otherinfo));
	printf("Vice user: ");
	fgets(cname, sizeof(cname), stdin);
	if ( cname[strlen(cname)-1] == '\n' ) 
		cname[strlen(cname)-1] = '\0';

	printf("New password: ");
	fgets(buf, sizeof(buf), stdin);
	if ( buf[strlen(buf)-1] == '\n' ) 
		buf[strlen(buf)-1] = '\0';
	strncpy((char *)ek, buf, RPC2_KEYSIZE);
	printf("New info: ");
	fgets(otherinfo, sizeof(otherinfo), stdin);
	if ( otherinfo[strlen(otherinfo)-1] == '\n' ) 
		otherinfo[strlen(otherinfo)-1] = '\0';
	
	if (GetVid(cname, &cpid) < 0) goto Done;
	if (ChangeUserFlag) {
	    rc = AuthChangeUser(AuthCid, cpid, ek, (RPC2_String)otherinfo);
	    if (rc < 0)
		printf("AuthChangeUser() --> %s\n", RPC2_ErrorMsg(rc));
	    else printf("AuthChangeUser() --> %s\n", U_AuthErrorMsg(rc));
	} else {
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

/* Set globals from command line args */
static void SetGlobals(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0)
	    {
	    DebugLevel++;
	    continue;
	    }

	if (strcmp(argv[i], "-h") == 0 && i < argc - 1)
	    {
	    host = argv[++i];
	    continue;
	    }

	if (strcmp(argv[i], "-p") == 0 && i < argc - 1)
	    {
	    AuthPortal = argv[++i];
	    continue;
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

	}
	if (ChangePasswordFlag || ChangeUserFlag || NewUserFlag || DeleteUserFlag)
	    return;

	printf("Usage: au [-x] [-h host] [-p portal]  {cp,cu,nu,du}\n");
	exit(-1);
}


static int GetVid(char *s, int *id)
{
	int rc;
	rc = AuthNameToId(AuthCid, (RPC2_String)s, (RPC2_Integer *)id);
	if (rc == AUTH_SUCCESS) 
		return(0);
	if (rc < 0)
		printf("AuthNameToId() --> %s\n", RPC2_ErrorMsg(rc));
	else   
		printf("AuthNameToId() --> %s\n", U_AuthErrorMsg(rc));
	return(-1);
}
