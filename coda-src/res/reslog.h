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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/res/reslog.h,v 1.1.1.1 1996/11/22 19:12:46 rvb Exp";
#endif /*_BLURB_*/







#ifndef _RESLOG_H_
#define _RESLOG_H_ 1
/* reslog.h 
 * Definitions for the logger used by resolution 
 * Puneet Kumar
 * Created 8/21/90
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdarg.h>
#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vcrcommon.h>
#include <cvnode.h>
#include <olist.h>
#include "logalloc.h"
#include "pdlist.h"

class PMemMgr;
class VNResLog;

#define MAXVOLS	1024	/* XX taken from coda_globals.h */
#define DIROPNAMESIZE	48 /* FOR NOW USE LOW NUMBER - IN FINAL VERSION WE 
			      WILL HAVE TO HAVE SOME OPTIMIZATION TRICKS AND THE
			      NAME BE 256 CHARS LONG */

/* define opcodes for log records spooling */
#define	RESOLVE_OPERATION_BASE	128
#define ResolveStoreAcl_OP	RESOLVE_OPERATION_BASE + 2
#define	ResolveViceRemove_OP	RESOLVE_OPERATION_BASE + 3
#define	ResolveViceCreate_OP	RESOLVE_OPERATION_BASE + 4
#define	ResolveViceRename_OP	RESOLVE_OPERATION_BASE + 5
#define	ResolveViceSymLink_OP	RESOLVE_OPERATION_BASE + 6 
#define	ResolveViceLink_OP	RESOLVE_OPERATION_BASE + 7 
#define	ResolveViceMakeDir_OP	RESOLVE_OPERATION_BASE + 8 
#define	ResolveViceRemoveDir_OP	RESOLVE_OPERATION_BASE + 9 
#define ResolveNULL_OP		RESOLVE_OPERATION_BASE + 10
#define ResolveViceSetVolumeStatus_OP RESOLVE_OPERATION_BASE + 11

#define	ResolveViceNewStore_OP	RESOLVE_OPERATION_BASE + 12

#define ResolveAfterCrash_OP	RESOLVE_OPERATION_BASE - 1

#define ISNONRESOLVEOP(a)	(((a) < RESOLVE_OPERATION_BASE) || \
				 ((a) == ResolveNULL_OP))

// for validating a resolution operation 
#define PERFORMOP	0
#define NULLOP		1
#define	MARKPARENTINC	2
#define	MARKOBJINC	3
#define CREATEINCOBJ	4

/* define correspondence between _OP and indexes for op-table */
#define	ViceCreate_IDX			2	/* ViceCreate_OP */
#define	ResolveViceCreate_IDX		3	/* ResolveViceCreate_OP */
#define	ViceRemove_IDX			4	/* ViceRemove_OP */
#define	ResolveViceRemove_IDX		5	/* ResolveViceRemove_OP */
#define	ViceLink_IDX			6	/* ViceLink_OP */
#define	ResolveViceLink_IDX		7	/* ResolveViceLink_OP */
#define	ViceRename_IDX			8	/* ViceRename_OP */
#define	ResolveViceRename_IDX		9	/* ResolveViceRename_OP */
#define	ViceMakeDir_IDX			10	/* ViceMakeDir_OP */
#define	ResolveViceMakeDir_IDX		11	/* ResolveViceMakeDir_OP */
#define	ViceRemoveDir_IDX		12	/* ViceRemoveDir_OP */
#define	ResolveViceRemoveDir_IDX	13	/* ResolveViceRemoveDir_OP */
#define	ViceSymLink_IDX			14	/* ViceSymLink_OP */
#define	ResolveViceSymLink_IDX		15	/* ResolveViceSymLink_OP */
#define	ResolveNULL_IDX			16	/* ResolveNULL_OP */
#define	ResolveAfterCrash_IDX		17	/* ResolveAfterCrash_OP */
#define	ViceRepair_IDX			18	/* ViceRepair_OP */
#define IllegalOP_IDX			19
#define ResolveViceSetVolumeStatus_IDX  20      /* ResolveViceSetVolumeStatus_OP */
#define ViceSetVolumeStatus_IDX         21      /* ViceSetVolumeStatus_OP */

#define ViceNewStore_IDX		22	/* ViceNewStore_OP */
#define ResolveViceNewStore_IDX		23	/* ResolveViceNewStore_OP */


/* global variables declaration */
extern PMemMgr *LogStore[MAXVOLS];
extern olist *VolLogPtrs[MAXVOLS];	


