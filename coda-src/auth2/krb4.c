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

/* krb4.c -- kerberos 4 support routines for Coda */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <des.h>
#include <krb.h>

#include <codaconf.h>
#include <getsecret.h>
#include "krbcommon.h"

#ifdef __cplusplus
}
#endif __cplusplus

/* The following can be specified/overridden using /etc/coda/auth.conf */
static char *kerberos4kinit;   /* defaults to "kinit" */
static char *kerberos4service; /* defaults to "rcmd" */
static char *kerberos4realm;

static int Krb4CommonInit(void)
{
    char default_realm[REALM_SZ];

    codaconf_init("venus.conf");
    codaconf_init("vice.conf");
    codaconf_init("auth2.conf");
    CONF_STR(kerberos4service, "kerberos4service", "host");

    CONF_STR(kerberos4realm, "kerberos4realm", NULL)
    /* When no realm has been specified, use the default realm */
    if (!kerberos4realm) {
        krb_get_lrealm(default_realm, 1);
	kerberos4realm = strdup(default_realm);
    }
    return 0; /* success */
}

int Krb4ClientInit(void)
{
    if (Krb4CommonInit())
        return -1;

    CONF_STR(kerberos4kinit, "kerberos4kinit", "kinit")
    return 0;
}

int Krb4ServerInit(void)
{
    if (Krb4CommonInit())
        return -1;
    return 0;
}

int do_GetSecret(char *hostname, char **identity, int *ilen, 
                 char **secret, int *slen)
{
    struct ktext *authenticator = NULL;
    struct credentials *cred = NULL;
    char *host = NULL;
    int rc = -1;

    host = krb_canonicalize_host(hostname);
    if (!host) return -1;

    cred = malloc(sizeof(struct credentials));
    if (!cred) {
	fprintf(stderr, "krb4: malloc(credentials) failed\n");
	goto out;
    }

    authenticator = malloc(sizeof(struct ktext));
    if (!authenticator) {
	fprintf(stderr, "krb4: malloc(ktext) failed\n");
	goto out;
    }

    /* construct a new authenticator, or find an old one */
    rc = krb_mk_req(authenticator, kerberos4service, host, kerberos4realm, 0);
    if (rc) {
	fprintf(stderr, "krb4: %s (%d)\n", krb_err_txt[rc], rc);
	rc = 1;
	goto out;
    }

    /* authenticator now contains authenticator */
    /* pull out the shared secret for RPC2 to use in the bind */

    rc = krb_get_cred(kerberos4service, host, kerberos4realm, cred);
    if (rc) {
	fprintf(stderr, "krb4: %s (%d)\n", krb_err_txt[rc], rc);
	rc = 1;
	goto out;
    }

    /* we now have the key in session_key */
    HashSecret(cred->session, 8, *secret);
    *slen = RPC2_KEYSIZE;

    *identity = (char *)authenticator;
    *ilen = sizeof(struct ktext);

    rc = 0;

    authenticator = NULL; /* XXX ok, who is supposed to be freeing the
			     authenticator now? -JH */
out:
    if (host)
	free(host);

    if (authenticator)
	free(authenticator);

    if (cred)
	free(cred);

    return rc;
}

int Krb4GetSecret(char *hostname, char **identity, int *ilen, 
                  char **secret, int *slen, int interactive)
{
    int rc;
    rc = do_GetSecret(hostname, identity, ilen, secret, slen);
    if (!interactive) return rc;

    krb_fork_kinit(kerberos4kinit);
    return do_GetSecret(hostname, identity, ilen, secret, slen);
}

int Krb4Validate(RPC2_CountedBS * cIdent, RPC2_EncryptionKey hKey, RPC2_EncryptionKey sKey)
{
    struct ktext *authenticator;
    struct auth_dat ticket;
    char *host = NULL;
    int rc = -1;

    if (cIdent->SeqLen != sizeof(struct ktext)) {
	fprintf(stderr, "Krb4Validate: cIdent too small to be authenticator");
	return -1;
    }

    host = krb_canonicalize_host(NULL);
    if (!host) return -1;

    authenticator = (struct ktext *)cIdent->SeqBody;

    rc = krb_rd_req(authenticator, kerberos4service, host, 0, &ticket, "");
    free(host);
    if (rc) {
	/* some kind of error */
	fprintf(stderr, "Krb4Validate: %s (%d)\n", krb_err_txt[rc], rc);
	return -1;
    }

    /* Check whether the realm is correct */
    if (strncmp(ticket.prealm, kerberos4realm, REALM_SZ)) {
	/* names differ */
	fprintf(stderr, "Krb4Validate: incorrect realm in ticket\n");
	/* do we have to clean up the ticket?? -JH */
	return -1;
    }

    /* Copy kerberos name back into cIdent. There should be room as it first
     * got to us via cIdent anyway */

    /* but let's make sure... */
    assert(ANAME_SZ + INST_SZ + 1 <= cIdent->SeqLen);

    snprintf(cIdent->SeqBody, cIdent->SeqLen, "%s.%s", ticket.pname, ticket.pinst);
    cIdent->SeqLen = strlen(cIdent->SeqBody);
    if (cIdent->SeqLen && cIdent->SeqBody[cIdent->SeqLen-1] == '.') {
	/* user.@realm == user@realm */
	cIdent->SeqLen--;
	cIdent->SeqBody[cIdent->SeqLen] = '\0';
    }
    cIdent->SeqLen++; /* include trailing '\0' */

    /* now prepare the keys */

    /* hKey is the md5 hash of the kerberos session secret */
    HashSecret(ticket.session, RPC2_KEYSIZE, hKey);

    /* sKey is a random sequence of bytes */
    GenerateSecret(sKey);

    return 0;
}
