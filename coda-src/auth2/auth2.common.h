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

#ifndef _AUTH2_COMMON_
#define _AUTH2_COMMON_

/* per-connection info */
struct UserInfo
{
	RPC2_Handle handle;
        int ViceId;     /* from NewConnection */
        int HasQuit;    /* TRUE iff Quit() was received on this connection */
        PRS_InternalCPS *UserCPS;
        int LastUsed;   /* timestamped at each RPC call; for gc'ing */
};

#endif
