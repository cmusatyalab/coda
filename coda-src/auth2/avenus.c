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
 -- Routines used by user-level processes (such as login, su, etc) to deal with venus

*/

#define VIRTUE 1
#define VICE 1

#ifdef __CYGWIN32__
#define VSTAB "c:\\usr\\coda\\etc\\vstab"
#else
#define VSTAB "/usr/coda/etc/vstab"
#endif

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <cfs/coda.h>
#include <pioctl.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include "auth2.h"
#undef VIRTUE
#undef VICE

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void GetPathName();

static char pName[64];		/* name to use on PIOCTLS */

typedef struct {
    int			    sTokenSize;
    EncryptedSecretToken    stoken;
    int			    cTokenSize;
    ClearToken		    ctoken;
} venusbuff;

 /* Tells Venus about the clear and secret tokens obtained from the
    auth server.  If setPag is true, a setpag system call is made.
    Returns 0 on success, -1 on failure.  Who knows what setpag did? */

int U_SetLocalTokens(IN int setPag, IN ClearToken *cToken, IN EncryptedSecretToken sToken)
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
    bcopy((char *)sToken,(char *)inbuff.stoken, sizeof(EncryptedSecretToken));
    inbuff.cTokenSize = sizeof(ClearToken);
    bcopy((char *)cToken, (char*)&inbuff.ctoken, sizeof(ClearToken));
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


int U_GetLocalTokens(OUT ClearToken *cToken, OUT EncryptedSecretToken sToken)
 /* Obtains the clear secret tokens from Venus.
    Fills in cToken and sToken with the clear and secret tokens.
    (Note: at the present time these are fixed-length data structures)
    Returns 0 on success, -1 on failure.    */
{
    int    rc;
    struct ViceIoctl buffer;
    venusbuff outbuff;

    buffer.in = 0;
    buffer.out = (char *)&outbuff;
    buffer.in_size = 0;
    buffer.out_size = sizeof(venusbuff);
    GetPathName();
    rc = pioctl(pName, _VICEIOCTL(8), &buffer, 0);
    if(rc) {
#ifdef __CYGWIN32__
	return (rc);
#else
	return(-1);
#endif
    }
    if(outbuff.sTokenSize != sizeof(EncryptedSecretToken))
	    return(-1);
    bcopy((char *)outbuff.stoken, (char *)sToken, sizeof(EncryptedSecretToken));
    if(outbuff.cTokenSize != sizeof(ClearToken))
	    return(-1);
    bcopy((char *)&outbuff.ctoken, (char *)cToken, sizeof(ClearToken));
    return(0);
}


int U_DeleteLocalTokens()
 /* Deletes internal state for viceId.  Returns 0.    */
{
    struct ViceIoctl buffer;

    buffer.in = 0;
    buffer.out = 0;
    buffer.in_size = 0;
    buffer.out_size = 0;
    GetPathName();
    pioctl(pName, _VICEIOCTL(9), &buffer, 0);
    return(0);    
}


static void GetPathName()
{
    int		fd;
    int		len;
    char	* area;
    struct	stat	buff;

    bzero(pName,sizeof(pName));
    
#if defined(DJGPP) || defined(__CYGWIN32__) /* hack for now until better solution for all pioctl using programs */
    strcpy(pName, "/coda");
    return;
#endif
    
    if((fd = open(VSTAB,O_RDONLY | O_BINARY,0)) >= 0) {
	if(!(fstat(fd, &buff))) {
	    area = (char *)malloc(buff.st_size);
	    if(!area) {
		close(fd);
		goto fini;
	    }
	    len = read(fd, area, buff.st_size);
	    if(len == buff.st_size) {
		strncpy(pName,area,index(area,':')-area);
	    }
	    else {
		perror("Read of VSTAB failed\n");
	    }
	    free(area);
	}
	else {
	    perror("fstat for VSTAB failed");
	}
	close(fd);
    }
    else {
	perror("open for VSTAB failed");
    }
 fini:
    if(strlen(pName) == 0)
	strcpy(pName, "/cmu");
}
