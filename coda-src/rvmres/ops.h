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





#ifndef _OPS_H
#define _OPS_H 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
}
#endif __cplusplus

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
extern int SpoolVMLogRecord(vle *, Volume *, ViceStoreId *, int op ...);
extern int SpoolVMLogRecord(vle *, Volume *, ViceStoreId *, int, va_list);
extern int SpoolVMLogRecord(dlist *, Vnode *, Volume *, ViceStoreId *, int op...);
extern int SpoolVMLogRecord(dlist *, Vnode *, Volume *, ViceStoreId *, int , va_list);
extern int SpoolVMLogRecord(dlist *, vle *,  Volume *, ViceStoreId *, int op ...);
extern int SpoolVMLogRecord(dlist *, vle *, Volume *, ViceStoreId *, int , va_list);
extern int SpoolRenameLogRecord(int, dlist *, Vnode *, Vnode *, Vnode *, Vnode *, 
				Volume *, char *, char *, ViceStoreId *);
extern void TruncateLog(Volume *, Vnode *, vmindex *);
extern void FreeVMIndices(Volume *, vmindex *);
extern void PurgeLog(rec_dlist *, Volume *, vmindex *);
extern void PrintLog(rec_dlist *, FILE *);
extern void PrintLog(Vnode *, FILE *);
extern void DumpLog(rec_dlist *, Volume *, char **, int *, int *);
// temporary  - should go to rvmrescoord.h
extern long RecovDirResolve(res_mgrpent *, ViceFid *, ViceVersionVector **, ResStatus **, int *, int *, ResPathElem **, int =1/* checkpaths flag*/);
extern int CheckAndPerformRename(rsle *, Volume *, VolumeId, ViceFid *, dlist *, olist *, dlist *, int *);
#endif _OPS_H
