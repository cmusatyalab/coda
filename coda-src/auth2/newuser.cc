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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/auth2/Attic/newuser.cc,v 4.1 1997/01/08 21:49:27 rvb Exp $";
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
 -- Routine to add a new user to the auth data base from a list built during user int

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include "auth2.h"

int main(int argc, char **argv);
PRIVATE int AddNewUser (char *uid, char *pw);

PRIVATE RPC2_Handle AuthID;


int main(int argc, char **argv)
{
    register int i;
    char   *current;
    char   *next;
    char   *uid;
    char   *pw;
    int    rc;
    int    fd;
    struct stat    buff;
    char   *auid = 0;
    char   *apw = 0;
    char   *filenm = 0;
    char   *area = 0;
    RPC2_EncryptionKey	key;

 /* parse arguments    */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    if (strcmp(argv[i], "-f") == 0) {
		filenm = argv[++i];
		continue;
	    }
	    break;
	}
	if (auid == 0) {
	    auid = argv[i];
	    continue;
	}
	if (apw == 0) {
	    apw = argv[i];
	    continue;
	}
	break;
    }

    if (!auid || !apw || !filenm) {
	printf("usage: newuser -f filename authuserid authpasswd\n");
	fflush(stdout);
	exit(-1);
    }

 /* Bind to auth server using auid and apw */
    U_InitRPC();
    bzero(key, sizeof(RPC2_EncryptionKey));
    strncpy(key, apw, sizeof(RPC2_EncryptionKey));
    rc = U_BindToServer(1, auid, key, &AuthID);
    if(rc != AUTH_SUCCESS) {
	printf("Bind to Auth Server failed %s\n",U_Error(rc));
	fflush(stdout);
	exit(-2);
    }

 /* open input file and read it into area malloc */
    if(stat(filenm, &buff)) {
	printf("Could not stat %s because %d\n",filenm, errno);
	fflush(stdout);
	exit(-3);
    }
    area = (char *)malloc(buff.st_size+1);
    fd = open(filenm, O_RDONLY, 0);
    if(fd <= 0) {
	printf("Could not open %s because %d\n",filenm, errno);
	fflush(stdout);
	exit(-4);
    }
    rc = read(fd, area, buff.st_size);
    if(rc != buff.st_size) {
	printf("Could nor read %s got %d bytes instead of %d, error = %d\n",
		filenm, rc, buff.st_size, errno);
	fflush(stdout);
	exit(-5);
    }
    close(fd);
    *(area+buff.st_size+1) = '\0';

 /* parse data in area and pass it to AddNewUser. The 2nd field in each line is the
    uid, the fourth field in each line is the password. The first field must contain
    the string "makeuser.sh ".  Fields are blank separated */
    for(current = area; current < area+buff.st_size, current > (char *)1; current = next) {
	next = index(current, '\n') + 1;	/* next line */
	if(strncmp(current, "makeuser.sh ", 12) != 0)  /* skip if not proper line */
	    continue;
	uid = index(current,' ') + 1;		/* use second field  */
	pw = index(index(uid,' ') + 1, ' ') + 1; /* use fourth field */
	*(index(uid,' ')) = '\0';		/* make uid a string */
	*(index(pw,' ')) = '\0';		/* make pw a string */
	AddNewUser(uid, pw);
    }

 /* clean up and Unbind the connection */
    if(area) free(area);
    RPC2_Unbind(AuthID);
    return(0);
}


PRIVATE int AddNewUser (char *uid, char *pw)
{
    RPC2_Integer	vid;
    int			rc;
    RPC2_EncryptionKey  ek;
    char		userid[256];

    /* get vid from input uid */
    bzero(ek, sizeof(RPC2_EncryptionKey));
    strncpy(ek, pw, sizeof(RPC2_EncryptionKey));
    rc = AuthNameToId(AuthID, uid, &vid);
    if(rc != AUTH_SUCCESS) {
	printf("Name translation for %s failed %s\n",uid, U_Error(rc));
	fflush(stdout);
	return(-1);
    }
	
    /* pass vid and pw to auth server */
    strcpy(userid,uid);
    strcat(userid," ");
    rc = AuthNewUser(AuthID, vid, ek, userid);
    if(rc != AUTH_SUCCESS) {
	printf("Add User for %s failed with %s\n",uid, U_Error(rc));
	fflush(stdout);
	return(-1);
    }
    return(0);
}
