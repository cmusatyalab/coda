#ifndef _BLURB_
#define _BLURB_
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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/login/cpasswd.cc,v 1.2 1997/01/07 18:40:47 rvb Exp";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

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
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <auth2.h>

PRIVATE char *myuser;
PRIVATE char mypasswd[10];

extern char *DefAuthHost;

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

	if (argc > 1) {
	    if (strcmp(argv[1], "-h") == 0) {
	        if (argc < 3) {
		    printf("Usage: %s [-h SCM-host-name]\n", argv[0]);
    		    exit(1);
 	        }

	        DefAuthHost = argv[2];
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
	while (c = *p++) {
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
	rc = U_ChangePassword (uname, newpw, myuser, mypasswd);
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
