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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/vicedep/srv.h,v 1.1 1996/11/22 19:09:34 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#ifndef	_VICE_SRV_H_
#define	_VICE_SRV_H_	1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/file.h>
#include <stdio.h>
#include <errno.h>

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <prs.h>
#include <prs_fs.h>
#include <nfs.h>
#include <errors.h>
#include <inconsist.h>
#include <vice.h>
#include <cvnode.h>
#include <volume.h>

/* From Vol package. */
#define	ThisHostAddr		((unsigned long)(HostAddress[ThisServerId]))
#define	VolToHostAddr(volnum)	((unsigned long)(HostAddress[(volnum) >> 24]))
#define NO_LOCK		0	/* remains for compatibility */
#define	VOL_NO_LOCK	NO_LOCK
#define VOL_SHARED_LOCK	SHARED_LOCK
#define VOL_EXCL_LOCK	WRITE_LOCK

#define ISDIR(fid)  ((fid).Vnode & 1)

extern bit32 HostAddress[];
extern int ThisServerId;
extern long rvm_no_yield; 

extern void VAdjustDiskUsage(Error *, Volume *, int);
extern int  VCheckVLDB();
extern void VPrintCacheStats(FILE * =stdout);
extern void VPrintDiskStats(FILE * =stdout);
extern void ViceUpdateDB();
extern void SwapLog();
extern void SwapMalloc();
extern void ViceTerminate();


#define	EINCOMPATIBLE	198	/* Move to inconsist.h! -JJK */

#define VSLEEP(seconds)\
{\
    struct timeval delay;\
    delay.tv_sec = seconds;\
    delay.tv_usec = 0;\
    IOMGR_Select(0, 0, 0, 0, &delay);\
}

#define	STREQ(a, b) (strcmp((a), (b)) == 0)
#define	STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)

#define FID_EQ(a, b)\
    ((a).Volume == (b).Volume && (a).Vnode == (b).Vnode && (a).Unique == (b).Unique)

#define FID_LT(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) < ((b).Unique)))

#define FID_LTE(a, b)\
    /* Assumes that ((a).Volume == (b).Volume)! */\
    ((((a).Vnode) < ((b).Vnode)) || ((a).Vnode == (b).Vnode && ((a).Unique) <= ((b).Unique)))

#define	SID_EQ(a, b)	((a).Host == (b).Host && (a).Uniquifier == (b).Uniquifier)

#define	SetAccessList(vptr, ACL, ACLSize)\
{\
    assert((vptr)->disk.type == vDirectory);\
    (ACL) = VVnodeACL((vptr));\
    (ACLSize) = VAclSize((vptr));\
}

#define MAXHOSTTABLEENTRIES 1000
#define MAXNAMELENGTH 64
#define MAXHOSTLENGTH 32

/* from vice/srvproc.c */
#define	EMPTYFILEBLOCKS	    1
#define	EMPTYDIRBLOCKS	    2
#define	EMPTYSYMLINKBLOCKS  1



typedef struct HostTable {
    RPC2_Handle id;			/* cid for call back connection	*/
    unsigned int	host;		/* IP address of host		*/
    unsigned int	port;		/* port address of host		*/
    unsigned int	LastCall;	/* time of last call from host	*/
    unsigned int	ActiveCall;	/* time of any call but gettime	*/
    struct ClientEntry	*FirstClient;	/* first connection from host	*/
    struct Lock		lock;		/* lock used for client sync	*/
    char HostName[MAXHOSTLENGTH];	/* name of workstation		*/
}   HostTable;


typedef struct ClientEntry {
    RPC2_Handle		RPCid;			/* cid for connection	*/
    PRS_InternalCPS	*CPS;			/* cps for authentication */
    RPC2_Integer	Id;			/* Vice ID of user	*/
    RPC2_Integer	SecurityLevel;		/* Security level of conn */
    int			SEType;			/* Type of side effect */
    unsigned int	LastCall;		/* time of last call	*/
    HostTable		*VenusId;		/* ptr to host entry	*/
    RPC2_Integer	LastOp;			/* op code of last call */
    int			DoUnbind;		/* true if Unbind needed */
    struct ClientEntry	*NextClient;		/* next entry for host	*/
    char		UserName[MAXNAMELENGTH]; /* name of user	*/
} ClientEntry;


#define NEWCONNECT "NEWCONNECT"
#define MAXMSG 100
#define MAXMSGLN 128

