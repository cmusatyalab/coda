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
 * Routines used by user-level processes (such as login, su, etc)  to do
 * authentication
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#ifndef __CYGWIN__
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <lwp/lwp.h>
#include <pioctl.h> 
#include <rpc2/rpc2.h>
#include <util.h>
#include <prs.h>
#include "auth2.h"
#include "auth2.common.h"
#include "auser.h"
#ifdef __cplusplus
}
#endif

#include <codaconf.h>
#include <parse_realms.h>

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
int U_Authenticate(struct RPC2_addrinfo *srvs, const int AuthenticationType,
		   const char *uName, const int uNamelen,
		   OUT ClearToken *cToken, 
		   OUT EncryptedSecretToken sToken, 
		   const int verbose, const int interactive)
{
	RPC2_Handle	RPCid;
	int		rc;
	int             bound = 0;
	char            passwd[128];
	int		secretlen;

	memset(passwd, 0, sizeof(passwd));

	switch(AuthenticationType) {
	case AUTH_METHOD_CODAUSERNAME:
		if (!interactive) {
			fgets(passwd, sizeof(passwd), stdin);
			rc = strlen(passwd);
			if ( passwd[rc-1] == '\n' )
				passwd[rc-1] = 0;
		} else {
			strncpy (passwd, getpass ("Password: "), 
				 sizeof(passwd)-1);
			passwd[sizeof(passwd)-1] ='\0';
		}

		rc = 0;
		break;
#ifdef HAVE_KRB4
	case AUTH_METHOD_KERBEROS4:
		Krb4ClientInit();
		/* actual secret recovery is done in U_BindToServer */
		break;
#endif
#ifdef HAVE_KRB5
	case AUTH_METHOD_KERBEROS5:
		rc = Krb5ClientInit();
		if ( rc != 0 ) {
			fprintf(stderr, "Cannot initialize KRB5\n");
			exit(1);
		}
		/* actual secret recovery is done in U_BindToServer */
		break;
#endif
	default:
		fprintf(stderr, "Unsupported authentication type\n");
		exit(1);
	}
			
	if (rc)
		return rc;

	secretlen = strlen(passwd);
	rc = U_BindToServer(srvs, AuthenticationType, uName, uNamelen,
			    passwd, secretlen, &RPCid, interactive);
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


 /* Talks to the central authentication server and changes the password for
  * uName to newPasswd if myName is the same as uName or a system
  * administrator. MyPasswd is used to validate myName. */
int U_ChangePassword(struct RPC2_addrinfo *srvs, const char *uName,
		     const char *newPasswd, const int AuthenticationType,
		     const char *myName, const int myNamelen,
		     const char *myPasswd, const int myPasswdlen)
{
    int rc;
    RPC2_Integer cpid;
    RPC2_Handle RPCid;
    RPC2_EncryptionKey ek;

    if (AuthenticationType != AUTH_METHOD_CODAUSERNAME) {
	fprintf(stderr, "Cannot change kerberos passwords with auth2 tools\n");
	exit(-1);
    }

    if(!(rc = U_BindToServer(srvs, AuthenticationType, myName, myNamelen,
			     myPasswd, myPasswdlen, &RPCid, 0)))
    {
	memset ((char *)ek, 0, RPC2_KEYSIZE);
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
    RPC2_Options options;


    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);
    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    tout.tv_sec = 15;
    tout.tv_usec = 0;

    rc = RPC2_Init(RPC2_VERSION, &options, NULL, -1, &tout);
    if ( rc != RPC2_SUCCESS ) {
	    fprintf(stderr, "Cannot initialize RPC2 (error %d). ! Exiting.\n",
		    rc);
	    exit(1);
    }
}


char *U_AuthErrorMsg(const int rc)
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

static struct RPC2_addrinfo *GetAuthServers(const char *realm)
{
    struct RPC2_addrinfo *res = NULL;

    GetRealmServers(realm, "codaauth2", &res);

    if (!res) {
	fprintf(stderr, "Unable to resolve addresses for Coda auth2 servers in realm '%s'\n", realm);
    }
    return res;
}

static int TryBinding(const RPC2_Integer AuthenticationType,
		      const char *viceName, const int viceNamelen,
		      const char *vicePasswd, const int vicePasswdlen,
		      const struct RPC2_addrinfo *AuthHost, RPC2_Handle *RPCid)
{
    RPC2_BindParms bp;
    RPC2_HostIdent hident;
    RPC2_SubsysIdent sident;
    RPC2_CountedBS cident;
    RPC2_EncryptionKey hkey;
    long rc;
    int len;

    hident.Tag = RPC2_HOSTBYADDRINFO;
    hident.Value.AddrInfo = RPC2_copyaddrinfo(AuthHost);

    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = htonl(AUTH_SUBSYSID);

    cident.SeqLen = viceNamelen;
    cident.SeqBody = (RPC2_ByteSeq)viceName;

    len = vicePasswdlen;
    if ( len > RPC2_KEYSIZE) 
	len = RPC2_KEYSIZE; 
    memset(hkey, 0, RPC2_KEYSIZE);
    memcpy(hkey, vicePasswd, len);

    bp.SecurityLevel = RPC2_SECURE;
    bp.EncryptionType = RPC2_XOR;
    bp.SideEffectType = 0;
    bp.AuthenticationType = AuthenticationType;
    bp.ClientIdent = &cident;
    bp.SharedSecret = &hkey;

    rc = RPC2_NewBinding(&hident, NULL, &sident, &bp, RPCid);

    RPC2_freeaddrinfo(hident.Value.AddrInfo);

    return (rc);
}

struct RPC2_addrinfo *U_GetAuthServers(const char *realm, const char *host)
{
    struct RPC2_addrinfo hints, *res = NULL;
    int rc;

    if (!host)
	return GetAuthServers(realm);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    rc = RPC2_getaddrinfo(host, "codaauth2", &hints, &res);

    return (rc == 0) ? res : NULL;
}

/* Binds to Auth Server on behalf of uName using uPasswd as password.
   Sets RPCid to the value of the connection id.    */
int U_BindToServer(struct RPC2_addrinfo *srvs,
		   const RPC2_Integer AuthenticationType, 
		   const char *uName, const int uNamelen,
		   const char *uPasswd, const int uPasswdlen,
		   RPC2_Handle *RPCid, const int interactive)
{
	struct RPC2_addrinfo AuthHost, *p;
	int bound = RPC2_FAIL;

	/* try all valid entries until we are rejected or accepted */
	for (p = srvs; p; p = p->ai_next)
	{
	    /* struct assignment, we only want to try the hosts one by one
	     * instead of having rpc2 walk the whole chain on each call to
	     * RPC2_NewBinding. */
	    AuthHost = *p;
	    AuthHost.ai_next = NULL;
#ifdef HAVE_KRB5
	    /* Get host secret for next host */
	    /* Either I did this right, or I broke multiple servers badly
	     * --Troy
	     */
	    if (AuthenticationType == AUTH_METHOD_KERBEROS5) {
		/* should this be error checked ?*/
		if (Krb5GetSecret(AuthHost.ai_canonname, &uName, &uNamelen,
				  &uPasswd, &uPasswdlen, interactive))
		{
		    fprintf(stderr, "Failed to get secret for %s\n", AuthHost->ai_canonname);
		    continue;
		}
	    }
#endif
#ifdef HAVE_KRB4
	    /* Copied Troy's success or mistake :) -JH */
	    if (AuthenticationType == AUTH_METHOD_KERBEROS4) {
		if (Krb4GetSecret(AuthHost.ai_canonname, &uName, &uNamelen,
				   &uPasswd, &uPasswdlen, interactive))
		{
		    fprintf(stderr, "Failed to get secret from %s\n",
			    AuthHost.ai_canonname);
		    continue;
		}
	    }
#endif
	    bound = TryBinding(AuthenticationType, uName, uNamelen, 
			       uPasswd, uPasswdlen, &AuthHost, RPCid);

	    /* break when we are successful or a server rejects our secret */
	    if (bound == 0 || bound == RPC2_NOTAUTHENTICATED)
		break;
	}
	return bound;
}

char *U_Error(const int rc)
{
    if(rc < 0)
	return((char *)RPC2_ErrorMsg(rc));
    else
	return((char *)U_AuthErrorMsg(rc));
}

/* sets type only if correct flag is found */
int U_GetAuthMethod(const char *arg, RPC2_Integer *type)
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
