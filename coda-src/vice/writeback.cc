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

/************************************************************************/
/*									*/
/*  writeback.c	- Writeback caching specific routines			*/
/*									*/
/************************************************************************/

#include <rpc2/rpc2.h>
#include <vice.h>
#include <writeback.h>

/* Attempt to get a WriteBack permit on a volume */
long FS_ViceGetWBPermit(RPC2_Handle cid, VolumeId Vid, 
			ViceFid *fid, RPC2_Integer *Permit)
{
    *Permit = WB_DISABLED;
    return(0);
}

/* Attempt to return an unused permit */
long FS_ViceRejectWBPermit(RPC2_Handle cid, VolumeId Vid)
{
    return 0;
}

/* Attempt to give up a WriteBack permit on a volume */
long FS_ViceTossWBPermit(RPC2_Handle cid, VolumeId Vid, ViceFid *Fid)
{
    return 0;
}

