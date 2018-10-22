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




/* this file contains local-repair related volent methods */

#ifdef __cplusplus
extern "C" {
#endif

#include <struct.h>

#include <rpc2/errors.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vcrcommon.h>

/* from venus */
#include "local.h"
#include "venusvol.h"


/* must not be called from within a transaction */
void reintvol::IncAbort(int tid)
{
    CML.IncAbort(tid);
    if (CML.count() == 0)
      CML.owner = UNSET_UID;
}

/* need not be called from within a transaction */
int reintvol::ContainUnrepairedCML()
{
    cml_iterator next(CML, CommitOrder);
    cmlent *m;
    while ((m = next())) {
	if (m->IsToBeRepaired())
	  return 1;
    }
    return 0;
}

/* must not be called from within a transaction */
int reintvol::GetReintId()
{
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(reint_id_gen);
    reint_id_gen++;
    Recov_EndTrans(MAXFP);
    return reint_id_gen;
}
