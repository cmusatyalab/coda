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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/venus.private.h,v 4.22 1998/12/14 19:00:23 smarc Exp $";
#endif /*_BLURB_*/


/*
 *
 * Manifest constants for Venus, plus declarations for source files without their own headers.
 *
 */


#ifndef	_VENUS_PRIVATE_H_
#define _VENUS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include "coda_assert.h"
#include <ctype.h>
#include <errno.h>

#include <lock.h>
#include <rpc2.h>
#include <util.h>
#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "venusstats.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define	EMULTRSLTS	ETOOMANYREFS	    /* external */
#define	ESYNRESOLVE	155		    /* internal */
#define	EASYRESOLVE	156		    /* internal */
#define	ERETRY		157		    /* internal */
#define	EVOLUME		158		    /* internal */
/* The next two are internal, but defined in other modules. */
/*
#define	EINCOMPATIBLE	198
#define	EINCONS		199
*/
/* added for implementing ASRs.  Used to tell the vfs layer that 
   an ASR was started and it should block */
#define EASRSTARTED     200


/*  *****  Command-line/vstab parameter defaults.  ***** */

#define	CODADIR	"/usr/coda"
#define	VSTAB	"/usr/coda/etc/vstab"
#define DFLT_CONSOLE "/usr/coda/etc/console"/* console file */
#define UNSET_CONSOLE 0
#define	DFLT_VR	"/coda"			    /* venus root */
#define	UNSET_VR 0
#define	DFLT_KD	"/dev/cfs0"		    /* kernel pseudo-device */
#define	UNSET_KD 0
#define	DFLT_FS	""	    /* file servers */
#define	UNSET_FS 0
#if defined(DJGPP) 
#define	DFLT_CD	"C:/usr/coda/venus.cache"    /* Win cache directory */
#elif  defined(__CYGWIN32__)
#define	DFLT_CD	"C:/usr/coda/venus.cache"    /* Win cache directory */
#else 
#define	DFLT_CD	"/usr/coda/venus.cache"	    /* cache directory */
#endif

#if defined(__CYGWIN32__)
#define CF_PREFIX "/DosDevices/"
#else
#define CF_PREFIX ""
#endif

#define	UNSET_CD 0
const int DFLT_CB = 8192;		    /* cache blocks */
const int UNSET_CB = -1;
const int MIN_CB = 2048;
#define	UNSET_RV 0
#define UNSET_PRIMARYUSER -1		    /* primary user of this machine */
#define DFLT_SPOOLDIR "/usr/coda/spool"
#define UNSET_SPOOLDIR 0

const int FREE_FACTOR = 16;


/*  *****  Manifest constants for Venus.  *****  */
const int MAXHOSTS = 8;	/* The number of hosts we generally try to parse in a host list.  S/B in vice.h! */
const int NFDS = 32;	/* IOMGR-enforced limit!  Kernel may allocate fds numbered higher than this! */
/* definition of vuid_t that used to be here has been moved to vicedep/vcrcommon.rpc2  (Satya 3/23/92) */
#ifdef DJGPP
const vuid_t V_UID = (vuid_t)500;    /* UID that the venus process runs under. */
#else
const vuid_t V_UID = (vuid_t)0;	    /* UID that the venus process runs under. */
#endif
/* Group id fields are 32 bits in BSD44 (not 16 bits); the use of a small 
   negative number (-2) means its unsigned long representation is huge
   (4294967294).  This causes the "ar" program to screw up because it
   blindly does a sprintf() of the gid into the ".a" file. (Satya, 1/11/97) */
/* In linux kernel, gid_t is unsigned short, but in venus vgid_t is
   unsigned int which is 32-bit, so we also need to hardcode the number
   here.  (Clement 6/10/97) */
