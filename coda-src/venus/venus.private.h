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


/*
 * Manifest constants for Venus, plus declarations for source files without
 * their own headers.
 */


#ifndef	_VENUS_PRIVATE_H_
#define _VENUS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>

#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <util.h>
#ifdef __cplusplus
}
#endif

#include "coda_assert.h"

/* interfaces */
#include <vice.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "venusstats.h"
#include "venusfid.h"

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

#define	VSTAB	"/usr/coda/etc/vstab"

#if defined(DJGPP)
#define DFLT_VR "N:"                     /* Venus Root */ 
#define MCFD    16                       /* Michael Callahan File Descriptor? */
#else
#define	DFLT_VR	"/coda"			 /* venus root */
#endif

#if defined(DJGPP) /* || defined(__CYGWIN32__) Not right now ... */
#define	DFLT_CD	"C:/usr/coda/venus.cache"    /* Win cache directory */
#else 
#define	DFLT_CD	"/usr/coda/venus.cache"	    /* cache directory */
#endif

const int MIN_CB = 2048;
#define UNSET_PRIMARYUSER 0		    /* primary user of this machine */

const int FREE_FACTOR = 16;


/*  *****  Manifest constants for Venus.  *****  */
const int NFDS = 32;	/* IOMGR-enforced limit!  Kernel may allocate fds numbered higher than this! */
#if defined(DJGPP) || defined(__CYGWIN32__)
extern uid_t V_UID;    /* UID that the venus process runs under. */
#else
const uid_t V_UID = (uid_t)0;	    /* UID that the venus process runs under. */
#endif
/* Group id fields are 32 bits in BSD44 (not 16 bits); the use of a small 
   negative number (-2) means its unsigned long representation is huge
   (4294967294).  This causes the "ar" program to screw up because it
   blindly does a sprintf() of the gid into the ".a" file. (Satya, 1/11/97) */
/* In linux kernel, gid_t is unsigned short, but in venus vgid_t is
   unsigned int which is 32-bit, so we also need to hardcode the number
   here.  (Clement 6/10/97) */
#if defined(__CYGWIN32__)
const gid_t V_GID = 513;    /* GID that the venus process runs under. */
#else
const gid_t V_GID = 65534;    /* GID that the venus process runs under. */
#endif
const uid_t ANYUSER_UID = (uid_t)-1;
const uid_t HOARD_UID = (uid_t)-2; /* uid of hoard daemon */
const uid_t UNSET_UID = (uid_t)-666; /* beastly but recognizable */
const unsigned short V_MODE = 0600;
const int OWNERBITS = 0700;
const int OWNERREAD = 0400;
const int OWNERWRITE = 0200;
const int OWNEREXEC = 0100;
const unsigned long NO_HOST = (unsigned long)-1;
const int V_MAXACLLEN = 1000;
const int V_BLKSIZE = 8192;
const int TIMERINTERVAL = 5;
const int GETDATA = 1;
#define	ALL_FIDS    (&NullFid)
typedef void (*PROC_V_UL)(unsigned long);
#define	STREQ(a, b)		(strcmp((a), (b)) == 0)
#define	STRNEQ(a, b, n)		(strncmp((a), (b), (n)) == 0)
#define	NBLOCKS(bytes)		((bytes + 1023) >> 10)
#define	NBLOCKS_BYTES(bytes)	(NBLOCKS(bytes) << 10)

/* Flags for the various vproc/fsobj name/object lookup routines. */
#define	FOLLOW_SYMLINKS	0x1	    /* should lookup follow symlinks for last component? */
#define	TRAVERSE_MTPTS	0x2	    /* should lookup cross covered mount points? */
#define	REFERENCE	0x8	    /* should cache references be noted? */


/*  *****  Debugging macros.  *****  */
#ifdef	VENUSDEBUG
#define	LOG(level, stmt)    if (LogLevel >= (level)) dprint stmt
#else
#define	LOG(level, stmt)
#endif /* !VENUSDEBUG */

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
#define SubTimes(end, start) \
    ((((end)->tv_sec  - (start)->tv_sec)  * 1000.0) + \
     (((end)->tv_usec - (start)->tv_usec) / 1000.0))

#define	START_TIMING()\
    struct timeval StartTV, EndTV;\
    gettimeofday(&StartTV, 0);
/*
    struct rusage StartRU, EndRU;\
    getrusage(RUSAGE_SELF, &StartRU);
*/

#define END_TIMING()\
    gettimeofday(&EndTV, 0);\
    float elapsed, elapsed_ru_utime, elapsed_ru_stime; \
    elapsed = SubTimes(&EndTV, &StartTV); \
    elapsed_ru_utime = elapsed_ru_stime = 0.0;
/*
    getrusage(RUSAGE_SELF, &EndRU); \
    elapsed_ru_utime = SubTimes(&(EndRU.ru_utime), &(StartRU.ru_utime)); \
    elapsed_ru_stime = SubTimes(&(EndRU.ru_stime), &(StartRU.ru_stime));
*/
#else
#define	SubTimes(end, start) (0.0)
#define START_TIMING()
#define END_TIMING()\
    float elapsed = 0.0;\
    float elapsed_ru_utime = 0.0;\
    float elapsed_ru_stime = 0.0;
#endif /* !TIMING */


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
void dprint(char * ...);
void choke(char *file, int line, char* ...);
void rds_printer(char * ...);
void VenusPrint(int, char **);
void VenusPrint(FILE *, int, char **);
void VenusPrint(int, int, char **);
char *VenusOpStr(int);
char *IoctlOpStr(int);
char *VenusRetStr(int);
void VVPrint(FILE *, vv_t **);
int binaryfloor(int);
void LogInit();
void DebugOn();
void DebugOff();
void Terminate();
void DumpState();
void RusagePrint(int);
void VFSPrint(int);
void RPCPrint(int);
void GetCSS(RPCPktStatistics *);
void SubCSSs(RPCPktStatistics *, RPCPktStatistics *);
void MallocPrint(int);
void StatsInit();
void SwapLog();
void ToggleMallocTrace();
char *lvlstr(LockLevel);
int GetTime(long *, long *);
time_t Vtime();
int FAV_Compare(ViceFidAndVV *, ViceFidAndVV *);
void DaemonInit();
void FireAndForget(char *name, void (*f)(void), int interval,
		   int stacksize=32*1024);
void RegisterDaemon(unsigned long, char *);
void DispatchDaemons();

extern FILE *logFile;
extern int LogLevel;
extern long int RPC2_DebugLevel;
extern long int SFTP_DebugLevel;
extern long int RPC2_Trace;
extern int MallocTrace;
extern const VenusFid NullFid;
extern const vv_t NullVV;
extern VFSStatistics VFSStats;
extern RPCOpStatistics RPCOpStats;
extern struct timeval DaemonExpiry;

/* venus.c */
class vproc;
extern vproc *Main;
extern VenusFid rootfid;
extern long rootnodeid;
extern int CleanShutDown;
extern char *venusRoot;
extern char *kernDevice;
extern char *realmtab;
extern char *CacheDir;
extern char *CachePrefix;
extern int CacheBlocks;
extern char *SpoolDir;
extern uid_t PrimaryUser;
extern char *VenusPidFile;
extern char *VenusControlFile;
extern char *VenusLogFile;
extern char *consoleFile;
extern char *MarinerSocketPath;
extern int   mariner_tcp_enable;
extern int   allow_reattach;
extern int   masquerade_port;
extern int   PiggyValidations;
extern int   T1Interval;

/* spool.cc */
extern void MakeUserSpoolDir(char *, uid_t);

#endif /* _VENUS_PRIVATE_H_ */
