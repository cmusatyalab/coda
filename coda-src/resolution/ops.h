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

#ifndef _OPS_H
#define _OPS_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <dlist.h>
#include <rec_dlist.h>
#include <vmindex.h>
#include <cvnode.h>
#include <vlist.h>
#include <res.h>
#include <resutil.h>

class rsle;
class res_mgrpent;

/* export definitions */
extern int SpoolVMLogRecord(dlist *, vle *, Volume *, ViceStoreId *, int op...);
extern int SpoolRenameLogRecord(int, dlist *, vle *, vle *, vle *, vle *,
                                Volume *, char *, char *, ViceStoreId *);
extern void TruncateLog(Volume *, Vnode *, vmindex *);
extern void FreeVMIndices(Volume *, vmindex *);
extern void PurgeLog(rec_dlist *, Volume *, vmindex *);
extern void PrintLog(rec_dlist *, FILE *);
extern void PrintLog(Vnode *, FILE *);
extern void DumpLog(rec_dlist *, Volume *, char **, int *, int *);
// temporary  - should go to rvmrescoord.h
extern long RecovDirResolve(res_mgrpent *, ViceFid *, ViceVersionVector **,
                            ResStatus **, int *, struct DirFid *);
extern int CheckAndPerformRename(rsle *, Volume *, VolumeId, ViceFid *, dlist *,
                                 olist *, dlist *, int *, DirFid *);
#endif /* _OPS_H_ */
