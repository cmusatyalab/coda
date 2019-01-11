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

#ifndef VICEINODE_INCLUDED
#define VICEINODE_INCLUDED 1
#include <vcrcommon.h>
#include <voltypes.h>

/* used by the inode methods */
struct i_header {
    int32_t lnk;
    VolumeId volume;
    VnodeId vnode;
    Unique_t unique;
    FileVersion dataversion;
    int32_t magic;
};

/* Structure of individual records output by fsck.
   When VICEMAGIC inodes are created, they are given four parameters;
   these correspond to the params.fsck array of this record.
 */
struct ViceInodeInfo {
    bit32 InodeNumber;
    int32_t ByteCount;
    int32_t LinkCount;
    VolumeId VolumeNo;
    VnodeId VnodeNumber;
    Unique_t VnodeUniquifier;
    FileVersion InodeDataVersion;
    int32_t Magic;
};

#define INODESPECIAL \
    0xffffffff /* This vnode number will never
					   be used legitimately */

#endif /* _VICEINODE_H_ */
