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
 -- Routines used by user-level processes (such as login, su, etc) to deal with venus

*/

#define VIRTUE 1
#define VICE 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <coda.h>
#include <pioctl.h>
#include <sys/file.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <codaconf.h>
#include "auth2.h"
#undef VIRTUE
#undef VICE

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void GetPathName();

static char *pName;		/* name to use on PIOCTLS */

typedef struct {
    int			    sTokenSize;
    EncryptedSecretToken    stoken;
    int			    cTokenSize;
    ClearToken		    ctoken;
    char		    realm[MAXHOSTNAMELEN];
} venusbuff;

 /* Tells Venus about the clear and secret tokens obtained from the
    auth server.  If setPag is true, a setpag system call is made.
    Returns 0 on success, -1 on failure.  Who knows what setpag did? */

int U_SetLocalTokens(IN int setPag, IN ClearToken *cToken, IN EncryptedSecretToken sToken, IN char *realm)
{
    int    rc;
    struct ViceIoctl buffer;
    venusbuff inbuff;

#if 0
    if(setPag) {
	setpag();
    }
#endif

    inbuff.sTokenSize = sizeof(EncryptedSecretToken);
    memcpy(&inbuff.stoken, sToken, sizeof(EncryptedSecretToken));
    inbuff.cTokenSize = sizeof(ClearToken);
    memcpy(&inbuff.ctoken, cToken, sizeof(ClearToken));
    strncpy(inbuff.realm, realm, MAXHOSTNAMELEN);

    buffer.in = (char *)&inbuff;
    buffer.out = 0;
    buffer.in_size = sizeof(inbuff);
    buffer.out_size = 0;

    GetPathName(); 
    rc = pioctl(pName, _VICEIOCTL(3), &buffer, 0);
    if(rc) {
	return(-1);
    }
    return(0);
}


int U_GetLocalTokens(OUT ClearToken *cToken, OUT EncryptedSecretToken sToken, IN const char *realm)
 /* Obtains the clear secret tokens from Venus.
    Fills in cToken and sToken with the clear and secret tokens.
    (Note: at the present time these are fixed-length data structures)
    Returns 0 on success, -1 on failure.    */
{
    int    rc;
    struct ViceIoctl buffer;
    venusbuff outbuff;

    buffer.in = (char *)realm;
    buffer.out = (char *)&outbuff;
    buffer.in_size = strlen(realm) + 1;
    buffer.out_size = sizeof(venusbuff);
    GetPathName();
    rc = pioctl(pName, _VICEIOCTL(8), &buffer, 0);
    if(rc) {
#ifdef __CYGWIN__
	return rc;
#else
	return -errno;
#endif
    }

    if(outbuff.sTokenSize != sizeof(EncryptedSecretToken)) return(-1);
    memcpy(sToken, &outbuff.stoken, sizeof(EncryptedSecretToken));

    if(outbuff.cTokenSize != sizeof(ClearToken)) return(-1);
    memcpy(cToken, &outbuff.ctoken, sizeof(ClearToken));

    return(0);
}


int U_DeleteLocalTokens(char *realm)
 /* Deletes internal state for viceId.  Returns 0.    */
{
    struct ViceIoctl buffer;

    buffer.in = realm;
    buffer.out = 0;
    buffer.in_size = strlen(realm) + 1;
    buffer.out_size = 0;
    GetPathName();
    pioctl(pName, _VICEIOCTL(9), &buffer, 0);
    return(0);    
}

#if defined(DJGPP) || defined(__CYGWIN32__)
#define DFLT_MNT "N:"
#else
#define DFLT_MNT "/coda"
#endif

static void GetPathName()
{
    codaconf_init("venus.conf");
    CODACONF_STR(pName, "mountpoint", DFLT_MNT);
}

