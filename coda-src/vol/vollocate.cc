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

#include <srv.h>
#include "vrdb.h"
#include "vldb.h"
#include "volhash.h"
#include "vollocate.h"

static VolumeId VREtoVolRepId(vrent *vre)
{
    int idx;

    if (!vre) return 0;

    idx = vre->index(ThisHostAddr);
    if (idx == -1) return 0;

    return vre->ServerVolnum[idx];
}

VolumeId VOL_Locate(char *volkey)
{
    vrent *vre;
    char *end;
    VolumeId volid;
    struct vldb *vldbp;
    
    /* Try tp find by replicated volume name */
    vre = VRDB.find(volkey);
    if (vre)
	return VREtoVolRepId(vre);

    /* Try to find by replicated volume id */
    volid = strtoul(volkey, &end, 16);
    if (volkey != end) {
	vre = VRDB.find(volid);
	if (vre)
	    return VREtoVolRepId(vre);
    }
    else
	volid = 0;

    /* Try to find by volume replica name */
    vldbp = VLDBLookup(volkey);
    if (vldbp)
	return ntohl(vldbp->volumeId[vldbp->volumeType]);

    return volid;
}

