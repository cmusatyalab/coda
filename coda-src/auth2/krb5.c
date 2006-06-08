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

/* krb5.c -- kerberos 5 support routines for Coda */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <krb5.h>
#include <com_err.h>

#include <codaconf.h>
#include <getsecret.h>
#include "krbcommon.h"

#include <ctype.h>

#ifdef __cplusplus
}
#endif

/* The following can be specified/overridden using /etc/coda/auth.conf */
static char *kerberos5kinit;   /* client only, defaults to "kinit" */
static char *kerberos5service; /* both, defaults to "host/%s" */
/* MUST contain "%s", for usual DCE setup it would be "host/%s/self" */
static char *kerberos5realm;   /* both, defaults to krb5_default_realm() */
static char *kerberos5keytab;  /* server only */

static krb5_context   krb5context;
static krb5_principal krb5principal;
static krb5_keytab    krb5keytab = NULL;

#ifdef HAVE_HEIMDAL
#define tkt_realm(t)    ((t)->client->realm)
#define tkt_realmlen(t) (strlen((t)->client->realm))
#define tkt_client(t)   ((t)->client)
#define tkt_key(t)      (&(t)->ticket.key)
#define key_data(k)     ((k)->keyvalue.data)
#define key_length(k)   ((k)->keyvalue.length)
#else
#define tkt_realm(t)    ((t)->enc_part2->client->realm.data)
#define tkt_realmlen(t) ((t)->enc_part2->client->realm.length)
#define tkt_client(t)   ((t)->enc_part2->client)
#define tkt_key(t)      ((t)->enc_part2->session)
#define key_data(k)     ((k)->contents)
#define key_length(k)   ((k)->length)
#endif

static int Krb5CommonInit(void)
{
    krb5_error_code krc;

    codaconf_init("venus.conf");
    codaconf_init("vice.conf");
    codaconf_init("auth2.conf");
    CODACONF_STR(kerberos5service, "kerberos5service", "host/%s");

    /* initialize Kerberos 5 contex */
    krc = krb5_init_context(&krb5context);
    if (krc) {
        com_err("Krb5CommonInit", krc, "while initializing krb5");
        return -1;
    }

    return 0; /* success */
}

int Krb5ClientInit(void)
{
    if (Krb5CommonInit())
        return -1;

    CODACONF_STR(kerberos5kinit, "kerberos5kinit", "kinit")
    return 0;
}

/* pin@math.chalmers.se, we have to construct our own full principal name
 * and use krb5_parse_name instead of krb5_sname_to_princ to allow for
 * multilevel principals. */
/* Either succeeds or exits the application */
static void get_principal(char *hostname, krb5_principal *principal)
{
    krb5_error_code krc;
    char **realm_list;
    char *host = NULL;
    char *principalname = NULL;
    int i, n, servicelen, havepct;

    host = krb_canonicalize_host(hostname);
    if (!host) {
	fprintf(stderr, "krb5.c: krb_canonicalize_host failed\n");
	exit(-1);
    }

    /* kerberos seems to be case-sensitive in it's tickets, but relies
     * on case-insensitive names from DNS. This is a hack from Michael
     * Tautschnig which canonicalizes the name we just got. */
    if (host) {
	n = strlen(host);
	for (i = 0; i < n; i++)
	    host[i] = toupper(host[i]);
    }

    /* what is the realm? */
    krc = krb5_get_host_realm(krb5context, host, &realm_list);
    if (krc) {
	com_err("krb5.c", krc, "while looking for authserver realm");
	exit(-1);
    }

    servicelen = strlen(kerberos5service);
    principalname = malloc(servicelen + strlen(host) +
			   strlen(realm_list[0]) + 2 - 2);
		    /* +2 is for '@' and '\0' */
		    /* -2 is for "%s" */

    if (!principalname) {
	fprintf(stderr, "krb5.c: failed to allocate space for principalname\n");
	exit(-1);
    }

    /* make sure we have only a single '%s' substitution */
    havepct = 0;
    for (i = 0; i < servicelen; i++) {
	if (kerberos5service[i] != '%') continue;
	havepct++;
	if (kerberos5service[i+1] != 's') break;
    }
    if (havepct != 1 || i != servicelen) {
	fprintf(stderr, "Badly formatted kerberos5service, need exactly one '%%s' subsitition\n");
	exit(-1);
    }

    n = sprintf(principalname, kerberos5service, host);
    sprintf(principalname + n, "@%s", realm_list[0]);

    krc = krb5_parse_name(krb5context, principalname, principal);
    if (krc) {
        com_err("krb5.c", krc, "while generating servicename");
	exit(-1);
    }

    free(principalname);
    krb5_free_host_realm(krb5context, realm_list);
    free(host);
}

int Krb5ServerInit(void)
{
    krb5_error_code krc;

    if (Krb5CommonInit())
        return -1;

    CODACONF_STR(kerberos5realm, "kerberos5realm", NULL)

    /* When no realm has been specified, use the default realm */
    if (!kerberos5realm) {
        if (krb5_get_default_realm(krb5context, &kerberos5realm)) {
            fprintf(stderr, "krb5.c: Can't get default realm\n");
            return -1; 
        }
    }

    get_principal(NULL, &krb5principal);

    CODACONF_STR(kerberos5keytab, "kerberos5keytab", NULL)

    /* If the user specified a different keytab, load it */
    if (kerberos5keytab) {
        krc = krb5_kt_resolve(krb5context, kerberos5keytab, &krb5keytab);
        if (krc) {
            com_err("krb5.c", krc, "while setting keytab");
            return -1;
        }
    }
    return 0;
}