class rlent {
  public:
    unsigned long	serverid;
    ViceStoreId 	storeid;
    RPC2_Unsigned 	opcode;
    pdlink 		link;
    VnodeId		dvnode;
    Unique_t		dunique;
    union {
	struct {
#define 	STATUSStore 	0
#define 	ACLStore    	1
	    unsigned long  stType;	/* can be either status or ACL */
	    union {
/* Acl is removed here to keep log records small - once we have 
		char	acl[SIZEOF_LARGEDISKVNODE - SIZEOF_SMALLDISKVNODE];
*/
		struct {
		    UserId	owner;
		    RPC2_Unsigned mode;
		} st;
	    } s;
	} u_store;
	struct {
	    unsigned long  stType;	/* can be either status or ACL */
	    union {
		struct {
		    UserId	owner;
		    RPC2_Unsigned mode;
		    RPC2_Integer mask;
		} st;
	    } s;
	} u_newstore;
	struct {
	    char 	name[DIROPNAMESIZE];
	    VnodeId	cvnode;
	    Unique_t	cunique;
	    ViceVersionVector cvv; /* version vector for child when deleted */
	} u_remove;
	struct {
	    char    	name[DIROPNAMESIZE]; /* name of child created */
	    VnodeId 	cvnode;
	    Unique_t 	cunique;
	} u_create;
#define	SOURCE	0	/* source or target log record */
#define TARGET	1	 
	struct {
	    unsigned long srctgt;
	    struct {
		char 	oldname[DIROPNAMESIZE];
		VnodeId	cvnode;
		Unique_t	cunique;
		ViceVersionVector cvv;
	    } rename_src;
	    VnodeId OtherDirV;		/* src/tgt dir spec */
	    Unique_t OtherDirU;
	    struct {
		char	newname[DIROPNAMESIZE];
		int 	tgtexisted;
		VnodeId	TgtVnode;	/* vnode of target obj if it existed */
		Unique_t	TgtUnique;	/* uniquifier of target obj */
		union {
		    ViceVersionVector TgtGhostVV;
		    struct {
			int	head;
			int 	count;
		    } TgtGhostLog;
		} TgtGhost;
	    } rename_tgt;
	} u_rename;
	struct {
	    char	name[DIROPNAMESIZE];
	    VnodeId 	cvnode; 
	    Unique_t 	cunique;
	} u_symlink;
	struct {
	    char	name[DIROPNAMESIZE];
	    VnodeId 	cvnode;
	    Unique_t 	cunique;
	} u_hardlink;
	struct {
	    char	name[DIROPNAMESIZE];
	    VnodeId 	cvnode;
	    Unique_t 	cunique;
	} u_makedir;
	struct {
	    char	name[DIROPNAMESIZE];
	    VnodeId 	cvnode;
	    Unique_t 	cunique;
	    int 	head;	/* head of log for child being deleted */
	    int		count;	/* number of log entries for child */
	    ViceStoreId childLCP; /* childs lcp in log with other reps */
	    ViceStoreId	csid;	/* storeid of directory when deleted */
	} u_removedir;
	struct {
	    
	} u_repair;
    } u;
    

    rlent();
    ~rlent();

    int init(ViceFid *, ViceStoreId *, int, va_list);
    void hton();
    void ntoh();
    ViceStoreId *GetStoreId();

    void print();
    void print(FILE *);
    void print(int);
};

#define	PRINTOPCODE(op)    ((op) == ViceCreate_OP ? "Create" :\
			    (op) == ResolveViceCreate_OP ? "ResolveCreate" :\
			    (op) == ViceRemove_OP ? "Remove" :\
			    (op) == ResolveViceRemove_OP ? "ResolveRemove" :\
			    (op) == ViceLink_OP ? "Link" :\
			    (op) == ResolveViceLink_OP ? "ResolveLink" :\
			    (op) == ViceRename_OP ? "Rename" :\
			    (op) == ResolveViceRename_OP ? "ResolveRename" :\
			    (op) == ViceMakeDir_OP ? "Mkdir" :\
			    (op) == ResolveViceMakeDir_OP ? "ResolveMkdir" :\
			    (op) == ViceRemoveDir_OP ? "Rmdir" :\
			    (op) == ResolveViceRemoveDir_OP ? "ResolveRmdir" :\
			    (op) == ViceSymLink_OP ? "Symlink" :\
			    (op) == ResolveViceSymLink_OP ? "ResolveSymlink" :\
			    (op) == ResolveNULL_OP ? "ResolveNULL_OP" :\
			    (op) == ViceRepair_OP ? "ViceRepair_OP" :\
			    (op) == ResolveViceSetVolumeStatus_OP ? "ResolveViceSetVolumeStatus_OP" :\
			    (op) == ViceSetVolumeStatus_OP ? "ViceSetVolumeStatus_OP" :\
			    (op) == ViceNewStore_OP ? "NewStore" :\
			    (op) == ResolveViceNewStore_OP ? "ResolveNewStore" :\
			    "???")

