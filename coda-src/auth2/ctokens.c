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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "auth2.h"
#include "avenus.h"

#ifdef __CYGWIN32__
#include <windows.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus



int main(int argc, char *argv[])
{
    ClearToken clear;
    EncryptedSecretToken secret;
    int rc;

    /* Header. */
    fprintf(stdout, "\nToken held by the Cache Manager:\n\n");
#ifdef __CYGWIN32__
    fprintf(stdout, "Local username: %s\n", getlogin());
#else
    fprintf(stdout, "Local uid: %d\n", (int) geteuid());
#endif

    /* Get the tokens.  */
    rc = U_GetLocalTokens(&clear, secret);
    if (rc < 0) {
#ifdef __CYGWIN32__
	if ((rc * (-1)) == ENOTCONN)
#else
	if (errno == ENOTCONN)
#endif
            fprintf(stdout, "Not Authenticated\n\n");
	else
#ifdef __CYGWIN32__
	    fprintf(stdout, "\nGetLocalTokens error (%d)\n", rc*(-1));
#else
	    fprintf(stdout, "\nGetLocalTokens error (%d)\n", errno);
#endif

	exit(-1);
    }

    fprintf(stdout, "Coda user id: %lu\n", clear.ViceId);

    /* Check for expiration. */
    if (clear.EndTimestamp <= time(0))
	fprintf(stdout, "This token has expired.\n");
    else {
	fprintf(stdout, "Expiration time: %s\n", ctime((time_t *)&clear.EndTimestamp));
    }

    return(0);
}
