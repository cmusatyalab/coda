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


/* krbsupport.cc -- kerberos support routines for ViceII

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <netdb.h>
#ifdef __linux__
#include <asm/param.h>
#else
#include <sys/param.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#ifdef KERBEROS4
#include <des.h>
#include <krb.h>
#endif	/* KERBEROS4 */

#ifdef KERBEROS5
#include <com_err.h>
#include <krb5.h>
#endif	/* KERBEROS5 */

#include <util.h>
#include <prs.h>
#include <al.h>
#include "auth2.h"

#ifdef __cplusplus
}
#endif __cplusplus

#define min(x, y) (x > y ? y : x)

extern int GetViceId(RPC2_CountedBS *cIdent);

#ifdef KERBEROS5
krb5_context coda_auth_krb5context;
krb5_principal coda_auth_krb5princ;
char *K5KINIT = "/usr/athena/bin/kinit";
static krb5_keytab coda_keytab = NULL;
#endif

/* If the user specified a realm during initialization it will be stored in
 * here */
char *realm = NULL;

#ifdef KERBEROS4
/* The following will definitely go away with a config file! XXXX */
char *K4KINIT = "/usr/bin/kinit";
#define SRVPRINC "rcmd"

void Krb4Init(char *initrealm) 
{
    if (initrealm)
	realm = strdup(initrealm);
}

/* Client-side call to get authenticator */
long Krb4GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen)
{
	struct ktext *authent;
	struct credentials *cred;
	char default_realm[REALM_SZ];
	int i;
	struct hostent *authserver;

	authserver = gethostbyname(hostname);
	if ( ! authserver ) {
		herror("Krb4GetSecret:");
		return -1;
	}
	/* Kerberos Authentication Invoked */

	/* if realm is not specified, try to get the default realm */
	if (!realm) {
	    krb_get_lrealm(default_realm, 1);
	    realm = strdup(default_realm);
	}

	assert(cred = (struct credentials *)malloc(sizeof(struct credentials)));
	assert(authent = (struct ktext *) malloc(sizeof(struct ktext)));

	i = krb_mk_req(authent, SRVPRINC, authserver->h_name, realm, (long) 0);
	if (i) {
		fprintf(stderr, "auser: %s (%d)\n", krb_err_txt[i], i);
		free(cred);
		free(authent);
		return(1);
	}

	i = krb_get_cred(SRVPRINC, hostname, realm, cred);
	if (i) {
		fprintf(stderr, "Krb4GetSecret: %s (%d)\n", krb_err_txt[i], i);
		free(cred);
		free(authent);
		return(1);
	}

	fprintf(stderr, "Acquired kerberos authenticator; sending to auth server\n");

	*identity = (char *) authent;
	*identitylen = sizeof(struct ktext);
	*secret = cred->session;
	*secretlen = 8;

	return 0;
}

long Krb4DoKinit()
{
	char            *childargv[] = { "kinit", NULL };

	child = fork();
	if ( child == -1 ) {
		perror("fork: ");
		exit(1);
	}
	if ( child == 0 ) {  /*  in child */
		execvp(K4KINIT, childargv);
	}
	if ( child ) {
		waitpid(child, &rc, 0);
	}
}


/* Server-side call to verify authenticator */
long Krb4GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
	struct auth_dat	auth;
	int	i, vid;
	char *thishost[MAXHOSTNAMLEN];
	char default_realm[REALM_SZ];
	struct hostent *authserver;
	int rc;

	rc = gethostname(thishost, MAXHOSTNAMLEN);
	if ( rc ) {
		fprintf(stderr, "Krb4GetKeys: Cannot find hostname of this machine\n");
		return -1;
	}
	authserver = gethostbyname(thishost);
	if ( ! authserver ) {
		herror("Krb4GetKeys: cannot do gethostbyname");
		return -1;
	}
	

	LogMsg(0, AuthDebugLevel, stdout, "In Krb4GetKeys()");

	if (cIdent->SeqLen != sizeof(struct ktext))
	{
		LogMsg(0, AuthDebugLevel, stdout, "\tcIdent too small to be authenticator");
		return(-1);
	}

	i = krb_rd_req((struct ktext *) (cIdent->SeqBody), SRVPRINC, authserver,
		(u_long) 0, &auth, "");

	/* if realm is not specified, try to get the default realm */
	if (!realm) {
	    krb_get_lrealm(default_realm, 1);
	    realm = strdup(default_realm);
	}

	if (strncmp((char *) auth.prealm, realm, REALM_SZ))
	{
		LogMsg(0, AuthDebugLevel, stdout, "\t%s.%s@%s not a valid realm for authentication", auth.pname, auth.pinst, auth.prealm);
		return(-1);
	}

	if (i)
	{
		LogMsg(0, AuthDebugLevel, stdout, "\t%s (%d)", krb_err_txt[i], i);
		return(-1);
	}

	/* copy session key */
	assert(RPC2_KEYSIZE == 8);	/* assume Coda key and Kerb key are the same size XXXX */

	for (i=0; i<RPC2_KEYSIZE; i++)
	{
		hKey[i] = auth.session[i];
	}
	for (i=0; i<RPC2_KEYSIZE; i++)
	{
		sKey[i] = rpc2_NextRandom(NULL);
	}

	/* now need to copy kerberos name into cIdent so that other people
	   know who it is  XXXX should realloc? */

	assert(ANAME_SZ + INST_SZ + 1 <= cIdent->SeqLen);	/* XXXXX */

	snprintf((char *) cIdent->SeqBody, cIdent->SeqLen, "%s.%s", auth.pname, auth.pinst);
	cIdent->SeqLen = strlen((char *) cIdent->SeqBody);
	if ((cIdent->SeqLen != 0) && (cIdent->SeqBody[cIdent->SeqLen-1] == '.'))
	{
		/* user.@REALM == user@REALM */
		cIdent->SeqBody[cIdent->SeqLen-1] = '\0';
		cIdent->SeqLen--;
	}
	cIdent->SeqLen++;	/* include \0 termination just because */

	LogMsg(0, AuthDebugLevel, stdout, "\tinstance = %s", cIdent->SeqBody);

	return(0);	/* success */
}
#endif	/* KERBEROS4 */

