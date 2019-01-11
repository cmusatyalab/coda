/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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

/* volutil.private.h (used to be confusingly called "vutils.h") */

/* Common definitions for volume utilities */

#ifndef _VOLUTIL_PRIVATE_H_
#define _VOLUTIL_PRIVATE_H_ 1

#define VOLUTIL_TIMEOUT 15 /* Timeout period for a remote host */

/* Exit codes -- see comments in tcp/exits.h */
#define VOLUTIL_RESTART 64 /* please restart this job later */
#define VOLUTIL_ABORT 1 /* do not restart this job */

void PrintVersionVector(FILE *outfile, ViceVersionVector vv);

#endif /* _VOLUTIL_PRIVATE_H_ */
