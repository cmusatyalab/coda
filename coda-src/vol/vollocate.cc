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

    /* Try tp find by replicated volume id */
    volid = strtoul(volkey, &end, 0);
    if (volkey == end) return 0;

    vre = VRDB.find(volid);
    if (vre)
	return VREtoVolRepId(vre);

    /* Try tp find by volume replica name */
    vldbp = VLDBLookup(volkey);
    if (vldbp)
	return ntohl(vldbp->volumeId[vldbp->volumeType]);

    /* Try to find by volume replica name */
    if (volid && HashLookup(volid) == -1)
	return 0;

    return volid;
}

