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
                           none currently

#*/


/*
 * definitions used by client processes (such as clog, testlog, au)  
 * to do authentication
 */

#ifndef _AUSER_INCLUDED
#define _AUSER_INCLUDED 1


#if defined (__CYGWIN32__) || defined (DJGPP)
#define VSTAB "c:\\usr\\coda\\etc\\vstab"
#else
#define VSTAB "/usr/coda/etc/vstab"
#endif

typedef enum AuthMethod {
	AUTH_CODA, AUTH_KERBEROS4, AUTH_KERBEROS5
} AuthMethod_t;

#ifdef KERBEROS4
long Krb4GetSecret(char *hostname, char **identity, int *identitylen, char **secret,
		   int *secretlen);
#endif
#ifdef KERBEROS5
long Krb5GetSecret(char *hostname, char **identity, int *identitylen, char **secret,
		   int *secretlen);
#endif

int U_GetAuthMethod(char *arg, RPC2_Integer *type);
void U_HostToNetClearToken(ClearToken *cToken);
void U_NetToHostClearToken(ClearToken *cToken);
void ntoh_SecretToken(SecretToken *stoken);
void hton_SecretToken(SecretToken *stoken);

int U_Authenticate(char *hostname, int AuthenticationType, char *uName,
		   int uNamelen, ClearToken *cToken, 
		   EncryptedSecretToken sToken, 
		   int passwdpipe, int interactive );

 /* Talks to the central authentication server and changes the password for
  * uName to newPasswd if myName is the same as uName or a system
  * administrator. MyPasswd is used to validate myName.  */
int U_ChangePassword(char *DefAuthHost, char *uName, char *newPasswd, 
		     int AuthenticationType, char *myName, int myNamelen,
                     char *myPasswd, int myPasswdlen);

void U_InitRPC();
char *U_AuthErrorMsg(int rc);

/* Binds to Auth Server on behalf of uName using uPasswd as password.
   Sets RPCid to the value of the connection id.    */
int U_BindToServer(char *DefAuthHost, RPC2_Integer AuthenticationType, 
		   char *uName, int uNamelen, char *uPasswd, int uPasswdlen,
		   RPC2_Handle *RPCid);


char *U_Error(int rc);

#endif _AUSER_INCLUDED
