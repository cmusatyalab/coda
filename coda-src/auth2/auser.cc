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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/auth2/RCS/auser.cc,v 4.1 1997/01/08 21:49:25 rvb Exp $";
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
 -- Routines used by user-level processes (such as login, su, etc)  to do authentication

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef __MACH__
#include <sys/viceioctl.h>
#endif /* __MACH__ */
#ifdef __BSD44__
#include <cfs/mach_vioctl.h> /* new location/name of sys/viceioctl.h */
#endif /* __BSD44__ */
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <errno.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "auth2.h"

#define VSTAB "/usr/coda/etc/vstab"

PRIVATE int SetHost(int write, int index, char *AuthHost);
PRIVATE void GetVSTAB();
PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost, RPC2_Handle *RPCid);

 /* areas to keep interesting information about what hosts to use */
#define MAXHOSTS 32
PRIVATE	int numHosts;		    /* number of local hosts to use */
PRIVATE	char lHosts[64][MAXHOSTS];  /* array of local hosts to use */
PRIVATE	char pName[64];		    /* name to use on PIOCTLS */

char *DefAuthHost = "scarlatti.coda.cs.cmu.edu";

void U_HostToNetClearToken(ClearToken *cToken)
{
	cToken->AuthHandle = htonl(cToken->AuthHandle);
	cToken->ViceId = htonl(cToken->ViceId);
	cToken->BeginTimestamp = htonl(cToken->BeginTimestamp);
	cToken->EndTimestamp = htonl(cToken->EndTimestamp);
}


void U_NetToHostClearToken(ClearToken *cToken)
{
	cToken->AuthHandle = ntohl(cToken->AuthHandle);
	cToken->ViceId = ntohl(cToken->ViceId);
	cToken->BeginTimestamp = ntohl(cToken->BeginTimestamp);
	cToken->EndTimestamp = ntohl(cToken->EndTimestamp);
}

void ntoh_SecretToken(SecretToken *stoken) {
    stoken->AuthHandle = ntohl(stoken->AuthHandle);
    stoken->ViceId = ntohl(stoken->ViceId);
    stoken->BeginTimestamp = ntohl(stoken->BeginTimestamp);
    stoken->EndTimestamp = ntohl(stoken->EndTimestamp);
}

void hton_SecretToken(SecretToken *stoken) {
    stoken->AuthHandle = htonl(stoken->AuthHandle);
    stoken->ViceId = htonl(stoken->ViceId);
    stoken->BeginTimestamp = htonl(stoken->BeginTimestamp);
    stoken->EndTimestamp = htonl(stoken->EndTimestamp);
}

int U_Authenticate(IN char *uName, IN char *uPasswd, OUT ClearToken *cToken, OUT EncryptedSecretToken sToken)
 /* Talks to an authentication server and obtains tokens on behalf of user uName.
    Gets back the viceId and clear and secretTokens for this user    */
{
    RPC2_Handle	RPCid;
    int		rc;

    if(!(rc = U_BindToServer((char *)NULL, uName, uPasswd, &RPCid)))
	rc = AuthGetTokens(RPCid, sToken, cToken);
    AuthQuit(RPCid);
    RPC2_Unbind(RPCid);
    return(rc);
}


int U_ChangePassword(IN char *uName, IN char *newPasswd, IN char *myName, IN char *myPasswd)
 /* Talks to the central authentication server and changes the password for uName to
    newPasswd if myName is the same as uName or a system administrator.  MyPasswd
    is used to validate myName.  */

{
    int rc;
    RPC2_Integer cpid;
    RPC2_Handle RPCid;
    RPC2_EncryptionKey ek;

    if(!(rc = U_BindToServer((char *)NULL, myName, myPasswd, &RPCid))) {
	bzero((char *)ek, RPC2_KEYSIZE);
	strncpy((char *)ek, newPasswd, RPC2_KEYSIZE);
	if(!(rc = AuthNameToId(RPCid, (RPC2_String) uName, &cpid))) {
	    rc = AuthChangePasswd(RPCid, cpid, ek);
	}
    }
    return(rc);
}


