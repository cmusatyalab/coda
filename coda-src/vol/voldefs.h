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

#ifndef _VOLDEFS_H_
#define _VOLDEFS_H_ 1

/* If you add volume types here, be sure to check the definition of
   volumeWriteable in volume.h */

#define readwriteVolume RWVOL
#define readonlyVolume  ROVOL
#define backupVolume	   BACKVOL
#define replicatedVolume	REPVOL

#define RWVOL			0
#define ROVOL			1
#define BACKVOL			2
#define	REPVOL			3

/* All volumes will have a volume header name in this format */
/*#define VFORMAT "V%010lu.vol"*/
#define VFORMAT "V%010lu"
#define VMAXPATHLEN 64  /* Maximum length (including null) of a volume
			   external path name */

/* Pathname for the maximum volume id ever created by this server */
#define MAXVOLIDPATH	Vol_vicefile("vol/maxvolid")

/* Pathname for server id definitions--the server id is used to allocate volume numbers */
#define SERVERLISTPATH	"/vice/db/servers"

/* Values for connect parameter to VInitVolumePackage */
#define CONNECT_FS	1
#define DONT_CONNECT_FS	0

#endif _VOLDEFS_H_
