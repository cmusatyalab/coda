/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

#ifndef _VOLTYPES_H_
#define _VOLTYPES_H_ 1

#ifndef NULL
#define NULL	0
#endif
#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#include <rpc2/rpc2.h>
#include <stdint.h>

typedef u_int32_t bit32;	/* Unsigned, 32 bits */
typedef u_int16_t bit16;	/* Unsigned, 16 bits */
typedef u_int8_t  byte;		/* Unsigned, 8 bits */

typedef bit32	Device;		/* Unix device number */
typedef bit32	Inode;		/* Unix inode number */
typedef bit32	Error;		/* Error return code */

#ifndef _FID_T_
#define _FID_T_
typedef RPC2_Unsigned VolumeId;
typedef RPC2_Unsigned VolId;
typedef RPC2_Unsigned VnodeId;
typedef RPC2_Unsigned Unique_t;
typedef bit32 FileVersion;
#endif

#endif