/* first srvOPARRAYSIZE reserved for Vice operations */
#define MAXCNTRS (srvOPARRAYSIZE+17)
#define TOTAL 0

#define CONNECT ViceConnectFS_OP
#define DISCONNECT ViceDisconnectFS_OP
//#define FETCH ViceFetch_OP
//#define STORE ViceStore_OP
#define REMOVE ViceVRemove_OP
#define CREATE ViceVCreate_OP
#define RENAME ViceVRename_OP
#define SYMLINK ViceVSymLink_OP
#define LINK ViceVLink_OP
#define MAKEDIR ViceVMakeDir_OP
#define REMOVEDIR ViceVRemoveDir_OP
#define REMOVECALLBACK ViceRemoveCallBack_OP
#define SETLOCK ViceSetLock_OP
#define RELEASELOCK ViceReleaseLock_OP
#define GETROOTVOLUME ViceGetRootVolume_OP
#define SETROOTVOLUME ViceSetRootVolume_OP
#define GETVOLUMESTAT ViceGetVolumeStatus_OP
#define SETVOLUMESTAT ViceSetVolumeStatus_OP
#define GETTIME ViceGetTime_OP
#define GETSTATISTICS ViceGetStatistics_OP
#define GETVOLUMEINFO ViceGetVolumeInfo_OP
#define RESOLVE ViceResolve_OP
#define REPAIR ViceRepair_OP
#define SETVV ViceSetVV_OP
#define REINTEGRATE ViceVIncReintegrate_OP
#define ALLOCFIDS ViceAllocFids_OP
#define VALIDATEATTRS ViceValidateAttrs_OP
#define NEWCONNECTFS ViceNewConnectFS_OP
#define GETVOLVS ViceGetVolVS_OP
#define VALIDATEVOLS ViceValidateVols_OP

#define FETCHDATAOP (srvOPARRAYSIZE+1)
#define FETCHDATA (srvOPARRAYSIZE+2)
#define FETCHD1 (srvOPARRAYSIZE+3)
#define FETCHD2 (srvOPARRAYSIZE+4)
#define FETCHD3 (srvOPARRAYSIZE+5)
#define FETCHD4 (srvOPARRAYSIZE+6)
#define FETCHD5 (srvOPARRAYSIZE+7)
#define FETCHTIME (srvOPARRAYSIZE+8)
#define STOREDATAOP (srvOPARRAYSIZE+9)
#define STOREDATA (srvOPARRAYSIZE+10)
#define STORED1 (srvOPARRAYSIZE+11)
#define STORED2 (srvOPARRAYSIZE+12)
#define STORED3 (srvOPARRAYSIZE+13)
#define STORED4 (srvOPARRAYSIZE+14)
#define STORED5 (srvOPARRAYSIZE+15)
#define STORETIME (srvOPARRAYSIZE+16)
#define SIZE1 1024
#define SIZE2 SIZE1*8
#define SIZE3 SIZE2*8
#define SIZE4 SIZE3*8


/* Timing macros. */
#define SubTimes(end, start)\
     ((end.tv_usec > start.tv_usec) ? \
     (((float)(end.tv_sec - start.tv_sec) * 1000) + (float)((end.tv_usec - start.tv_usec) / 1000)):\
     ((float)((end.tv_sec - start.tv_sec - 1) * 1000) + (float)((end.tv_usec + 1000000 - start.tv_usec)/1000)))