void U_InitRPC()
{
    PROCESS mylpid;


    assert(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    assert (RPC2_Init(RPC2_VERSION, 0, NULL, 1, -1, NULL) > RPC2_ELIMIT);
}


char *U_AuthErrorMsg(int rc)
{
    switch(rc) {
	case AUTH_SUCCESS:	return("AUTH_SUCCESS");
	case AUTH_FAILED:	return("AUTH_FAILED");
	case AUTH_DENIED:	return("AUTH_DENIED");
	case AUTH_BADKEY:	return("AUTH_BADKEY");
	case AUTH_READONLY:	return("AUTH_READONLY");
	default:		return("Unknown Auth Return Code");
    }
}


int U_BindToServer(char *DefAuthHost, char *uName, char *uPasswd, RPC2_Handle *RPCid)
{
 /* Binds to Auth Server on behalf of uName using uPasswd as password.
    Sets RPCid to the value of the connection id.    */

    char    AuthHost[128];
    int     i = 0;
    int     rc;

    if (DefAuthHost) {
        rc = TryBinding(uName, uPasswd, DefAuthHost, RPCid);
    } else {
        GetVSTAB();
        while (1) {
	    if (SetHost(1, i, AuthHost) == 0) {
                rc = TryBinding(uName, uPasswd, AuthHost, RPCid);
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




PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost, RPC2_Handle *RPCid)
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
    strcpy(pident.Value.Name, AUTH_SERVICE);
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = htonl(AUTH_SUBSYSID);

    cident.SeqLen = 1+strlen(viceName);
    cident.SeqBody = (RPC2_ByteSeq)viceName;
    bzero(hkey, RPC2_KEYSIZE);
    bcopy(vicePasswd, hkey, RPC2_KEYSIZE);
    rc = RPC2_Bind(RPC2_SECURE, RPC2_XOR, &hident, &pident, &sident, NULL,
		   &cident, &hkey, RPCid);

    return (rc);
}




PRIVATE int SetHost(int write, int index, char *AuthHost)
{
 /* Sets AuthHost to the next host to try.  For now only mahler is available.
    The intention is to have the host be a list of hosts to try from vstab with
    mahler being used only if all of the other hosts are not available */

    if(index < numHosts && index >= 0) {
	strcpy(AuthHost, lHosts[index]);
	return(0);
    }
    return(-1);
}


PRIVATE void GetVSTAB()
{
    int		fd;
    int		len;
    char	* end;
    char	* area;
    char	* host;
    char	* endHost;
    char	* start;
    struct	stat	buff;

    bzero(pName,sizeof(pName));
    bzero((char *)lHosts,sizeof(lHosts));
    numHosts = 0;
    if(fd = open(VSTAB,O_RDONLY,0)) {
	if(!(fstat(fd, &buff))) {
	    area = (char *)malloc(buff.st_size);
	    if(!area) {
		free(area);
		(void)close(fd);
		goto fini;
	    }
	    len = read(fd, area, buff.st_size);
	    if(len == buff.st_size) {
		strncpy(pName,area,index(area,':')-area);
		host = index(area,':') + 1;
		host = index(host,':') + 1;
		endHost = index(host,':');
		for(start = host; start < endHost;) {
		    end = index(start,',');
		    if(!end || end>endHost) {
			end = endHost;
		    }
		    len = end - start;
		    strncpy(lHosts[numHosts++],start,len);
		    start += len + 1;
		}
	    }
	    else {
		perror("Read of VSTAB failed\n");
	    }
	    free(area);
	}
	else {
	    perror("fstat for VSTAB failed");
	}
	close(fd);
    }
    else {
	perror("open for VSTAB failed");
    }
 fini:
    if(strlen(pName) == 0)
	strcpy(pName, "/cmu");
}


char *U_Error(int rc)
{
    if(rc < 0)
	return((char *)RPC2_ErrorMsg(rc));
    else
	return((char *)U_AuthErrorMsg(rc));
}
