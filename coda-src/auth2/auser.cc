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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/auth2/Attic/auser.cc,v 4.5 1997/12/10 16:06:18 braam Exp $";
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
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
#include <pioctl.h> 
#include <rpc2.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "auth2.h"

#define VSTAB "/usr/coda/etc/vstab"

PRIVATE int SetHost(int write, int index, char *AuthHost);
PRIVATE void GetVSTAB(char *);
PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost, 
		       RPC2_Handle *RPCid);

 /* areas to keep interesting information about what hosts to use */
#define MAXHOSTS 32
PRIVATE	int numHosts;		    /* number of local hosts to use */
PRIVATE	char lHosts[MAXHOSTNAMELEN][MAXHOSTS];  /* array of local hosts to use */
PRIVATE	char pName[MAXHOSTNAMELEN];		    /* name to use on PIOCTLS */

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

/* Talks to an authentication server and obtains tokens on behalf of user uName.
   Gets back the viceId and clear and secretTokens for this user    */
int U_Authenticate(IN char *uName, IN char *uPasswd, OUT ClearToken *cToken, 
		   OUT EncryptedSecretToken sToken)
{
    RPC2_Handle	RPCid;
    int		rc;

    if(!(rc = U_BindToServer((char *)NULL, uName, uPasswd, &RPCid)))
	rc = AuthGetTokens(RPCid, sToken, cToken);
    AuthQuit(RPCid);
    RPC2_Unbind(RPCid);
    return(rc);
}


 /* Talks to the central authentication server and changes the password for uName to
    newPasswd if myName is the same as uName or a system administrator.  MyPasswd
    is used to validate myName.  */
int U_ChangePassword(IN char *uName, IN char *newPasswd, IN char *myName, 
		     IN char *myPasswd)
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


/* Binds to Auth Server on behalf of uName using uPasswd as password.
   Sets RPCid to the value of the connection id.    */
int U_BindToServer(char *DefAuthHost, char *uName, char *uPasswd, 
		   RPC2_Handle *RPCid)
{
    char    AuthHost[128];
    int     i = 0;
    int     rc;

    GetVSTAB(VSTAB);
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
    return(rc);
}




PRIVATE int TryBinding(char *viceName, char *vicePasswd, char *AuthHost, 
		       RPC2_Handle *RPCid)
{
    RPC2_BindParms bp;
    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_EncryptionKey hkey;
    RPC2_CountedBS cident;
    long rc;
    int len;


    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, AuthHost);
    pident.Tag = RPC2_PORTALBYNAME;
    strcpy(pident.Value.Name, AUTH_SERVICE);
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = htonl(AUTH_SUBSYSID);

    cident.SeqLen = 1+strlen(viceName);
    cident.SeqBody = (RPC2_ByteSeq)viceName;
    if ( RPC2_KEYSIZE < strlen(vicePasswd) ) 
	len = RPC2_KEYSIZE; 
    else 
	len = strlen(vicePasswd);
	       
    bzero(hkey, RPC2_KEYSIZE);
    bcopy(vicePasswd, hkey, len);

    bp.SecurityLevel = RPC2_SECURE;
    bp.EncryptionType = RPC2_XOR;
    bp.SideEffectType = 0;
    bp.ClientIdent = &cident;
    bp.SharedSecret = &hkey;

    rc = RPC2_NewBinding(&hident, &pident, &sident, &bp, RPCid);

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


PRIVATE void GetVSTAB(char *vstab)
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

    fd = open(vstab, O_RDONLY, 0); 
    if ( fd < 0 ) {
	perror("Error opening VSTAB");
	return;
    }

    if(fstat(fd, &buff) == -1 ) {
	perror("Error statting VSTAB");
	return;
    }
    
    area = malloc(buff.st_size);
    if(!area) {
	perror("No memory!");
	close(fd);
	return;
    }

    len = read(fd, area, buff.st_size);
    if ( len != buff.st_size ) {
	perror("Error reading VSTAB");
	free(area);
	return;
    }
	
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
    free(area);
    close(fd);
    return;
}


char *U_Error(int rc)
{
    if(rc < 0)
	return((char *)RPC2_ErrorMsg(rc));
    else
	return((char *)U_AuthErrorMsg(rc));
}
