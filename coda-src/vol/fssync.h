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



#ifndef _FSSYNC_H_
#define _FSSYNC_H_ 1

/* FSYNC commands */
#define FSYNC_ON		1 /* Volume online */
#define FSYNC_OFF		2 /* Volume offline */
#define FSYNC_NEEDVOLUME	4 /* Put volume in whatever mode (offline, or
				     whatever) best fits the attachment mode
				     provided in reason */
#define FSYNC_MOVEVOLUME	5 /* Generate temporary relocation information
				     for this volume to another site, to be used
				     if this volume disappears */


/* Reasons (these could be communicated to venus or converted to messages) */
#define FSYNC_WHATEVER		0 /* XXXX */
#define FSYNC_SALVAGE		1 /* volume is being salvaged */
#define FSYNC_MOVE		2 /* volume is being moved */
#define FSYNC_OPERATOR		3 /* operator forced volume offline */

/* Replies (1 byte) */
#define FSYNC_DENIED		0
#define FSYNC_OK		1

/* Exported Routines */
extern void FSYNC_fsInit();
extern int FSYNC_clientInit();
extern void FSYNC_clientFinis();
extern int FSYNC_askfs(VolumeId volume, int com, int reason);
extern unsigned int FSYNC_CheckRelocationSite(VolumeId volumeId);

#endif /* _FSSYNC_H_ */