#ifdef KERBEROS5
/* The following will definitely go away with a config file! XXXX */
#define SRV5PRINC "host"
//#define SRV5PRINC "coda"

long Krb5Init(char *initrealm, char *keytab_filename)
{
	int retval;
	char thishost[MAXHOSTNAMELEN];
	struct hostent *authserver;
	int rc;

	LogMsg(0, AuthDebugLevel, stdout, "In Krb5Init()");

	rc = gethostname(thishost, MAXHOSTNAMELEN);
	if ( rc ) {
	    fprintf(stderr, "Krb5Init: Cannot find hostname of this machine\n");
	    return -1;
	}
	authserver = gethostbyname(thishost);
	if ( ! authserver ) {
		herror("Krb5Init: cannot do gethostbyname");
		return -1;
	}
	

	/* initialize Kerberos 5 contex */
	retval = krb5_init_context(&coda_auth_krb5context);
	if (retval) {
		com_err("auth2", retval, "while initializing krb5");
		exit(-1);
	}

	if (keytab_filename != NULL) {
	    retval = krb5_kt_resolve(coda_auth_krb5context,
				     keytab_filename,
				     &coda_keytab);
	    if (retval) {
		com_err("auth2", retval, "while setting keytab");
		exit(-1);
	    }
	}

	if (initrealm)
	    realm = strdup(initrealm);
    
	/* generate principal given the service name codaauth */
	retval = krb5_sname_to_principal(coda_auth_krb5context, authserver->h_name, 
					 SRV5PRINC,
					 KRB5_NT_SRV_HST, &coda_auth_krb5princ);

	if (retval) {
		com_err("auth2", retval, "while generating servicename");
		exit(-1);
	}
	 
	return 0; /* success */
}

long Krb5GetSecret(char *hostname, char **identity, int *identitylen, 
		   char **secret,   int *secretlen)
{
       	krb5_error_code retval;
	krb5_data authenticator;
	krb5_ccache ccdef;
	krb5_auth_context auth_context = NULL;
	krb5_keyblock *session_key;
	char *s;
	int i;
	struct hostent *authserver;

	authserver = gethostbyname(hostname);
	if ( ! authserver ) {
		herror("Krb5GetSecret:");
		return -1;
	}

	/* aquire cache of existing authenticators */
	retval = krb5_cc_default(coda_auth_krb5context, &ccdef);
	if (retval) {
		com_err("krbsupport", retval, "while getting default ccache");
		return(1);
	}

	/* construct a new authenticator, or find an old one */
	retval = krb5_mk_req(coda_auth_krb5context, &auth_context, 0, SRV5PRINC, 
			     authserver->h_name,
			     NULL, ccdef, &authenticator);
	if (retval) {
		com_err("krbsupport", retval, "while preparing AP_REQ");
		return(1);
	}

	/* authenticator.{length,data} now contains authenticator */
	
	/* pull out the shared secret for RPC2 to use in the bind */
	

	retval = krb5_auth_con_getkey(coda_auth_krb5context, auth_context,
				      &session_key);

	if (retval) {
		com_err("krbsupport", retval, "while retrieving session key");
		/*
		krb5_auth_con_free(coda_auth_krb5context, auth_context);
		*/
		return(1);
	}
		
	/* we now have the key in session_key -- hopefully ->length, ->data */

	/* this is a good key size for RPC2_KEYSIZE */
	s = malloc(RPC2_KEYSIZE);
	
	/* which is smaller? zero any remainder */
	for (i=0; i < min(RPC2_KEYSIZE, session_key->length) ; i++) {
		s[i] = session_key->contents[i];
	}

	if (RPC2_KEYSIZE > session_key->length) {
		/* zero the remainder of the key */
		fprintf(stderr, "krbsupport: Unexpectedly short kerberos key\n");
		for (i = session_key->length; i < RPC2_KEYSIZE; i++)
			s[i] = '\0';
	}

	*secret = s;
	*secretlen = RPC2_KEYSIZE;
	*identity = authenticator.data;
	*identitylen = authenticator.length;

	/* ok, have the key ready, and the authenticator, so free some stuff */
	
	krb5_auth_con_free(coda_auth_krb5context, auth_context);

	return 0;
}

