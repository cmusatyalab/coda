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






#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <sys/types.h>
#include <stdio.h>
#include <codadir.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <srv.h>
#include "vlist.h"

int VLECmp(vle *a, vle *b) 
{
	CODA_ASSERT(FID_VolEQ(&a->fid, &b->fid));
	return FID_Cmp(&a->fid, &b->fid);
}


vle *FindVLE(dlist& dl, ViceFid *fid) 
{
    dlist_iterator next(dl);
    vle *v;
    while (v = (vle *)next())
	if (FID_EQ(&v->fid, fid)) return(v);
    return(0);
}


vle *AddVLE(dlist& dl, ViceFid *fid) 
{
    vle *v = FindVLE(dl, fid);
    if (v == 0)
	dl.insert((v = new vle(fid)));
    return(v);
}
