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


/************************************************************************/
/*									*/
/*  ViceErrorMsg - Change a vice error code to a string			*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include "coda_string.h"

#include <rpc2/rpc2.h>
#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

#include <inconsist.h>

char *ViceErrorMsg(int errorCode)
{
    if(errorCode < 0)
	return(RPC2_ErrorMsg((long)errorCode));

    switch(errorCode) {
	case 0:			return("Success");
	case VSALVAGE:		return("Volume needs to be salvaged");
	case VNOVNODE:		return("Bad vnode number");
	case VNOVOL:		return("Volume not online");
	case VVOLEXISTS:	return("Volume already exists");
	case VNOSERVICE:	return("Volume is not in service");
	case VOFFLINE:		return("Volume offline");
	case VONLINE:		return("Volume is already online");
	case EINCONS:		return("Inconsistent Object");
    }

    return strerror(errorCode);
}