long Krb5DoKinit()
{
	char *childargv[] = { "kinit", NULL };
	pid_t child;
	int rc;

	child = fork();
	if ( child == -1 ) {
		perror("fork: ");
		exit(1);
	}
	if ( child == 0 ) {  /*  in child */
		execvp(K5KINIT, childargv);
	}
	if ( child ) {
		waitpid(child, &rc, 0);
	}
	return 0;
}

long Krb5GetKeys(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
	krb5_error_code retval;
	krb5_auth_context auth_context = NULL;
	krb5_data authenticator;
	krb5_ticket *ticket = NULL;
	char *cp;
	char *at;
	int i;
	char thishost[MAXHOSTNAMELEN];
	struct hostent *authserver;
	int rc;

	LogMsg(0, AuthDebugLevel, stdout, "In Krb5GetKeys()");

	rc = gethostname(thishost, MAXHOSTNAMELEN);
	if ( rc ) {
		fprintf(stderr, "Krb5GetKeys: Cannot find hostname of this machine\n");
		return -1;
	}
	authserver = gethostbyname(thishost);
	if ( ! authserver ) {
		herror("Krb5GetKeys: cannot do gethostbyname");
		return -1;
	}
	

	authenticator.length = cIdent->SeqLen;
	authenticator.data = (krb5_pointer) cIdent->SeqBody;

	retval = krb5_rd_req(coda_auth_krb5context, &auth_context,
			     &authenticator, coda_auth_krb5princ,
			     coda_keytab, NULL, &ticket);
	
	if (retval) {
		/* some kind of error */
		com_err("auth2", retval, "while reading request");
		/* when do we free this 
		krb5_auth_con_free(coda_auth_krb5context, auth_context);
		*/
		if (ticket)
			krb5_free_ticket(coda_auth_krb5context, ticket);
		return(-1);
	}

	/* success authenticating someone, but who? */
	retval = krb5_unparse_name(coda_auth_krb5context,
				   ticket->enc_part2->client, &cp);
	if (retval) {
		/* this is a bad situation -- kerberos server should not generate
		   bad names in its authenticators? */
		com_err("auth2", retval, "while unparsing client name");
		krb5_auth_con_free(coda_auth_krb5context, auth_context);
		if (ticket)
			krb5_free_ticket(coda_auth_krb5context, ticket);
		return(-1);
	}

	/* now have a char* cp that contains the name of the principal */

	if (!realm) {
	    if ( krb5_get_default_realm(coda_auth_krb5context, &realm) ) {
		fprintf(stderr, "krbsupport: Can't get default realm\n");
		return -1; 
	    }
	}

	if (strncmp(realm, ticket->enc_part2->client->realm.data, 
		    ticket->enc_part2->client->realm.length)) {
		/* names differ */
		krb5_auth_con_free(coda_auth_krb5context, auth_context);
		free(cp);
		if (ticket)
			krb5_free_ticket(coda_auth_krb5context, ticket);
		return (-1);
	}
	/* have the correct realm, so can move along */
	/* null terminate boring principals */
	if (cp[strlen(cp)-1] == '/')
		cp[strlen(cp)-1] = '\0';

	if ((at = strchr(cp, '@')))
		*at = '\0';

	/* should be room as it first got to us via cIdent anyway */
	cIdent->SeqLen = strlen(cp)+1;
	strcpy(cIdent->SeqBody, cp);

	/* now prepare the keys */

	/* Kerberos should always generate a key at least as long as 8 chars */
	if (ticket->enc_part2->session->length >= RPC2_KEYSIZE) {
		/* kerberos session key is of sufficient length */
		for (i=0; i<RPC2_KEYSIZE; i++){
			hKey[i] = ticket->enc_part2->session->contents[i];
		}
	}
	else
        {
		/* kerberos session key is too short ? */
		LogMsg(00, AuthDebugLevel, stdout, "\tWARNING: Kerberos generated a short key!");
		for (i=0; i<ticket->enc_part2->session->length; i++)
			hKey[i] = ticket->enc_part2->session->contents[i];
	}

	/* generate RPC2 session key */
	for (i=0; i<RPC2_KEYSIZE; i++)
	{
		sKey[i] = rpc2_NextRandom(NULL);
	}

	LogMsg(0, AuthDebugLevel, stdout, "\tkrb5 identity: ", cIdent->SeqBody);
		
	if (ticket)
		krb5_free_ticket(coda_auth_krb5context, ticket);
	free(cp);
	krb5_auth_con_free(coda_auth_krb5context, auth_context);
	return(0);
}

#endif /* KERBEROS5 */






