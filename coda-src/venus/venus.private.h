/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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

#ifndef _VENUS_PRIVATE_H_
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

/* from util */
#include <venusconf.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Replica Control Rights. */
/* Note that we presently do not distinguish between read and write rights. */
/* We may well do so in the future, however. */
#define RC_STATUSREAD 1
#define RC_STATUSWRITE 2
#define RC_STATUS (RC_STATUSREAD | RC_STATUSWRITE)
#define RC_DATAREAD 4
#define RC_DATAWRITE 8
#define RC_DATA (RC_DATAREAD | RC_DATAWRITE)

#define EMULTRSLTS ETOOMANYREFS /* external */
#define ESYNRESOLVE 155 /* internal */
#define EASYRESOLVE 156 /* internal */
#define ERETRY 157 /* internal */
/* The next three are internal, but defined in other modules. */
/*
#define	EVOLUME		158
#define	EINCOMPATIBLE	198
#define	EINCONS		199
*/
/* added for implementing ASRs.  Used to tell the vfs layer that
   an ASR was started and it should block */
#define EASRSTARTED 200

#define ASR_INTERVAL 300

const int FREE_FACTOR = 16;

/*  *****  Manifest constants for Venus.  *****  */
#ifdef __CYGWIN32__
extern uid_t V_UID; /* UID that the venus process runs under. */
#else
const uid_t V_UID = (uid_t)0; /* UID that the venus process runs under. */
#endif
/* Group id fields are 32 bits in BSD44 (not 16 bits); the use of a small
   negative number (-2) means its unsigned long representation is huge
   (4294967294).  This causes the "ar" program to screw up because it
   blindly does a sprintf() of the gid into the ".a" file. (Satya, 1/11/97) */
/* In linux kernel, gid_t is unsigned short, but in venus vgid_t is
   unsigned int which is 32-bit, so we also need to hardcode the number
   here.  (Clement 6/10/97) */
#if defined(__CYGWIN32__)
const gid_t V_GID = 513; /* GID that the venus process runs under. */
#else
const gid_t V_GID = 65534; /* GID that the venus process runs under. */
#endif
const uid_t ANYUSER_UID     = (uid_t)-1;
const uid_t HOARD_UID       = (uid_t)-2; /* uid of hoard daemon */
const uid_t UNSET_UID       = (uid_t)-666; /* beastly but recognizable */
const unsigned short V_MODE = 0600;
const int OWNERBITS         = 0700;
const int OWNERREAD         = 0400;
const int OWNERWRITE        = 0200;
const int OWNEREXEC         = 0100;
const uint32_t NO_HOST      = (uint32_t)-1;
const uint32_t V_MAXACLLEN  = 1000;
const int V_BLKSIZE         = 8192;
const int TIMERINTERVAL     = 5;
const int GETDATA           = 1;
#define ALL_FIDS (&NullFid)
typedef void (*PROC_V_UL)(unsigned long);
#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define NBLOCKS(bytes) ((bytes + 1023) >> 10)
#define NBLOCKS_BYTES(bytes) (NBLOCKS(bytes) << 10)

/* Flags for the various vproc/fsobj name/object lookup routines. */
#define FOLLOW_SYMLINKS \
    0x1 /* should lookup follow symlinks for last component? */
#define TRAVERSE_MTPTS 0x2 /* should lookup cross covered mount points? */
#define REFERENCE 0x8 /* should cache references be noted? */

/*  *****  Debugging macros.  *****  */
#ifdef VENUSDEBUG
#define LOG(level, stmt)              \
    do {                              \
        if (GetLogLevel() >= (level)) \
            dprint stmt;              \
    } while (0)
#else
#define LOG(level, stmt)
#endif /* !VENUSDEBUG */

/*  *****  Locking macros.  *****  */

enum LockLevel
{
    NL,
    RD,
    SH,
    WR
};

#define ObtainLock(lock, level)     \
    {                               \
        switch (level) {            \
        case RD:                    \
            ObtainReadLock(lock);   \
            break;                  \
                                    \
        case SH:                    \
            ObtainSharedLock(lock); \
            break;                  \
                                    \
        case WR:                    \
            ObtainWriteLock(lock);  \
            break;                  \
                                    \
        case NL:                    \
        default:                    \
            CODA_ASSERT(0);         \
        }                           \
    }

