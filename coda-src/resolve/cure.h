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
int ObjExists C_ARGS((resreplica *, long , long ));
int RepairRename C_ARGS((int , resreplica *, resdir_entry **, int , listhdr **, char *));
int RepairSubsetCreate C_ARGS((int , resreplica *, resdir_entry **, int , listhdr **));
int RepairSubsetRemove C_ARGS((int, resreplica *, resdir_entry **, int , listhdr **));
