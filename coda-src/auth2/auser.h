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
                           none currently

#*/


/*
 * definitions used by client processes (such as clog, testlog, au)  
 * to do authentication
 */

#ifndef _AUSER_INCLUDED
#define _AUSER_INCLUDED 1


typedef enum AuthMethod {
	AUTH_CODA, AUTH_KERBEROS4, AUTH_KERBEROS5
} AuthMethod_t;

int U_GetAuthMethod(const char *arg, RPC2_Integer *type);
void U_HostToNetClearToken(ClearToken *cToken);
void U_NetToHostClearToken(ClearToken *cToken);

int U_Authenticate(struct RPC2_addrinfo *srvs, const int AuthenticationType,
		   const char *uName, const int uNamelen,
		   ClearToken *cToken, EncryptedSecretToken sToken, 
		   const int verbose, const int interactive);

 /* Talks to the central authentication server and changes the password for
  * uName to newPasswd if myName is the same as uName or a system
  * administrator. MyPasswd is used to validate myName. */
int U_ChangePassword(struct RPC2_addrinfo *srvs, const char *uName,
		     const char *newPasswd, const int AuthenticationType,
		     const char *myName, const int myNamelen,
                     const char *myPasswd, const int myPasswdlen);

void U_InitRPC();
char *U_AuthErrorMsg(int rc);

/* Resolve hostname, or realm to a list of servers to contact */
struct RPC2_addrinfo *U_GetAuthServers(const char *realm, const char *host);

/* Binds to Auth Server on behalf of uName using uPasswd as password.
   Sets RPCid to the value of the connection id.    */
int U_BindToServer(struct RPC2_addrinfo *srvs,
		   const RPC2_Integer AuthenticationType, 
		   const char *uName, const int uNamelen,
		   const char *uPasswd, const int uPasswdlen,
		   RPC2_Handle *RPCid, const int interactive);


char *U_Error(int rc);

#endif /* _AUSER_INCLUDED */
