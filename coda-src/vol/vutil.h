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

#ifndef _VUTIL_H_
#define _VUTIL_H_ 1
#include "voldefs.h"

extern Volume *VCreateVolume(Error *ec, char *partition, VolumeId volumeId, VolumeId parentId, VolumeId groupId, int type =readwriteVolume, int rvmlogsize =0);
extern Volume *MakeBackupVolume(Volume *vp, Volume *sacrifice, int verbose);
extern void AssignVolumeName(VolumeDiskData *vol, char *name, const char *ext);
extern void CopyVolumeHeader(VolumeDiskData *from, VolumeDiskData *to);
extern void ClearVolumeStats(VolumeDiskData *vol);

extern int ListViceInodes(char *devname, char *mountedOn, char *resultFile, int (*judgeInode)(struct ViceInodeInfo*, VolumeId), int judgeParam);
extern int ListCodaInodes(char *devname, char *mountedOn, char *resultFile, int (*judgeInode)(struct ViceInodeInfo*, VolumeId), int judgeParam);
extern int HashString(char *s, unsigned int size);
extern void CloneVolume(Error *error, Volume *original, Volume *newv, Volume *old);

#endif /* _VUTIL_H_ */
