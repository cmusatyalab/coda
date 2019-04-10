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
 * Specification of the Venus Worker subsystem.
 *
 */

#ifndef _VENUS_WORKER_H_
#define _VENUS_WORKER_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from util */
#include <olist.h>

/* from venus */
#include "fso.h"
#include "vproc.h"

class msgent;
class msg_iterator;
class worker;
class worker_iterator;

int WorkerCloseMuxfd(void);
void ReadUpcallMsg(int fd, size_t size);

class msgent : public olink {
    friend msgent *FindMsg(olist &, u_long);
    friend worker *FindWorker(u_long);
    friend msgent *AllocMsgent(void);
    friend void ReadUpcallMsg(int fd, size_t size);
    friend void DispatchWorker(msgent *);
    friend int IsAPrefetch(msgent *);
    friend class worker;
    friend class vproc;
    friend int k_Purge();
    friend int k_Purge(VenusFid *, int);
    friend int k_Purge(uid_t);
    friend int k_Replace(VenusFid *, VenusFid *);
    friend class fsobj;

    char msg_buf[VC_MAXMSGSIZE];
    int return_fd;

public:
    msgent();
    ~msgent();

    static int allocs;
    static int deallocs;
};

class msg_iterator : public olist_iterator {
public:
    msg_iterator(olist &);
    msgent *operator()();
};

class worker : public vproc {
    friend void WorkerInit();
    friend worker *FindWorker(u_long);
    friend worker *GetIdleWorker();
    friend void DispatchWorker(msgent *);
    friend msgent *AllocMsgent(void);
    friend void ReadUpcallMsg(int fd, size_t size);
    friend ssize_t WriteDowncallMsg(int fd, const char *buf, size_t size);
    friend ssize_t MsgWrite(const char *msg, size_t size);
    friend void WorkerMux(int fd, void *udata);
    friend time_t GetWorkerIdleTime();
    friend void PrintWorkers(int);
    friend int WorkerCloseMuxfd(void);
    friend void VFSMount();
    friend class vproc;
    friend class fsobj;
    friend int GetKernelModuleVersion();

    static int muxfd;
    static int nworkers;
    static int nprefetchers;
    static time_t lastresign;
    static olist FreeMsgs;
    static olist QueuedMsgs;
    static olist ActiveMsgs;
    static int kernel_version;
    static const char *CacheDir;
    static const char *CachePrefix;
    static const char *kernDevice;

    unsigned returned : 1;
    msgent *msg; /* For communication with the kernel */
    int opcode;
    VenusFid StoreFid;

    inline void op_coda_access(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_close(union inputArgs *in, union outputArgs *out,
                              int *msg_size);
    inline void op_coda_create(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_fsync(union inputArgs *in, union outputArgs *out,
                              int *msg_size);
    inline void op_coda_getattr(union inputArgs *in, union outputArgs *out,
                                int *msg_size);
    inline void op_coda_ioctl(union inputArgs *in, union outputArgs *out,
                              int *msg_size);
    inline void op_coda_link(union inputArgs *in, union outputArgs *out,
                             int *msg_size);
    inline void op_coda_lookup(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_mkdir(union inputArgs *in, union outputArgs *out,
                              int *msg_size);
    inline void op_coda_open(union inputArgs *in, union outputArgs *out,
                             int *msg_size, CodaFid *saveFid, int *saveFlags);
    inline void op_coda_open_by_fd(union inputArgs *in, union outputArgs *out,
                                   int *msg_size, int *openfd, CodaFid *saveFid,
                                   int *saveFlags, struct venus_cnode *vtarget);
    inline void op_coda_open_by_path(union inputArgs *in, union outputArgs *out,
                                     int *msg_size, CodaFid *saveFid,
                                     int *saveFlags);
    inline void op_coda_readlink(union inputArgs *in, union outputArgs *out,
                                 int *msg_size);
    inline void op_coda_remove(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_rename(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_rmdir(union inputArgs *in, union outputArgs *out,
                              int *msg_size);
    inline void op_coda_root(union inputArgs *in, union outputArgs *out,
                             int *msg_size);
    inline void op_coda_setattr(union inputArgs *in, union outputArgs *out,
                                int *msg_size);
    inline void op_coda_symlink(union inputArgs *in, union outputArgs *out,
                                int *msg_size);
    inline void op_coda_vget(union inputArgs *in, union outputArgs *out,
                             int *msg_size);
    inline void op_coda_statfs(union inputArgs *in, union outputArgs *out,
                               int *msg_size);
    inline void op_coda_access_intent(union inputArgs *in,
                                      union outputArgs *out, int *msg_size);

public:
    worker();
    worker(worker &); /* not supported! */
    int operator=(worker &); /* not supported! */
    virtual ~worker();

    void AwaitRequest();
    void Resign(msgent *, int);
    void Return(msgent *, size_t);
    void Return(int);

    static int isReady() { return (muxfd != -1); }

protected:
    virtual void main(void);
};

class worker_iterator : public vproc_iterator {
public:
    worker_iterator();
    worker *operator()();
};

extern msgent *FindMsg(olist &, u_long);
extern int k_Purge();
extern int k_Purge(VenusFid *, int = 0);
extern int k_Purge(uid_t);
extern int k_Replace(VenusFid *, VenusFid *);
extern void VFSMount();
extern void VFSUnmount();
extern void WorkerInit();
extern worker *FindWorker(u_long);
extern worker *GetIdleWorker();
extern void DispatchWorker(msgent *);
extern void WorkerMux(int fd, void *udata);
extern time_t GetWorkerIdleTime(void);
extern void PrintWorkers();
extern void PrintWorkers(FILE *);
extern void PrintWorkers(int);
extern int GetKernelModuleVersion();

#endif /* _VENUS_WORKER_H_ */