/* Macro to perform time_value subtraction t3 = t1 - t2 */
/* Assume t1 > t2 always */
#define	time_value_sub(t1, t2, t3)	{   \
if ((t1).microseconds < (t2).microseconds){	\
    (t1).microseconds += 1000000;				\
    (t1).seconds -= 1;						\
}								\
(t3).microseconds = (t1).microseconds - (t2).microseconds;	\
(t3).seconds = (t1).seconds - (t2).seconds;			\
}
#ifdef CODA_DEBUG
#define	START_TIMING(id)\
    struct timeval start##id, end##id;\
    if(SrvDebugLevel > 1)\
	gettimeofday(&start##id, 0);
#define END_TIMING(id)\
    if(SrvDebugLevel > 1)\
	gettimeofday(&end##id, 0);\
	LogMsg(2, SrvDebugLevel, stdout, "%s: start:(%#x.%x) end:(%#x.%x)\n", #id, start##id.tv_sec, start##id.tv_usec, end##id.tv_sec, end##id.tv_usec);\
    LogMsg(2, SrvDebugLevel, stdout, "%s: elapsed = %7.1f", #id, (float)SubTimes(end##id, start##id));
#else
#define	START_TIMING(id)
#define END_TIMING(id)
#endif


/* ViceErrorMsg.c */
extern char *ViceErrorMsg(int);

/* clientproc.c */
extern int BuildClient(RPC2_Handle, char *, RPC2_Integer, ClientEntry **);
extern int MakeCallBackConn(ClientEntry *);
extern void DeleteClient(ClientEntry *);
extern void CallBackCheck();
extern void CleanUpHost(HostTable *);
extern void PrintClients();
extern void SetUserName(ClientEntry *);
extern void GetWorkStats(int *, int *, unsigned int);
extern HostTable *FindHostEntry(RPC2_Handle);

/* codaproc.c */
extern ViceVersionVector NullVV;
extern long InternalCOP2(RPC2_Handle, ViceStoreId *, ViceVersionVector *);
extern void NewCOP1Update(Volume *, Vnode *, ViceStoreId *, RPC2_Integer * =NULL);
extern void COP2Update(Volume *, Vnode *, ViceVersionVector *);
extern long InternalCOP2(RPC2_Handle, ViceStoreId *, ViceVersionVector *);
extern void PollAndYield();
extern int GetSubTree(ViceFid *, Volume *, dlist *);
extern void GetMyVS(Volume *, RPC2_CountedBS *, RPC2_Integer *);
extern void SetVSStatus(ClientEntry *, Volume *, RPC2_Integer *, CallBackStatus *);

/* codaproc2.c */
extern int LookupChild(Volume *, Vnode *, char *, ViceFid *);
extern int AddChild(Volume **, dlist *, ViceFid *, char *, int =0);

/* codasrv.c */
extern int SystemId;
extern int SrvDebugLevel;
extern unsigned StartTime;
extern int CurrentConnections;
extern int Authenticate;
extern int Counters[];
extern ViceFid NullFid;
#ifdef	PERFORMANCE
thread_t *lwpth;
thread_array_t thread_list;
int thread_count;
#endif	PERFORMANCE

extern void Die(char *);

/* srv.c */
extern void SetStatus(Vnode *, ViceStatus *, Rights, Rights);
extern int GetRights (PRS_InternalCPS *, AL_AccessList *, int, Rights *, Rights *);
extern int GetFsObj(ViceFid *, Volume **, Vnode **, int, int, int , int);
extern int SystemUser(ClientEntry *);
extern int AdjustDiskUsage(Volume *, int);
extern void ChangeDiskUsage(Volume *, int);
extern int GetVolObj(VolumeId, Volume **, int, int =0, int =0);
extern void PutVolObj(Volume **, int, int =0);
extern int CheckDiskUsage(Volume *, int );
extern void PrintCounters(FILE *fp =stdout);

/* srvproc2.c */
extern int supported;
extern unsigned int etherWrites;
extern unsigned int etherRetries;
extern unsigned int etherInterupts;
extern unsigned int etherGoodReads;
extern unsigned int etherBytesRead;
extern unsigned int etherBytesWritten;
extern int  GetEtherStats();

/* vicecb.c */
extern int CBEs;
extern int CBEBlocks;
extern int FEs;
extern int FEBlocks;
extern int InitCallBack();
extern CallBackStatus AddCallBack(HostTable *, ViceFid *);
extern void BreakCallBack(HostTable *, ViceFid *);
extern void DeleteCallBack(HostTable *, ViceFid *);
extern void DeleteVenus(HostTable *);
extern void DeleteFile(ViceFid *);
extern void PrintCallBackState(FILE *);
extern void PrintCallBacks(ViceFid *, FILE *);
extern CallBackStatus CodaAddCallBack(HostTable *, ViceFid *, VolumeId);
extern void CodaBreakCallBack(HostTable *, ViceFid *, VolumeId);
extern void CodaDeleteCallBack(HostTable *, ViceFid *, VolumeId);

/* resolution */
extern int AllowResolution;

/* coppend.c */
extern void AddToCopPendingTable(ViceStoreId *, ViceFid *);

// volutil
extern void SetDirHandle(DirHandle *, Vnode *);

#endif	not _VICE_SRV_H_
