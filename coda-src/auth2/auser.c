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
 -- Routines used by user-level processes (such as login, su, etc)  to do authentication

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <lwp.h>
#include <pioctl.h> 
#include <rpc2.h>
#include <util.h>
#include <prs.h>
#include "auth2.h"
#include "auth2.common.h"
#include "auser.h"
#ifdef __cplusplus
}
#endif __cplusplus

#ifdef __CYGWIN32__
#define VSTAB "c:\\usr\\coda\\etc\\vstab"
#else
#define VSTAB "/usr/coda/etc/vstab"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int SetHost(int write, int index, char *AuthHost);
static void GetVSTAB(char *);
static int TryBinding(RPC2_Integer AuthenticationType, char *viceName, int viceNamelen, char *vicePasswd, int vicePasswdlen, char *AuthHost, RPC2_Handle *RPCid);

 /* areas to keep interesting information about what hosts to use */
#define MAXHOSTS 32
static	int numHosts;		    /* number of local hosts to use */
static	char lHosts[MAXHOSTNAMELEN][MAXHOSTS];  /* array of local hosts to use */
static	char pName[MAXHOSTNAMELEN];		    /* name to use on PIOCTLS */

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


/* Talks to an authentication server and obtains tokens on behalf of user uName.
   Gets back the viceId and clear and secretTokens for this user    */
int U_Authenticate(char *hostname, int AuthenticationType, char *uName,
		   int uNamelen, OUT ClearToken *cToken, 
		   OUT EncryptedSecretToken sToken, 
		   int passwdpipe, int interactive )
{
	RPC2_Handle	RPCid;
	int		rc;
	int             bound = 0;
	char            passwd[128];
	char *secret, *identity;
	int secretlen, identitylen;

	bzero(passwd, sizeof(passwd));

	switch(AuthenticationType) {
	case AUTH_METHOD_NULL:
		/* don't do this */
		fprintf(stderr, "Calling U_Authenticate without"
			"any authentication seems pretty stupid.\n");
		exit(1);
	case AUTH_METHOD_CODAUSERNAME:
		if (passwdpipe) {
			fgets(passwd, sizeof(passwd), stdin);
			rc = strlen(passwd);
			if ( passwd[rc-1] == '\n' )
				passwd[rc-1] = 0;
		} else {
			strncpy (passwd, getpass ("Password: "), 
				 sizeof(passwd)-1);
			passwd[sizeof(passwd)-1] ='\0';
		}

		identity = uName;
		identitylen = uNamelen;
		secret = passwd;
		secretlen = strlen(passwd);
		rc = 0;
		break;
	case AUTH_METHOD_KERBEROS4:
#ifdef KERBEROS4
		Krb4Init(NULL);
		rc = Krb4GetSecret(hostname, &identity, &identitylen,
				   &secret, &secretlen);

		rc = Krb4DoKinit();
		if ( rc == 0 )
			rc = Krb4GetSecret(hostname, &identity, &identitylen,
					   &secret, &secretlen);
#else
		fprintf(stderr, "Kerberos4 not supported\n");
		exit(1);
#endif
		break;
	case AUTH_METHOD_KERBEROS5:
#ifdef KERBEROS5

		rc = Krb5Init(NULL);

		if ( rc != 0 ) {
			fprintf(stderr, "Cannot initialize KRB5\n");
			exit(1);
		}
			
		rc = Krb5GetSecret(hostname, &identity, &identitylen,
				   &secret, &secretlen);
		if (rc == 0)
			break;
		if (interactive == 0) 
			break;

		rc = Krb5DoKinit();
		if ( rc == 0 )
			rc = Krb5GetSecret(hostname, &identity, &identitylen,
					   &secret, &secretlen);
#else
		fprintf(stderr, "Kerberos5 not supported\n");
		exit(1);
#endif
		break;
	default:
		fprintf(stderr, "Unsupported authentication type\n");
		exit(1);
	}
			
	if (rc)
		return rc;


	rc = U_BindToServer(hostname, AuthenticationType, identity,
			    identitylen, secret, secretlen, &RPCid);
	if(rc == 0) {
		bound = 1;
		rc = AuthGetTokens(RPCid, sToken, cToken);
		if ( rc >= 0 ) 
			/* return irrelevant */
			AuthQuit(RPCid);
	}

        if (bound)
		RPC2_Unbind(RPCid);

	return(rc);
}


 /* Talks to the central authentication server and changes the password for uName to
    newPasswd if myName is the same as uName or a system administrator.  MyPasswd
    is used to validate myName.  */