#define	GETOPINDEX(op) \
     ((op) == ViceCreate_OP ?		ViceCreate_IDX :\
      (op) == ResolveViceCreate_OP ?	ResolveViceCreate_IDX :\
      (op) == ViceRemove_OP ?		ViceRemove_IDX :\
      (op) == ResolveViceRemove_OP ?	ResolveViceRemove_IDX :\
      (op) == ViceLink_OP ?		ViceLink_IDX :\
      (op) == ResolveViceLink_OP ?	ResolveViceLink_IDX :\
      (op) == ViceRename_OP ?		ViceRename_IDX :\
      (op) == ResolveViceRename_OP ?	ResolveViceRename_IDX :\
      (op) == ViceMakeDir_OP ?		ViceMakeDir_IDX :\
      (op) == ResolveViceMakeDir_OP ?	ResolveViceMakeDir_IDX :\
      (op) == ViceRemoveDir_OP ?	ViceRemoveDir_IDX :\
      (op) == ResolveViceRemoveDir_OP ?	ResolveViceRemoveDir_IDX :\
      (op) == ViceSymLink_OP ?		ViceSymLink_IDX :\
      (op) == ResolveViceSymLink_OP ?	ResolveViceSymLink_IDX :\
      (op) == ResolveNULL_OP ?		ResolveNULL_IDX :\
      (op) == ResolveAfterCrash_OP ?	ResolveAfterCrash_IDX :\
      (op) == ViceRepair_OP ?		ViceRepair_IDX :\
      (op) == ViceSetVolumeStatus_OP ?  ViceSetVolumeStatus_IDX :\
      (op) == ResolveViceSetVolumeStatus_OP ?  ResolveViceSetVolumeStatus_IDX :\
      (op) == ViceNewStore_OP ? ViceNewStore_IDX:\
      (op) == ResolveViceNewStore_OP ? ResolveViceNewStore_IDX:\
      IllegalOP_IDX)

class VNResLog : public olink {
  public:
    VnodeId vnode;
    Unique_t uniquifier;
    ViceStoreId LCP;		/* storeid of operation that I know happened everywhere */
    pdlist *loglist;		/* list of all the log entries */

    VNResLog(VnodeId, Unique_t, int);
    VNResLog(VnodeId, Unique_t, PMemMgr *, int, int, ViceStoreId *);
    ~VNResLog();
};
/* 
 * class SpoolListEntry(sle)
 * 	list of log records spooled for a directory vnode
 *	all these records are appended to the directory log during
 *	transaction commmit time 
 */
struct sle : public olink {
    int rec_index;
    
    sle(int i) {
	rec_index = i;
    }
};
    
/* procedure declarations */
extern void InitLogStorage();
extern pdlist *AllocateVMResLogList(int, VnodeId, Unique_t);
extern void DeAllocateVMResLogListHeader(int, VnodeId, Unique_t);
extern pdlist *GetResLogList(int, VnodeId, Unique_t, VNResLog **);
extern pdlist *iGetResLogList(int, int, Unique_t, VNResLog **);
extern void CreateAfterCrashLogRecord(Vnode *, pdlist *);
extern void TruncResLog(int, VnodeId, Unique_t);
extern void iTruncResLog(int, int, Unique_t);
extern void PurgeLog(int , int , int , int );
extern void PurgeLog(int, VnodeId, Unique_t);
extern void PrintResLog(pdlist *, FILE *);
extern void PrintResLog(int , VnodeId , Unique_t , FILE *);
extern int  InitVMLogRecord(int, ViceFid *, ViceStoreId *, int  ...);
extern int AppendRVMLogRecord(Vnode *, int);
extern int SpoolLogRecord(int, ViceFid *, ViceStoreId *, int op ...);
extern void PrintResLog(int , VnodeId , Unique_t , FILE *);
extern void fPrintResLog(pdlist *, FILE *);
extern void DumpVolResLog(PMemMgr *, int);
extern void ReadVolResLog(PMemMgr **, int);
extern void htonVolResLog(char *, int);
extern void ntohVolResLog(char *, int);
extern void GetResStatistics(PMemMgr *, int *, int *, int *);
extern VolumeId VolindexToVolid(int);
#endif not _RESLOG_H_

