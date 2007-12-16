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
	Change a user's password by talking to the mini-authentication servers.
	Supply your own password to the "Old password: " prompt, even if you're
	changing someone else's password.

	There may not seem to be much left, but this routine started out as
	the standard passwd routine.

*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include "auth2.h"
#include "auser.h"
#include <prs.h>
#include "auth2.common.h"
#ifdef __cplusplus
}
#endif

#include <auth2.h>
#include "parse_realms.h"
#include "codaconf.h"

int main(int argc, char **argv)
{
	char *p;
	int insist;
	int flags;
	int c, pwlen;
	char oldpw[128], newpw[128];
	int ok, rc;
	char *username = NULL;
	const char *realm = NULL;
	char *host = NULL;
	struct RPC2_addrinfo *srvs;

	if (argc > 1) {
	    if (strcmp(argv[1], "-h") == 0) {
	        if (argc < 3) {
		    printf("Usage: %s [-h SCM-host-name] [coda-user-name][@realm]\n", argv[0]);
    		    exit(1);
 	        }

	        host = argv[2];
	        argv += 2;
	        argc -= 2;
 	    }
	}
	
	insist = 0;
	if (argc >= 2) {
		username = argv[1];
		SplitRealmFromName(username, &realm);
	}

	if (!username || *username == '\0') {
#ifdef __CYGWIN32__
	    username = getlogin();   
#else
	    struct passwd *pw = getpwuid(geteuid());
	    if (pw) {
		username=pw->pw_name;
	    }
#endif
	}

	codaconf_init("venus.conf");
	codaconf_init("auth2.conf");

	CODACONF_STR(realm, "realm", NULL);

	if (!username || !realm) {
	    fprintf (stderr, "Can't figure out your username or realm.\n");
	    fprintf (stderr, "Try \"cpasswd user[@realm]\"\n");
	    exit (1);
	}

	/* Make sure our arrays don't overflow. */
	if (strlen(username) > 20) {
	    fprintf(stderr, "User name is invalid.\n");
	    exit(1);
	}

	printf("Changing password for %s@%s\n", username, realm);
/*
	if (U_InitRPC() != 0) {
		fprintf(stderr, "Internal error: RPC or vstab problems.\n");
		exit(1);
	}
*/
	U_InitRPC();
	strncpy(oldpw, getpass("Old password: "), sizeof(oldpw)-1);
tryagain:
	memset(newpw, 0, sizeof(newpw));
	strncpy(newpw, getpass("New password: "), sizeof(newpw)-1);
	pwlen = strlen(newpw);
	if (pwlen == 0) {
		printf("Password unchanged.\n");
		exit(1);
	}

	/*
	 * Insure password is of reasonable length and
	 * composition.  If we really wanted to make things
	 * sticky, we could check the dictionary for common
	 * words, but then things would really be slow.
	 */
	ok = 0;
	flags = 0;
	p = newpw;
	while (( c = *p++) ) {
		if (c >= 'a' && c <= 'z')
			flags |= 2;
		else if (c >= 'A' && c <= 'Z')
			flags |= 4;
		else if (c >= '0' && c <= '9')
			flags |= 1;
		else
			flags |= 8;
	}
	if (flags >= 7 && pwlen >= 4)
		ok = 1;
	if ((flags == 2 || flags == 4) && pwlen >= 6)
		ok = 1;
	if ((flags == 3 || flags == 5 || flags == 6) && pwlen >= 5)
		ok = 1;
	if (!ok && insist < 2) {
		printf("Please use %s.\n", flags == 1 ?
			"at least one non-numeric character" :
			"a longer password");
		insist++;
		goto tryagain;
	}

	if (strcmp(newpw, getpass("Retype new password: ")) != 0) {
		printf("Mismatch - password unchanged.\n");
		exit(1);
	}
	srvs = U_GetAuthServers(realm, host);
	rc = U_ChangePassword (srvs, username, newpw, AUTH_METHOD_CODAUSERNAME, username, strlen(username)+1, oldpw, strlen(oldpw)+1);
	RPC2_freeaddrinfo(srvs);

	switch(rc) {
	    case RPC2_DEAD:
		printf("Server to change passwords down, try again later\n");
		break;
	    case AUTH_DENIED:
		printf("authentication failed, unable to change passwd for %s\n", username);
		break;
	    case AUTH_SUCCESS:
		printf("Password changed\n");
		break;
	    case AUTH_BADKEY:
		printf("Bad new password. Try again\n");
		break;
	    case AUTH_READONLY:
		printf("Auth server is read-only\n");
		break;
	    case AUTH_FAILED:
		printf("Auth failed\n");
		break;
	    case RPC2_NOTAUTHENTICATED:
		printf("bind failed; user unauthenticated\n");
		break;
	    default:
		printf("Authentication Failed: %s\n", RPC2_ErrorMsg(rc));
	}
	fflush(stdout);
	exit(0);
}
