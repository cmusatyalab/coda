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
#include "tokenfile.h"

#ifdef __cplusplus
}
#endif __cplusplus

#ifndef O_BINARY
#define O_BINARY 0
#endif

void printusage(void)
{
		    fprintf(stderr,
                            "Usage clog [-pipe] [-test] [-host authserver]"
                            "[{-kerberos4,-kerberos5,-coda}]\n\t"
                            "[-tofile <file>] [-fromfile <file>] [username]\n");
}

int main(int argc, char **argv)
{
    EncryptedSecretToken    sToken;
/* Make intelligent default decisions, depending on how we were built..
		-- Troy <hozer@drgw.net> */
#if defined(KERBEROS5)
    RPC2_Integer            authmethod = AUTH_METHOD_KERBEROS5;
#else 
#if defined(KERBEROS4)
    RPC2_Integer            authmethod = AUTH_METHOD_KERBEROS4;
#else
    RPC2_Integer            authmethod = AUTH_METHOD_CODAUSERNAME;
#endif
#endif
    int                     passwdpipe = 0;
    int                     interactive = 1;
    ClearToken		    cToken;
    EncryptedSecretToken    testSTok;
    ClearToken		    testCTok;
    struct passwd	    *pw;
    char *hostname=NULL;
    char *username=NULL;
    long		    rc;
    int i;
    int testing = 0;
    char *tofile   = NULL;
    char *fromfile = NULL;

#ifdef __CYGWIN32__
    username = getlogin();	 
#elif defined(DJGPP)
    __djgpp_set_quiet_socket(1);	
#else
    pw = getpwuid (getuid ());
    if (pw) {
        username=pw->pw_name;
    }
#endif

    i = 1;
    while (i < argc) {
	    if (U_GetAuthMethod(argv[i], &authmethod)) {
		    i++;
		    continue;
	    } else if ( strcmp(argv[i], "-?") == 0 ||
                        strcmp(argv[i], "-h") == 0 ||
                        strcmp(argv[i], "--help") == 0 ) {
		    printusage();
		    exit(0);
	    } else if ( strcmp(argv[i], "-test") == 0 ) {
		    testing =1;
		    i++;
	    } else if ( strcmp(argv[i], "-nokinit") == 0 ) {
		    interactive = 0;
		    i++;
	    }  else if ( strcmp(argv[i], "-pipe") == 0 ) {
		    passwdpipe =1;
		    i++;
	    }  else if ( strcmp(argv[i], "-tofile") == 0 ) {
		    i++;
		    if (i >= argc) {
			    fprintf(stderr, "Missing file to write token to\n");
			    printusage();
			    exit(1);
		    }
                    tofile = argv[i++];
	    }  else if ( strcmp(argv[i], "-fromfile") == 0 ) {
		    i++;
		    if (i >= argc) {
			    fprintf(stderr,"Missing file to read token from\n");
			    printusage();
			    exit(1);
		    }
                    fromfile = argv[i++];
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

    if (!isatty(0)) passwdpipe = 1;

    if (passwdpipe == 0)
	printf("username: %s\n", username);

    U_InitRPC();

    if (fromfile) {
        ReadTokenFromFile(fromfile, &cToken, sToken);
    } else {
        rc = U_Authenticate(hostname, authmethod, username, 
                            strlen(username)+1, &cToken, sToken, passwdpipe, 
                            interactive);
        if (rc != 0) {
            fprintf (stderr, "Invalid login (%s).\n", RPC2_ErrorMsg(rc));
            exit (1);
        }
    }

    if (testing)
	    printf ("Sending token to venus\n");

    if (tofile) {
        WriteTokenToFile(tofile, &cToken, sToken);
    }

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
