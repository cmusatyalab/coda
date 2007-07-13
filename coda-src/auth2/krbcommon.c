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

/* krbcommon.c -- common support support routines for kerberos */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "krbcommon.h"

#ifdef __cplusplus
}
#endif

char *krb_canonicalize_host(const char *host)
{
    char thishost[MAXHOSTNAMELEN];
    struct hostent *hent;
    int ret;

    if (!host) {
	ret = gethostname(thishost, MAXHOSTNAMELEN);
	if (ret) {
	    fprintf(stderr, "canonicalize_host failed\n");
	    return NULL;
	}
	host = thishost;
    }

    hent = gethostbyname(host);
    if (!hent) {
	herror("canonicalize_host: gethostbyname failed");
	return NULL;
    }

    /* We need to copy the hostname because krb5 sometimes uses the
     * resolver as well and we would lose this name */
    return strdup(hent->h_name);
}

void krb_fork_kinit(const char *kinit)
{
    char *childargv[] = { "kinit", NULL };
    pid_t child;
    int rc;

    child = fork();
    if ( child == -1 ) {
	perror("fork: ");
	return;
    }

    if ( child == 0 ) /*  in child */
	execvp(kinit, childargv);
    else
	waitpid(child, &rc, 0);

    return;
}

