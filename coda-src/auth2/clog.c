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
#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <lwp.h>
#include <rpc2.h>
#include <auth2.h>

#include "avenus.h"
#include "auser.h"

#ifdef __cplusplus
}
#endif __cplusplus



void printusage(void)
{
		    fprintf(stderr, "Usage clog [-pipe] [-test] [-host authserver]"
			    " [{-kerberos4,-kerberos5,-coda}] [username]\n");
}

int main(int argc, char **argv)
{
    EncryptedSecretToken    sToken;
    RPC2_Integer            authmethod = AUTH_METHOD_CODAUSERNAME;
    int                     passwdpipe = 0;
    int                     interactive = 1;
    ClearToken		    cToken;
    EncryptedSecretToken    testSTok;
    ClearToken		    testCTok;
    struct passwd	    pwent;
    struct passwd	    *pw = &pwent;
    char *hostname=NULL;
    char *username=0;
    long		    rc;
    int i;
    int testing = 0;

    pw = getpwuid (getuid ());
    if (pw) {
        username=pw->pw_name;
    }

    i = 1;
    while (i < argc) {
	    if (U_GetAuthMethod(argv[i], &authmethod)) {
		    i++;
		    continue;
	    } else if ( strcmp(argv[i], "-test") == 0 ) {
		    testing =1;
		    i++;
	    } else if ( strcmp(argv[i], "-nokinit") == 0 ) {
		    interactive = 0;
		    i++;
	    }  else if ( strcmp(argv[i], "-pipe") == 0 ) {
		    passwdpipe =1;
		    i++;
	    } else if ( strcmp(argv[i], "-host") == 0) {
		    i++;
		    if (i >= argc) {
			    fprintf(stderr, "Missing host\n");
			    printusage();
			    exit(1);
		    }
		    hostname = argv[i];
		    i++;
	    } else if ( i == argc-1 ) {
                    username = argv[i];
                    i++;
	    }
	    /* still coming: 
	       -e ".............": newpag, authenticate, exec
	       -u unsafe, tell Venus to use uid instead of pag.
	    */
	    else {
		    fprintf(stderr, "Wrong argument: %s\n", argv[i]);
		    printusage();
		    exit(1);
	    }

    }
    
    if (username == NULL) {
	    fprintf (stderr, "Can't figure out your user id.\n");
	    fprintf (stderr, "Try \"clog user\"\n");
	    exit (1);
    }


    U_InitRPC();

    rc = U_Authenticate(hostname, authmethod, username, 
			strlen(username)+1, &cToken, sToken, passwdpipe, 
			interactive);

    if (rc != 0) {
	fprintf (stderr, "Invalid login (%s).\n", RPC2_ErrorMsg(rc));
	exit (1);
    }

    if (testing)
	    printf ("Sending token to venus\n");
    if(U_SetLocalTokens(0, &cToken, sToken))
	printf("Local login only, could not contact venus\n");
    
    if (testing) {
	    printf("Getting tokens back from venus\n");
	    if (U_GetLocalTokens(&testCTok, testSTok) < 0)
		    perror("U_GetLocalTokens");
	    
	    printf("Comparing clear token\n");
	    if(bcmp((char *)&cToken,(char *)&testCTok,sizeof(ClearToken)) != 0) {
		    printf("Bad ClearToken\n");
	    }
	    printf("Comparing secret token\n");
	    if(bcmp((char *)sToken,(char *)testSTok,
		    sizeof(EncryptedSecretToken)) != 0) {
	    printf("Bad SecretToken\n");
	    }

	    fprintf(stderr,"net order:\n");		
	    fprintf(stderr,"\tAuthHandle = %ld\n",testCTok.AuthHandle);
	    fprintf(stderr,"\tViceId = %ld\n",testCTok.ViceId);
	    fprintf(stderr,"\tBeginTimestamp = %ld\n",testCTok.BeginTimestamp);
	    fprintf(stderr,"\tEndTimestamp = %ld\n",testCTok.EndTimestamp);
	    U_NetToHostClearToken(&testCTok);
	    fprintf(stderr,"host order:\n");
	    fprintf(stderr,"\tAuthHandle = %ld\n",testCTok.AuthHandle);
	    fprintf(stderr,"\tViceId = %ld\n",testCTok.ViceId);
	    fprintf(stderr,"\tBeginTimestamp = %ld\n",testCTok.BeginTimestamp);
	    fprintf(stderr,"\tEndTimestamp = %ld\n",testCTok.EndTimestamp);

	    printf("Done ! ! ! !\n");
    }
    return 0;
}
