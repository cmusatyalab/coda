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






/*
 * codaproc.h
 * Created November 1989
 */

/* for recursive removes during repairs */
typedef struct rmBlk {
    struct VListStruct *vlist;
    long    VolumeId;
    Volume  *volptr;
    ViceStoreId	*StoreId;
    ClientEntry *client;
} rmBlk;

/* for recursive semantic checking */
typedef struct semBlk {
    RPC2_Handle RPCid;
    ClientEntry *client;
    Volume *volptr;
    ViceFid pFid;
    int	error;
} semBlk;

    