int U_ChangePassword(IN char *uName, IN char *newPasswd, IN int AuthenticationType, IN char *myName, IN int myNamelen,
		     IN char *myPasswd, IN int myPasswdlen)
{
    int rc;
    RPC2_Integer cpid;
    RPC2_Handle RPCid;
    RPC2_EncryptionKey ek;

    if(!(rc = U_BindToServer((char *)NULL, AuthenticationType, myName, myNamelen, myPasswd, myPasswdlen, &RPCid))) {
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
    int rc;
    struct timeval tout;


    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
    
    tout.tv_sec = 15;
    tout.tv_usec = 0;

    rc = RPC2_Init(RPC2_VERSION, 0, NULL, -1, &tout);
    if ( rc != RPC2_SUCCESS ) {
	    fprintf(stderr, "Cannot initialize RPC2 (error %d). ! Exiting.\n",
		    rc);
	    exit(1);
    }
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
int U_BindToServer(char *DefAuthHost, RPC2_Integer AuthenticationType, 
		   char *uName, int uNamelen, char *uPasswd, int uPasswdlen,
		   RPC2_Handle *RPCid)
{
	char    AuthHost[MAXHOSTNAMELEN];
	int     i = 0;
	int     rc;
	int bound;

	if ( DefAuthHost ) {
		bound = TryBinding(AuthenticationType, uName, uNamelen, 
				   uPasswd, uPasswdlen, DefAuthHost, RPCid);
		return bound;
	}
	

	/* fill in the host array */
	GetVSTAB(VSTAB);

	/* try all valid entries until we are rejected or accepted */
	while ((rc = SetHost(1, i, AuthHost)) == 0 ) {		
		bound = TryBinding(AuthenticationType, uName, uNamelen, 
				   uPasswd, uPasswdlen, AuthHost, RPCid);
		if (bound == 0 || bound == RPC2_NOTAUTHENTICATED)
			return (bound);
		i++;
	}
	return bound;
}




static int TryBinding(RPC2_Integer AuthenticationType, char *viceName, 
		      int viceNamelen, char *vicePasswd, 
		      int vicePasswdlen, char *AuthHost, 
		      RPC2_Handle *RPCid)
{
    RPC2_BindParms bp;
    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_EncryptionKey hkey;
    RPC2_CountedBS cident;
    long rc;
    int len;

    hident.Tag = RPC2_HOSTBYINETADDR;
    if (inet_aton(AuthHost, &hident.Value.InetAddress) == 0)
    {
	hident.Tag = RPC2_HOSTBYNAME;
	strcpy(hident.Value.Name, AuthHost);
    }
    pident.Tag = RPC2_PORTBYNAME;

    strcpy(pident.Value.Name, AUTH_SERVICE);
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = htonl(AUTH_SUBSYSID);

    cident.SeqLen = viceNamelen;
    cident.SeqBody = (RPC2_ByteSeq)viceName;
    if ( RPC2_KEYSIZE < vicePasswdlen ) 
	len = RPC2_KEYSIZE; 
    else 
	len = vicePasswdlen;
	       
    bzero(hkey, RPC2_KEYSIZE);
    bcopy(vicePasswd, hkey, len);

    bp.SecurityLevel = RPC2_SECURE;
    bp.EncryptionType = RPC2_XOR;
    bp.SideEffectType = 0;
    bp.AuthenticationType = AuthenticationType;
    bp.ClientIdent = &cident;
    bp.SharedSecret = &hkey;

    rc = RPC2_NewBinding(&hident, &pident, &sident, &bp, RPCid);

    return (rc);
}



/* sets authhost to host[i] */
static int SetHost(int write, int index, char *AuthHost)
{
    if(index < numHosts && index >= 0) {
	    strcpy(AuthHost, lHosts[index]);
    } else
	    return -1;
    
    if( *AuthHost == '\0' )
	    return -1;

    return 0;
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void GetVSTAB(char *vstab)
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

    fd = open(vstab, O_RDONLY | O_BINARY, 0); 
    if ( fd < 0 ) {
	    perror("Error opening VSTAB");
	return;
    }

    if(fstat(fd, &buff) == -1 ) {
	perror("Error statting VSTAB");
	return;
    }
    
    area = (char *) malloc(buff.st_size+1);
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
    area[buff.st_size] = '\0';
	
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

/* sets type only if correct flag is found */
int U_GetAuthMethod(char *arg, RPC2_Integer *type)
{
	if ( strcmp(arg, "-coda") == 0 ) {
		*type =  AUTH_METHOD_CODAUSERNAME;
		return 1;
	}
	if ( strcmp(arg, "-kerberos4") == 0 ) {
		*type =  AUTH_METHOD_KERBEROS4;
		return 1;
	}
	if ( strcmp(arg, "-kerberos5") == 0 ) {
		*type =  AUTH_METHOD_KERBEROS5;
		return 1;
	}
	return 0 ;
}
