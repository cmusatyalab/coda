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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/auth2/Attic/acommon.c,v 4.1 1998/08/05 23:49:07 braam Exp $";
#endif /*_BLURB_*/




#/*
#
#                         IBM COPYRIGHT NOTICE
#
#                          Copyright (C) 1986
#             International Business Machines Corporation
#                         All Rights Reserved
#
#This  file  contains  some  code identical to or derived from the 1986
#version of the Andrew File System ("AFS"), which is owned by  the  IBM
#Corporation.    This  code is provded "AS IS" and IBM does not warrant
#that it is free of infringement of  any  intellectual  rights  of  any
#third  party.    IBM  disclaims  liability of any kind for any damages
#whatsoever resulting directly or indirectly from use of this  software
#or  of  any  derivative work.  Carnegie Mellon University has obtained
#permission to distribute this code, which is based on Version 2 of AFS
#and  does  not  contain the features and enhancements that are part of
#Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
#supported by Transarc Corporation, Pittsburgh, PA.
#
#*/


/*
 -- Routines used by user-level processes (such as login, su, etc)  to do authentication

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <lwp.h>
#include <pioctl.h> 
#include <rpc2.h>
#include <util.h>
#include "auth2.h"

#include "auser.h"
#ifdef __cplusplus
}
#endif __cplusplus




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
