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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <pwd.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif __MACH__
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#include <errno.h>
#include <string.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <auth2.h>


int main(int argc, char **argv)
{
    EncryptedSecretToken    sToken;
    ClearToken		    cToken;
    EncryptedSecretToken    testSTok;
    ClearToken		    testCTok;
    struct passwd	    *pw;
    static char		    passwd[100] = { '\0' };

    if (argc < 2) {
	pw = getpwuid (getuid ());
	if (pw == NULL) {
	    fprintf (stderr, "Can't figure out your user id.\n");
	    fprintf (stderr, "Try \"log user\"\n");
	    exit (1);
	}
    }
    else if (argc == 2 || argc == 3) {
	pw = getpwnam (argv[1]);
	if (pw == NULL) {
	    fprintf (stderr, "%s not a valid user.\n", argv[1]);
	    exit (1);
	}
	if (argc == 3) {
	    strcpy(passwd, argv[2]);
	    bzero((char *)&argv[2], strlen(passwd));
	}
    }
    else {
	fprintf (stderr, "Usage: log [user [password]]\n");
	exit (1);
    }

/*
    if (U_InitRPC() != 0) {
	fprintf (stderr, "%s: problems with RPC.\n", argv[0]);
	exit (1);
    }
*/
    U_InitRPC();

    if (passwd[0] == '\0') strcpy (passwd, getpass ("Password: "));
    if ((U_Authenticate(pw->pw_name, passwd, &cToken, sToken)) != 0) {
	fprintf (stderr, "Invalid login.\n");
	exit (1);
    }

    printf ("Sending token to venus\n");
    if (U_SetLocalTokens(0, &cToken, sToken) < 0)
	perror("U_SetLocalTokens");
    printf("Getting tokens back from venus\n");
    if (U_GetLocalTokens(&testCTok, testSTok) < 0)
	perror("U_GetLocalTokens");

    printf("Comparing clear token\n");
    if(bcmp((char *)&cToken,(char *)&testCTok,sizeof(ClearToken)) != 0) {
	printf("Bad ClearToken\n");
    }
    printf("Comparing secret token\n");
    if(bcmp((char *)sToken,(char *)testSTok,sizeof(EncryptedSecretToken)) != 0) {
	printf("Bad SecretToken\n");
    }
    printf("Done ! ! ! !\n");
    exit(0);
}
