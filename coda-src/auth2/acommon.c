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
 -- Routines used by user-level processes (such as login, su, etc)  to do authentication

*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <lwp/lwp.h>
#include <pioctl.h> 
#include <rpc2/rpc2.h>
#include <util.h>
#include "auth2.h"

#include "auser.h"
#ifdef __cplusplus
}
#endif




void ntoh_SecretToken(SecretToken *stoken) {
    stoken->AuthHandle = ntohl(stoken->AuthHandle);
    stoken->ViceId = ntohl(stoken->ViceId);
    stoken->BeginTimestamp = ntohl(stoken->BeginTimestamp);
    stoken->EndTimestamp = ntohl(stoken->EndTimestamp);
}

void hton_SecretToken(SecretToken *stoken) {
    stoken->AuthHandle = htonl(stoken->AuthHandle);
    stoken->ViceId = htonl(stoken->ViceId);
    stoken->BeginTimestamp = htonl(stoken->BeginTimestamp);
    stoken->EndTimestamp = htonl(stoken->EndTimestamp);
}
