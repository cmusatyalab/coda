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
                           none currently

#*/







/* cure.h */

/* routines included in other files */
int ObjExists (resreplica *, long , long );
int RepairRename (int , resreplica *, resdir_entry **, int , listhdr **, char *, VolumeId);
int RepairSubsetCreate (int , resreplica *, resdir_entry **, int , listhdr **, VolumeId);
int RepairSubsetRemove (int, resreplica *, resdir_entry **, int , listhdr **);
