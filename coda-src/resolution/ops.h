#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/resolution/ops.h,v 4.1 1997/01/08 21:50:35 rvb Exp $";
#endif /*_BLURB_*/





#ifndef _OPS_H
#define _OPS_H 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>

#ifdef	__MACH__
#include <libc.h>    
#endif
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif /* LINUX || __NetBSD__ */

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
#include <reslog.h>

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
