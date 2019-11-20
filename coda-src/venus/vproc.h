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
 *
 * Specification of the Venus process abstraction
 *
 */

#ifndef _VENUS_PROC_H_
#define _VENUS_PROC_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>

#include <coda.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
/* interfaces */
#include <vice.h>

/* from rvm */
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

/* from util */
#include <olist.h>
#include <dlist.h>
#include <rec_dlist.h>

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "fso_cachefile.h"
#include "venus.private.h"

/* string with counts */
struct coda_string {
    int cs_len;
    int cs_maxlen;
    char *cs_buf;
};

/* Forward declarations. */
struct uarea;
class vproc;
class vproc_iterator;

/* C++ Bogosity. */
extern void PrintVprocs();
extern void PrintVprocs(FILE *);
extern void PrintVprocs(int);

/* *****  Exported constants  ***** */

const int VPROC_DEFAULT_STACK_SIZE = 16384;
const int RETRY_LIMIT              = 10;

/* *****  Exported types  ***** */

/* local-repair modification */
enum vproctype
{
    VPT_Main,
    VPT_Worker,
    VPT_Mariner,
    VPT_CallBack,
    VPT_HDBDaemon,
    VPT_Reintegrator,
    VPT_Resolver,
    VPT_FSODaemon,
    VPT_ProbeDaemon,
    VPT_VSGDaemon,
    VPT_VolDaemon,
    VPT_UserDaemon,
    VPT_RecovDaemon,
    VPT_VmonDaemon,
    VPT_AdviceDaemon,
    VPT_LRDaemon,
    VPT_Daemon
};

/* Holds user/call specific context. */
class namectxt;
class volent;
struct uarea {
    int u_error; /* implicit return code */
    uid_t u_uid; /* implicit user identifier */
    int u_priority; /* to be used in resource requests */
    VenusFid u_cdir; /* for name lookup */
    int u_flags; /*  "	" */
    namectxt *u_nc; /*  "	" */
    volent *u_vol; /* for volume-level concurrency control */
    int u_volmode; /*  "	" */
    int u_vfsop; /* vfs operation in progress */
#ifdef TIMING
    struct timeval u_tv1; /* for recording elapsed time */
    struct timeval u_tv2; /*  "	" */
#endif /* TIMING */
    char *u_resblk; /* block to wait on for resolves */
    int u_rescnt; /* safeguard against infinite retry loops */
    int u_retrycnt; /* safeguard against infinite retry loops */
    int u_wdblkcnt; /* safeguard against infinite retry loops */

    int u_pid; /* the process id of the calling process */
    int u_pgid; /* the process group id of the calling process */

    /* Initialization. */
    void Init()
    {
        memset((void *)this, 0, (int)sizeof(struct uarea));
        u_volmode = /*VM_UNSET*/ -1;
        u_vfsop   = /*VFSOP_UNSET*/ -1;
    }
};

typedef void (*PROCBODY)(void);

class vproc : public olink {
    friend void VprocInit();
    friend void Rtry_Wait();
    friend void Rtry_Signal();
    friend vproc *FindVproc(int);
    friend void VprocPreamble(void *);
    friend vproc *VprocSelf();
    friend int VprocIdle();
    friend int VprocInterrupted();
    friend void PrintVprocs(int);
    friend class vproc_iterator;
    friend void PrintWorkers(int);
    friend void PrintMariners(int);

private:
    static olist tbl;
    static int counter;
    static char rtry_sync;
    static int redzone_limit;
    static int yellowzone_limit;

    void do_ioctl(VenusFid *fid, unsigned char nr, struct ViceIoctl *data);

    void init(void);

protected:
    PROCESS lwpid;
    char *name;
    PROCBODY func; /* function should be set if vproc::main isn't overloaded */
    int vpid;
    rvm_perthread_t rvm_data;
    struct Lock init_lock;
    static const char *venusRoot;
    static VenusFid rootfid;
    static int MaxWorkers;
    static int MaxPrefetchers;

    /* derived classes should call this function once they have finished their
     * constructor. */
    void start_thread(void);

    /* entry point, should be overloaded by derived classes */
    virtual void main(void);

public:
    /* Public for the time being. -JJK */
    vproctype type;
    int stacksize;
    int lwpri;
    int seq;
    struct uarea u;
    unsigned idle : 1;
    unsigned interrupted : 1;
    struct vcbevent *ve;

    vproc(const char *, PROCBODY, vproctype, int = VPROC_DEFAULT_STACK_SIZE,
          int = LWP_NORMAL_PRIORITY);
    vproc(vproc &); // not supported
    int operator=(vproc &); // not supported
    virtual ~vproc();

    /* Volume-level concurrency control. */
    void Begin_VFS(Volid *, int, int = -1);
    void Begin_VFS(VenusFid *fid, int op, int arg = -1)
    {
        Begin_VFS(MakeVolid(fid), op, arg);
    }
    void End_VFS(int * = 0);

