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

static char *myuser;
static char mypasswd[10];

int main(int argc, char **argv)
{
	char *p;
	int insist;
	int flags;
	int c, pwlen;
	char *uname;
	char newpw[10];
	char buf[200];
	int ok, rc;
	char *Realm_or_Host = NULL;

	memset(newpw, 0, sizeof(newpw));
	memset(buf, 0, sizeof(buf));

	if (argc > 1) {
	    if (strcmp(argv[1], "-h") == 0) {
	        if (argc < 3) {
		    printf("Usage: %s [-h SCM-host-name]\n", argv[0]);
    		    exit(1);
 	        }

	        Realm_or_Host = argv[2];
	        argv += 2;
	        argc -= 2;
 	    }
	}
	
	insist = 0;
	if ((myuser = getenv("USER")) == NULL) {
		fprintf(stderr, "Who are you?  Set $USER.\n");
		exit(1);
	}
	if (argc < 2)
		uname = myuser;
	else
		uname = argv[1];

	/* Make sure our arrays don't overflow. */
	if (strlen(uname) > 20) {
	    fprintf(stderr, "User name is invalid.\n");
	    exit(1);
	}

	printf("Changing password for %s\n", uname);
/*
	if (U_InitRPC() != 0) {
		fprintf(stderr, "Internal error: RPC or vstab problems.\n");
		exit(1);
	}
*/
	U_InitRPC();
	if (strcmp(uname, myuser) == 0)
		strcpy(mypasswd, getpass("Old password:"));
	else
		strcpy(mypasswd, getpass("Your password:"));
tryagain:
	sprintf(buf, "New password for %s:", uname);
	memset(newpw, 0, sizeof(newpw));
	strcpy(newpw, getpass(buf));
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

	if (strcmp(newpw, getpass("Retype new password:")) != 0) {
		printf("Mismatch - password unchanged.\n");
		exit(1);
	}
	rc = U_ChangePassword (Realm_or_Host, uname, newpw, AUTH_METHOD_CODAUSERNAME, myuser, strlen(myuser)+1, mypasswd, strlen(mypasswd));
	switch(rc) {
	    case RPC2_DEAD:
		printf("Server to change passwords down, try again later\n");
		break;
	    case AUTH_DENIED:
		printf("%s not authenticated to change passwd for %s\n", myuser, uname);
		break;
	    case AUTH_SUCCESS:
		printf("Password changed, it will be in effect in about 1 hour\n");
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
