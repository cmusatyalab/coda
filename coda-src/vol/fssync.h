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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/vol/fssync.h,v 1.1.1.1 1996/11/22 19:10:08 rvb Exp";
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



#ifndef _FSSYNC_H_
#define _FSSYNC_H_ 1

/* FSYNC commands */
#define FSYNC_ON		1 /* Volume online */
#define FSYNC_OFF		2 /* Volume offline */
#define FSYNC_LISTVOLUMES	3 /* Update local volume list */
#define FSYNC_NEEDVOLUME	4 /* Put volume in whatever mode (offline, or whatever)
				     best fits the attachment mode provided in reason */
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

#endif _FSSYNC_H_