    /* The vproc interface: mostly matching kernel requests.  */
    void root(struct venus_cnode *);
    void statfs(struct coda_statfs *);
    void sync();
    void vget(struct venus_cnode *, VenusFid *, int what = RC_STATUS);
    void open(struct venus_cnode *, int);
    void close(struct venus_cnode *, int);
    void ioctl(struct venus_cnode *, unsigned char nr, struct ViceIoctl *, int);
    void select(struct venus_cnode *, int);
    void getattr(struct venus_cnode *, struct coda_vattr *);
    void setattr(struct venus_cnode *, struct coda_vattr *);
    void access(struct venus_cnode *, int);
    void lookup(struct venus_cnode *, const char *, struct venus_cnode *, int);
    void create(struct venus_cnode *, char *, struct coda_vattr *, int, int,
                struct venus_cnode *);
    void remove(struct venus_cnode *, char *);
    void link(struct venus_cnode *, struct venus_cnode *, char *);
    void rename(struct venus_cnode *, char *, struct venus_cnode *, char *);
    void mkdir(struct venus_cnode *, char *, struct coda_vattr *,
               struct venus_cnode *);
    void rmdir(struct venus_cnode *, char *);
    void symlink(struct venus_cnode *, char *, struct coda_vattr *, char *);
    void readlink(struct venus_cnode *, struct coda_string *);
    void fsync(struct venus_cnode *);

    /**
     * Read file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes to be read from the file
     *
     */
    void read(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Write file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes to be written to the file
     *
     */
    void write(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Signal the end of a synchronous read file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes read from the file
     *
     */
    void read_finish(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Signal the end of a synchronous write file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes written to the file
     *
     */
    void write_finish(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Memory map file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes mapped into memory
     *
     */
    void mmap(struct venus_cnode *node, uint64_t pos, int64_t count);

    /* Pathname translation. */
    int namev(char *, int, struct venus_cnode *);
    void GetPath(VenusFid *, char *, int *, int = 1);
    static VenusFid &GetRootFid() { return vproc::rootfid; }
    const char *expansion(const char *path);
    void verifyname(char *name, int flags);
#define NAME_NO_DOTS 1 /* don't allow '.', '..', '/' */
#define NAME_NO_CONFLICT 2 /* don't allow @XXXXXXXX.YYYYYYYY.ZZZZZZZZ */
#define NAME_NO_EXPANSION 4 /* don't allow @cpu / @sys */

    void GetStamp(char *);
    void print();
    void print(FILE *);
    void print(int);
};

class vproc_iterator : public olist_iterator {
    vproctype type;

public:
    vproc_iterator(vproctype = (vproctype)-1);
    vproc *operator()();
};

/* *****  Exported routines  ***** */
void VPROC_printvattr(struct coda_vattr *vap);
extern void VprocInit();
extern void Rtry_Wait();
extern void Rtry_Signal();
extern vproc *FindVproc(int);
extern vproc *VprocSelf();
extern void VprocWait(const void *);
extern void VprocMwait(int, const void **);
extern void VprocSignal(const void *, int = 0);
extern void VprocSleep(struct timeval *);
extern void VprocYield();
extern int VprocSelect(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern void VprocSetRetry(int = -1, struct timeval * = 0);
extern int VprocIdle();
extern int VprocInterrupted();
//extern void PrintVprocs();
//extern void PrintVprocs(FILE *);
//extern void PrintVprocs(int);

/* Things which should be in vnode.h? -JJK */

extern void va_init(struct coda_vattr *);
extern long FidToNodeid(VenusFid *);

#define FTTOVT(ft)                         \
    ((ft) == (int)File ?                   \
         C_VREG :                          \
         (ft) == (int)Directory ? C_VDIR : \
                                  (ft) == (int)SymbolicLink ? C_VLNK : C_VREG)

/* Venus cnodes are a small placeholder structure to pass arguments
   into the output buffer back to the kernel without clobbering the
   inputbuffer, which is the same pointer as the output buffer.
*/

struct venus_cnode {
    u_short c_flags; /* flags (see below) */
    VenusFid c_fid; /* file handle */
    CacheFile *c_cf; /* container file object */
    int c_type;
};

#define MAKE_CNODE(vp, fid, type)            \
    {                                        \
        KernelToVenusFid(&(vp).c_fid, &fid); \
        (vp).c_type  = type;                 \
        (vp).c_flags = 0;                    \
    }

#define MAKE_CNODE2(vp, fid, type) \
    {                              \
        (vp).c_fid   = fid;        \
        (vp).c_type  = type;       \
        (vp).c_flags = 0;          \
    }

/* Venus cnode's c_flags */
#define C_FLAGS_INCON 0x2

/* Definitions of the value -1 with correct cast for different
   platforms, to be used in struct vattr to indicate a field to be
   ignored.  Used mostly in vproc::setattr() */

#define VA_IGNORE_FSID ((long)-1)
#define VA_IGNORE_ID ((unsigned long)-1)
#define VA_IGNORE_NLINK ((short)-1)
#define VA_IGNORE_BLOCKSIZE ((long)-1)
#define VA_IGNORE_RDEV ((cdev_t)-1)
#define VA_IGNORE_STORAGE ((uint64_t)-1)
#define VA_IGNORE_MODE ((u_short)-1)
#define VA_IGNORE_UID ((uid_t)-1)
#define VA_IGNORE_GID ((gid_t)-1)
#define VA_IGNORE_SIZE ((uint64_t)-1)
#define VA_IGNORE_TIME1 ((time_t)-1)
#define VA_IGNORE_FLAGS ((u_long)-1)

#endif /* _VENUS_PROC_H_ */
