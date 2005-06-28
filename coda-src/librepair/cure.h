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
                           none currently
#*/

/* cure.h */
#include <vcrcommon.h>

/* routines included in other files */
int ObjExists(resreplica *dir, VnodeId vnode, Unique_t unique);
int RepairRename (int , resreplica *, resdir_entry **, int, listhdr **,
		  VolumeId, char *realm);
int RepairSubsetCreate (int , resreplica *, resdir_entry **, int , listhdr **,
			VolumeId);
int RepairSubsetRemove (int, resreplica *, resdir_entry **, int , listhdr **);
