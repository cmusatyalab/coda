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


/*
 * definitions used by client processes (such as clog, testlog, au)  
 * to do authentication
 */

#ifndef _AUSER_INCLUDED
#define _AUSER_INCLUDED 1

#define VSTAB "/usr/coda/etc/vstab"
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

 /* Talks to the central authentication server and changes the password for uName to
    newPasswd if myName is the same as uName or a system administrator.  MyPasswd
    is used to validate myName.  */
int U_ChangePassword(char *uName, char *newPasswd, 
		     int AuthenticationType, char *myName, 
		     int myNamelen,
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
