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

	log -- tell Venus your password

*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <auth2.h>

#include "avenus.h"
#include "auser.h"
#include "tokenfile.h"
#include "codaconf.h"

#ifdef __cplusplus
}
#endif

#include <parse_realms.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

void printusage(void)
{
		    fprintf(stderr,
                            "Usage clog [-q] [-test] [-h authserver]"
                            "\t[{-kerberos4,-kerberos5,-coda}]\n"
                            "\t[-tofile <file>] [-fromfile <file>]\n"
			    "\t[-as username] [Coda username]\n");
}

int main(int argc, char **argv)
{
    EncryptedSecretToken    sToken;
    RPC2_Integer            authmethod = AUTH_METHOD_CODAUSERNAME;
    int                     verbose = 1;
    int                     interactive = 1;
    ClearToken		    cToken;
    EncryptedSecretToken    testSTok;
    ClearToken		    testCTok;
    char *hostname=NULL;
    char *username=NULL;
    char *realm = NULL;
    long rc;
    int i;
    int testing = 0;
    char *tofile   = NULL;
    char *fromfile = NULL;
    char *runas = NULL;

/* Make intelligent default decisions, depending on how we were built..
		-- Troy <hozer@drgw.net> */
#ifdef HAVE_KRB4
    authmethod = AUTH_METHOD_KERBEROS4;
#endif
#ifdef HAVE_KRB5
    authmethod = AUTH_METHOD_KERBEROS5;
#endif 

#ifdef DJGPP
	__djgpp_set_quiet_socket(1);	
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
	    }  else if ( strcmp(argv[i], "-q") == 0 ) {
		    verbose = 0;
		    i++;
	    }  else if ( strcmp(argv[i], "-pipe") == 0 ) {
		    /* obsolete option -pipe used to mainly reduce verbosity */
		    interactive = 0;
		    verbose = 0;
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
	    } else if ( strncmp(argv[i], "-h", 2) == 0) {
		    i++;
		    if (i >= argc) {
			    fprintf(stderr, "Missing host\n");
			    printusage();
			    exit(1);
		    }
		    hostname = argv[i];
		    i++;
	    }  else if ( strcmp(argv[i], "-as") == 0 ) {
		    i++;
		    if (i >= argc) {
			    fprintf(stderr, "Missing -as username\n");
			    printusage();
			    exit(1);
		    }
		    runas = argv[i];
		    i++;
	    } else if ( i == argc-1 ) {
                    username = argv[i];
		    SplitRealmFromName(username, &realm);
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

#ifndef __CYGWIN32__
    if (runas) {
	struct passwd *pw = getpwnam(runas);
	if (pw) 
	    setuid(pw->pw_uid);
    }
#endif
    
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
	    fprintf (stderr, "Try \"clog user[@realm]\"\n");
	    exit (1);
    }

    if (!isatty(0)) {
	verbose = 0;
	interactive = 0;
    }

    if (verbose)
	printf("username: %s@%s\n", username, realm);

    U_InitRPC();

    if (fromfile) {
        ReadTokenFromFile(fromfile, &cToken, sToken);
    } else {
	struct RPC2_addrinfo *srvs = U_GetAuthServers(realm, hostname);
	rc = U_Authenticate(srvs, authmethod, username, strlen(username)+1,
			    &cToken, sToken, verbose, interactive);
	RPC2_freeaddrinfo(srvs);
        if (rc != 0) {
            fprintf (stderr, "Invalid login (%s).\n", RPC2_ErrorMsg(rc));
            exit (1);
        }
    }

    if (testing)
	    printf ("Sending token to venus\n");

    if (tofile)
        WriteTokenToFile(tofile, &cToken, sToken);

    if(U_SetLocalTokens(0, &cToken, sToken, realm))
	printf("Local login only, could not contact venus\n");
    
    if (testing) {
	    printf("Getting tokens back from venus\n");
	    if (U_GetLocalTokens(&testCTok, testSTok, realm) < 0)
		    perror("U_GetLocalTokens");
	    
	    printf("Comparing clear token\n");
	    if(memcmp(&cToken, &testCTok, sizeof(ClearToken)) != 0) {
		    printf("Bad ClearToken\n");
	    }
	    printf("Comparing secret token\n");
	    if(memcmp(sToken, testSTok, sizeof(EncryptedSecretToken)) != 0) {
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