const vuid_t V_GID = (vuid_t)65534;    /* GID that the venus process runs under. */
const vuid_t ALL_UIDS = (vuid_t)-1;
const vuid_t HOARD_UID = (vuid_t)-2; /* uid of hoard daemon */
const vuid_t UNSET_UID = (vuid_t)-666; /* beastly but recognizable */
const unsigned short V_MODE = 0600;
const int OWNERBITS = 0700;
const int OWNERREAD = 0400;
const int OWNERWRITE = 0200;
const int OWNEREXEC = 0100;
const unsigned long NO_HOST = (unsigned long)-1;
const int V_MAXACLLEN = 1000;
const int V_BLKSIZE = 8192;
const TIMERINTERVAL = 5;
const int GETDATA = 1;
#define	ALL_FIDS    (&NullFid)
typedef void (*PROC_V_UL)(unsigned long);
#define	STREQ(a, b) (strcmp((a), (b)) == 0)
#define	STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define	NBLOCKS(bytes)	((bytes + 1023) >> 10)
#define	LOGFILE	    "venus.log"
#define	LOGFILE_OLD "venus.log.old"

/* Flags for the various vproc/fsobj name/object lookup routines. */
#define	FOLLOW_SYMLINKS	0x1	    /* should lookup follow symlinks for last component? */
#define	TRAVERSE_MTPTS	0x2	    /* should lookup cross covered mount points? */
#define	REFERENCE	0x8	    /* should cache references be noted? */


/*  *****  Debugging macros.  *****  */
#ifdef	VENUSDEBUG
#define	LOG(level, stmt)    if (LogLevel >= (level)) dprint stmt
#else	VENUSDEBUG
#define	LOG(level, stmt)
#endif	VENUSDEBUG

/*  *****  Locking macros.  *****  */

enum LockLevel { NL, RD, SH, WR };

#define ObtainLock(lock, level)\
{\
    switch(level) {\
	case RD:\
	    ObtainReadLock(lock);\
	    break;\
\
	case SH:\
	    ObtainSharedLock(lock);\
	    break;\
\
	case WR:\
	    ObtainWriteLock(lock);\
	    break;\
\
	case NL:\
	default:\
	    CODA_ASSERT(0);\
    }\
}

#define ReleaseLock(lock, level)\
{\
    switch(level) {\
	case RD:\
	    ReleaseReadLock(lock);\
	    break;\
\
	case SH:\
	    ReleaseSharedLock(lock);\
	    break;\
\
	case WR:\
	    ReleaseWriteLock(lock);\
	    break;\
\
	case NL:\
	default:\
	    CODA_ASSERT(0);\
    }\
}


/*  *****  Timing macros.  *****  */
#ifdef	TIMING
#define SubTimes(end, start)\
    (((end).tv_sec - (start).tv_sec) * 1000 + ((end).tv_usec - (start).tv_usec) / 1000)
#define	START_TIMING()\
    struct timeval StartTV, EndTV;\
    gettimeofday(&StartTV, 0);
/*
    struct rusage StartRU, EndRU;\
    getrusage(RUSAGE_SELF, &StartRU);
*/

#define END_TIMING()\
    gettimeofday(&EndTV, 0);\
    float elapsed; elapsed = SubTimes(EndTV, StartTV);\
    float elapsed_ru_utime; elapsed_ru_utime = 0.0;\
    float elapsed_ru_stime; elapsed_ru_stime = 0.0;
/*
    getrusage(RUSAGE_SELF, &EndRU);\
    float elapsed_ru_utime; elapsed_ru_utime = SubTimes(EndRU.ru_utime, StartRU.ru_utime);\
    float elapsed_ru_stime; elapsed_ru_stime = SubTimes(EndRU.ru_stime, StartRU.ru_stime);
*/
#else	TIMING
#define	SubTimes(end, start)	(0.0)
#define START_TIMING()
#define END_TIMING()\
    float elapsed; elapsed = 0.0;\
    float elapsed_ru_utime; elapsed_ru_utime = 0.0;\
    float elapsed_ru_stime; elapsed_ru_stime = 0.0;
