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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#ifndef _VICEINODE_H_
#define _VICEINODE_H_ 1

/* The four inode parameters for most inodes (files, directories,
   symbolic links) */
struct InodeParams {
    VolId	volumeId;
    VnodeId	vnodeNumber;
    Unique_t	vnodeUniquifier;
    FileVersion	inodeDataVersion;
};

/* The four inode parameters for special inodes, i.e. the descriptive
   inodes for a volume */
struct SpecialInodeParams {
    VolId	volumeId;
    VnodeId	vnodeNumber; /* this must be INODESPECIAL */
    int		type;
    VolId	parentId;
};

/* Structure of individual records output by fsck.
   When VICEMAGIC inodes are created, they are given four parameters;
   these correspond to the params.fsck array of this record.
 */
typedef
struct ViceInodeInfo {
    bit32	inodeNumber;
    int		byteCount;
    int		linkCount;
    union {
        bit32			  param[4];
        struct InodeParams 	  vnode;
	struct SpecialInodeParams special;
    } u;
}ViceInodeInfo; 

#define INODESPECIAL	0xffffffff	/* This vnode number will never
					   be used legitimately */
/* Special inode types.  Be careful of the ordering.  Must start at 1.
   See vutil.h */
#define VI_VOLINFO	1	/* The basic volume information file */
#define VI_SMALLINDEX	2	/* The index of small vnodes */
#define VI_LARGEINDEX	3	/* The index of large vnodes */
#define VI_ACL		4	/* The volume's access control list */
#define	VI_MOUNTTABLE	5	/* The volume's mount table */

#endif _VICEINODE_H_