#define ReleaseLock(lock, level)     \
    {                                \
        switch (level) {             \
        case RD:                     \
            ReleaseReadLock(lock);   \
            break;                   \
                                     \
        case SH:                     \
            ReleaseSharedLock(lock); \
            break;                   \
                                     \
        case WR:                     \
            ReleaseWriteLock(lock);  \
            break;                   \
                                     \
        case NL:                     \
        default:                     \
            CODA_ASSERT(0);          \
        }                            \
    }

/*  *****  Timing macros.  *****  */
#ifdef TIMING
#define SubTimes(end, start)                        \
    ((((end)->tv_sec - (start)->tv_sec) * 1000.0) + \
     (((end)->tv_usec - (start)->tv_usec) / 1000.0))

#define START_TIMING()             \
    struct timeval StartTV, EndTV; \
    gettimeofday(&StartTV, 0);

#define END_TIMING()         \
    gettimeofday(&EndTV, 0); \
    float elapsed;           \
    elapsed = SubTimes(&EndTV, &StartTV);
#else
#define SubTimes(end, start) (0.0)
#define START_TIMING()
#define END_TIMING() float elapsed = 0.0;
#endif /* !TIMING */

/*  *****  Cache Stuff *****  */
enum CacheType
{
    ATTR,
    DATA
};

#undef WRITE

enum CacheEvent
{
    HIT,
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
    CacheEventRecord events[10]; /* indexed by CacheEvent type! */
};

/*  *****  Misc stuff  *****  */
#define TRANSLATE_TO_LOWER(s)      \
    {                              \
        for (char *c = s; *c; c++) \
            if (isupper(*c))       \
                *c = tolower(*c);  \
    }
#define TRANSLATE_TO_UPPER(s)      \
    {                              \
        for (char *c = s; *c; c++) \
            if (islower(*c))       \
                *c = toupper(*c);  \
    }

#define CHOKE(me...) choke(__FILE__, __LINE__, ##me)

/*  *****  Declarations for source files without their own headers.  ***** */
void dprint(const char *...);
void choke(const char *file, int line, const char *...);
void rds_printer(char *...);
void VenusPrint(int argc, const char **argv);
void VenusPrint(FILE *, int argc, const char **argv);
void VenusPrint(int, int argc, const char **argv);
const char *VenusOpStr(int);
const char *IoctlOpStr(unsigned char nr);
const char *VenusRetStr(int);
void VVPrint(FILE *, ViceVersionVector **);
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
const char *lvlstr(LockLevel);
int GetTime(long *, long *);
time_t Vtime();
int FAV_Compare(ViceFidAndVV *, ViceFidAndVV *);
void DaemonInit();
void FireAndForget(const char *name, void (*f)(void), int interval,
                   int stacksize = 32 * 1024);
void RegisterDaemon(unsigned long, char *);
void DispatchDaemons();

/* Helper to add a file descriptor with callback to main select loop. */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata);

static const VenusFid NullFid         = { 0, 0, 0, 0 };
static const ViceVersionVector NullVV = { { 0, 0, 0, 0, 0, 0, 0, 0 },
                                          { 0, 0 },
                                          0 };
extern VFSStatistics VFSStats;
extern RPCOpStatistics RPCOpStats;
extern struct timeval DaemonExpiry;

/* venus.c */
class vproc;
extern int parent_fd;
extern long rootnodeid;

/* spool.cc */
void MakeUserSpoolDir(char *, uid_t);

/* ASR misc */
/* Note: At some point, it would be nice to run ASRs in different
 * volumes concurrently. This requires replacing the globals
 * below with a table or other data structure. Due to token
 * assignment constraints, though, this is not possible as of 06/2006.
 */

struct MRPC_common_params {
    RPC2_Unsigned nservers;
    RPC2_Handle *handles;
    struct in_addr *hosts;
    RPC2_Integer *retcodes;
    int ph_ix;
    unsigned long ph;
    RPC2_Multicast *MIp;
};

FILE *GetLogFile();
int GetLogLevel();
void SetLogLevel(int loglevel);

#endif /* _VENUS_PRIVATE_H_ */