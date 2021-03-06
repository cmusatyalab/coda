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

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>

/* Return the MaxVolId from recoverable storage */
int GetMaxVolId()
{
    return (SRV_RVM(MaxVolId) & 0x00FFFFFF);
}

/* Get a volume from recoverable storage
 * Returns pointer to volume if successful, NULL otherwise
 */

VolHead *VolByIndex(int myind)
{
    int maxid = GetMaxVolId();

    if ((myind < 0) || (myind >= maxid) || (myind >= MAXVOLS))
        return (NULL);

    return (&SRV_RVM(VolumeList[myind]));
}

/* Get a volume header from recoverable storage given the appropriate index
 * Returns pointer to header if successful, NULL otherwise
 */
VolumeHeader *VolHeaderByIndex(int myind)
{
    VolHead *vol = VolByIndex(myind);

    if (!vol)
        return NULL;

    return &(vol->header);
}