#endif	TIMING


/*  *****  Cache Stuff *****  */
enum CacheType {    ATTR,
		    DATA
};

#undef WRITE

enum CacheEvent	{   HIT,
		    MISS,
		    RETRY,
		    TIMEOUT,
		    NOSPACE,
		    FAILURE,
		    CREATE,
		    WRITE,
		    REMOVE,
		    REPLACE
};

/* "blocks" field is not relevant for all events */
/* "blocks" is constant for (relevant) ATTR events */
/* Now the same struct named CacheEventEntry is generated from mond.rpc2 */
struct CacheEventRecord {
   int count;
   int blocks;
};

struct CacheStats {
    CacheEventRecord events[10];	    /* indexed by CacheEvent type! */
};


/*  *****  Misc stuff  *****  */
#define TRANSLATE_TO_LOWER(s)\
{\
    for (char *c = s; *c; c++)\
	if (isupper(*c)) *c = tolower(*c);\
}
#define TRANSLATE_TO_UPPER(s)\
{\
    for (char *c = s; *c; c++)\
	if (islower(*c)) *c = toupper(*c);\
}

#define CHOKE(me...) choke(__FILE__, __LINE__, ##me)

/*  *****  Declarations for source files without their own headers.  ***** */
extern void dprint(char * ...);
extern void choke(char *file, int line, char* ...);
extern void rds_printer(char * ...);
extern void VenusPrint(int, char **);
extern void VenusPrint(FILE *, int, char **);
extern void VenusPrint(int, int, char **);
extern char *VenusOpStr(int);
extern char *IoctlOpStr(int);
extern char *VenusRetStr(int);
extern void VVPrint(FILE *, vv_t **);
extern int binaryfloor(int);
extern void LogInit();
extern void DebugOn();
extern void DebugOff();
extern void Terminate();
extern void DumpState();
extern void RusagePrint(int);
extern void VFSPrint(int);
extern void RPCPrint(int);
extern void GetCSS(RPCPktStatistics *);
extern void SubCSSs(RPCPktStatistics *, RPCPktStatistics *);
extern void MallocPrint(int);
extern void StatsInit();
extern void ProfInit();
extern void ToggleProfiling();
extern void SwapLog();
extern void ToggleMallocTrace();
extern char *lvlstr(LockLevel);
extern int GetTime(long *, long *);
extern long Vtime();
extern int Fid_Compare(ViceFid *, ViceFid *);
extern int FAV_Compare(ViceFidAndVV *, ViceFidAndVV *);
extern void DaemonInit();
extern void RegisterDaemon(unsigned long, char *);
extern void DispatchDaemons();
extern FILE *logFile;
extern int LogLevel;
extern long int RPC2_DebugLevel;
extern long int SFTP_DebugLevel;
extern long int RPC2_Trace;
extern int ProfBoot;
extern int MallocTrace;
extern int Profiling;
extern ViceFid NullFid;		    /* should be const -JJK */
extern vv_t NullVV;		    /* should be const -JJK */
extern VFSStatistics VFSStats;
extern RPCOpStatistics RPCOpStats;
extern struct timeval DaemonExpiry;

/* venus.c */
class vproc;
extern vproc *Main;
extern ViceFid rootfid;
extern long rootnodeid;
extern int CleanShutDown;
extern char *venusRoot;
extern char *kernDevice;
extern char *fsname;
extern char *CacheDir;
extern int CacheBlocks;
extern char *RootVolName;
extern int PrimaryUser;

/* spool.cc */
extern void SpoolInit();
extern void MakeUserSpoolDir(char *, vuid_t);


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

extern void MallocStats(char *, FILE *);
extern long CheckAllocs(char *);
extern void plumber(FILE *);

#ifdef __cplusplus
}
#endif __cplusplus


#endif not _VENUS_PRIVATE_H_
