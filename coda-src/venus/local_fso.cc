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

/* this file contains local-repair related fsobj methods */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <struct.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <codadir.h>
#include <fcntl.h>

/* interfaces */
#include <vcrcommon.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif



/* from venus */
#include "fso.h"
#include "local.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "venusvol.h"
#include "worker.h"

/* MUST be called from within a transaction */
void fsobj::SetComp(char *name)
{
    RVMLIB_REC_OBJECT(comp);
    if (comp) rvmlib_rec_free(comp);
    if (name && name[0] != '\0')
	 comp = rvmlib_rec_strdup(name);
    else comp = rvmlib_rec_strdup("");
}

const char *fsobj::GetComp(void)
{
    if (comp && comp[0] != '\0')
	 return comp;
    else return FID_(&fid);
}

/* must be called from within a transaction */
void fsobj::SetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 1;
}

/* must be called from within a transaction */
void fsobj::UnsetLocalObj()
{
    RVMLIB_REC_OBJECT(flags);
    flags.local = 0;
}

/* need not be called from within a transaction */
cmlent *fsobj::FinalCmlent(int tid)
{
    /* return the last cmlent done by iot tid */
    LOG(100, ("fsobj::FinalCmlent: %s\n", FID_(&fid)));
    FSO_ASSERT(this, mle_bindings);
    dlist_iterator next(*mle_bindings);
    dlink *d;
    cmlent *last = (cmlent *)0;

    while ((d = next())) {
	binding *b = strbase(binding, d, bindee_handle);
	cmlent *m = (cmlent *)b->binder;
	CODA_ASSERT(m);
	if (m->GetTid() != tid) continue;
	last = m;
    }
    CODA_ASSERT(last && last->GetTid() == tid);
    return last;
}
