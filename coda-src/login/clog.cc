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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/login/clog.cc,v 4.2 1997/02/26 16:02:44 rvb Exp $";
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

	log -- tell Venus your password

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <string.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <auth2.h>


int main(int argc, char **argv)
{
    EncryptedSecretToken    sToken;
    ClearToken		    cToken;
    struct passwd	    pwent;
    struct passwd	    *pw = &pwent;
    static char		    passwd[100];
    long		    rc;

    bzero(passwd, sizeof(passwd));
    if (argc < 2) {
	pw = getpwuid (getuid ());
	if (pw == NULL) {
	    fprintf (stderr, "Can't figure out your user id.\n");
	    fprintf (stderr, "Try \"clog user\"\n");
	    exit (1);
	}
    }
    else if (argc == 2 && strcmp(argv[1],"-x") != 0) {
	pw = getpwnam (argv[1]);
	if (pw == NULL) {
	    fprintf (stderr, "%s not a valid user.\n", argv[1]);
	    exit (1);
	}
    }
    else if (argc == 3) {
	if (strcmp(argv[1],"-x") == 0)
	    pw->pw_name = argv[2];
	else {
	    pw = getpwnam (argv[1]);
	    if (pw == NULL) {
		fprintf (stderr, "%s not a valid user.\n", argv[1]);
	        exit (1);
	    }
	    strcpy(passwd, argv[2]);
	    bzero((char *)argv[2], strlen(passwd));
	}
    }
    else if (argc == 4 && strcmp(argv[1],"-x") == 0) {
	pw->pw_name = argv[2];
	strcpy(passwd, argv[3]);
	bzero((char *)argv[3], strlen(passwd));
    }
    else {
	fprintf (stderr, "Usage: log [[-x] user [password]]\n");
	exit (1);
    }

    /* set flags to disable cs name resolver */

    U_InitRPC();
    if (passwd[0] == '\0') strcpy (passwd, getpass ("Password: "));
    rc = U_Authenticate(pw->pw_name, passwd, &cToken, sToken);
    if (rc != 0) {
	fprintf (stderr, "Invalid login (%s).\n", RPC2_ErrorMsg(rc));
	exit (1);
    }
    if(U_SetLocalTokens(0, &cToken, sToken))
	printf("Local login only, could not contact venus\n");
    exit(0);
}