static int do_GetSecret(char *hostname, char **identity, int *ilen, 
                 char **secret, int *slen)
{
    krb5_error_code krc;
    krb5_auth_context auth_context = NULL;
    krb5_principal principal;

    krb5_data authenticator;
    krb5_ccache ccdef;
    krb5_keyblock *session_key;
    krb5_creds creds, *credsp = NULL;
    int rc = -1;

    /* aquire cache of existing authenticators */
    krc = krb5_cc_default(krb5context, &ccdef);
    if (krc) {
	com_err("krb5.c", krc, "while getting default ccache");
	goto out;
    }

    /* pin@math.chalmers.se, we can't use krb5_mk_req if we want to allow for
     * multilevel principals */
    get_principal(hostname, &principal);
    
    /* obtain ticket and session key */
    memset(&creds, 0, sizeof(creds));
    krc = krb5_copy_principal(krb5context, principal, &creds.server);
    if (krc) goto cleanup_principal;

    krc = krb5_cc_get_principal(krb5context, ccdef, &creds.client);
    if (krc) goto cleanup;

    krc = krb5_get_credentials(krb5context, 0, ccdef, &creds, &credsp);
    if (krc) goto cleanup;

    krc = krb5_mk_req_extended(krb5context, &auth_context, 0, NULL, credsp,
			       &authenticator);
cleanup:
    if (credsp)
	krb5_free_creds(krb5context, credsp);
    krb5_free_cred_contents(krb5context, &creds);
cleanup_principal:
    krb5_free_principal(krb5context, principal);

    if (krc) {
	com_err("krb5.c", krc, "while preparing AP_REQ");
	goto out;
    }

    /* authenticator.{length,data} now contains authenticator */
    /* pull out the shared secret for RPC2 to use in the bind */

    krc = krb5_auth_con_getkey(krb5context, auth_context, &session_key);
    if (krc) {
	com_err("krb5.c", krc, "while retrieving session key");
	goto out;
    }

    /* we now have the key in session_key -- hopefully ->length, ->data */
    HashSecret(key_data(session_key), key_length(session_key), *secret);
    *slen = RPC2_KEYSIZE;

    *identity = authenticator.data;
    *ilen = authenticator.length;
    rc = 0;

out:
    if (auth_context)
	krb5_auth_con_free(krb5context, auth_context);

    return rc;
}

int Krb5GetSecret(char *hostname, char **identity, int *ilen, 
                  char **secret, int *slen, int interactive)
{
    int rc;
    rc = do_GetSecret(hostname, identity, ilen, secret, slen);
    if (!rc || !interactive) return rc;

    krb_fork_kinit(kerberos5kinit);
    return do_GetSecret(hostname, identity, ilen, secret, slen);
}

int Krb5Validate(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
    krb5_error_code krc;
    krb5_auth_context auth_context = NULL;
    krb5_ticket *ticket = NULL;
    char *cp = NULL;

    krb5_data authenticator;
    char *at;
    int rc = -1;

    authenticator.length = cIdent->SeqLen;
    authenticator.data = (krb5_pointer) cIdent->SeqBody;

    krc = krb5_rd_req(krb5context, &auth_context, &authenticator,
		      krb5principal, krb5keytab, NULL, &ticket);
    if (krc) {
	/* some kind of error */
	com_err("Krb5Validate", krc, "while reading request");
	goto out;
    }

    /* Check whether the realm is correct */
    if (strncmp(tkt_realm(ticket), kerberos5realm, tkt_realmlen(ticket))) {
	/* names differ */
	fprintf(stderr, "incorrect realm in ticket\n");
	goto out;    
    }

    /* success authenticating someone, but who? */
    krc = krb5_unparse_name(krb5context, tkt_client(ticket), &cp);
    if (krc) {
	/* this is a bad situation -- kerberos server should not generate
	   bad names in its authenticators? */
	com_err("Krb5Validate", krc, "while unparsing client name");
	goto out;    
    }

    /* now have a char* cp that contains the name of the principal */
    /* null terminate boring principals */
    if (cp[strlen(cp)-1] == '/')
	cp[strlen(cp)-1] = '\0';

    if ((at = strchr(cp, '@')))
	*at = '\0';

    /* should be room as it first got to us via cIdent anyway */
    cIdent->SeqLen = strlen(cp)+1;
    strcpy(cIdent->SeqBody, cp);

    /* now prepare the keys */

    /* hKey is the md5 hash of the kerberos session secret */
    HashSecret(key_data(tkt_key(ticket)), key_length(tkt_key(ticket)), hKey);

    /* sKey is a random sequence of bytes */
    GenerateSecret(sKey);

    rc = 0;

out:
    if (cp)
	free(cp);

    if (ticket)
	krb5_free_ticket(krb5context, ticket);

    if (auth_context)
	krb5_auth_con_free(krb5context, auth_context);

    return rc;
}
