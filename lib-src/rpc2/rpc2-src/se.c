/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>

struct SE_Definition *SE_DefSpecs; /* array of definitions, one per activated side-effect */
long SE_DefCount;

char *SE_ErrorMsg(rc)
    long rc;
    /* Returns a pointer to a static string describing error rc. */
    {
    static char msgbuf[100];

    switch((enum SE_Status)rc)
	{
	case SE_SUCCESS:		return("SE_SUCCESS");

	case SE_FAILURE:		return("SE_FAILURE");

	case SE_INPROGRESS:		return("SE_INPROGRESS");

	case SE_NOTSTARTED:		return("SE_NOTSTARTED");

	default:			sprintf(msgbuf, "Unknown SE return code %ld", rc); return(msgbuf);
	}
    
    }


