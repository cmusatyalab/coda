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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/ss/coda-src/venus/RCS/worker.h,v 4.1 1997/01/08 21:51:53 rvb Exp braam $";
#endif /*_BLURB_*/







/*
 *
 * Specification of the Venus Worker subsystem.
 *
 */


#ifndef _VENUS_WORKER_H_
#define _VENUS_WORKER_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>

#ifdef __cplusplus
}
#endif __cplusplus

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


const int DFLT_MAXWORKERS = 5;
const int UNSET_MAXWORKERS = -1;
const int DFLT_MAXPREFETCHERS = 1;

class msgent : public olink {
  friend msgent *FindMsg(olist&, u_long);
  friend int MsgRead(msgent *);
  friend int MsgWrite(char *, int);
  friend worker *FindWorker(u_long);
  friend void DispatchWorker(msgent *);
  friend int IsAPrefetch(msgent *);
  friend class worker;
  friend class vproc;
  friend int k_Purge();
  friend int k_Purge(ViceFid *, int);
  friend int k_Purge(vuid_t);
  friend int k_Replace(ViceFid *, ViceFid *);
  friend class fsobj;
  friend void WorkerMux(int);

    char msg_buf[VC_MAXMSGSIZE];
    msgent();
    virtual ~msgent();

  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG
};

class msg_iterator : public olist_iterator {

  public:
    msg_iterator(olist&);
    msgent *operator()();
};

class worker : public vproc {
  friend void WorkerInit();
  friend worker *FindWorker(u_long);
  friend worker *GetIdleWorker();
  friend void DispatchWorker(msgent *);
  friend void WorkerMux(int);
  friend void WorkerReturnEarly(ViceFid *);
  friend int GetWorkerIdleTime();
  friend void PrintWorkers(int);
  friend int MsgRead(msgent *);
  friend int MsgWrite(char *, int);
  friend int WorkerCloseMuxfd();
  friend class vproc;
  friend class fsobj;

    static int muxfd;
    static int nworkers;
    static int nprefetchers;
    static time_t lastresign;
    static olist FreeMsgs;
    static olist QueuedMsgs;
    static olist ActiveMsgs;

    unsigned returned : 1;
    msgent *msg;			/* For communication with the kernel */
    int opcode;
    ViceFid StoreFid;

  public:
    worker();
    worker(worker&);	    /* not supported! */
    operator=(worker&);	    /* not supported! */
    virtual ~worker();

    void AwaitRequest();
    void Resign(msgent *, int);
    void Return(msgent *, int);
    void Return(int);

    void main(void *);
};

class worker_iterator : public vproc_iterator {

  public:
    worker_iterator();
    worker *operator()();
};


extern int MaxWorkers;
extern int MaxPrefetchers;
extern int KernelMask;


extern msgent *FindMsg(olist&, u_long);
extern int MsgRead(msgent *);
extern int MsgWrite(char *, int);
extern int k_Purge();
extern int k_Purge(ViceFid *, int =0);
extern int k_Purge(vuid_t);
extern int k_Replace(ViceFid *, ViceFid *);
extern void VFSMount();
extern void VFSUnmount();
extern void WorkerInit();
extern worker *FindWorker(u_long);
extern worker *GetIdleWorker();
extern void DispatchWorker(msgent *);
extern void WorkerMux(int);
extern void WorkerReturnEarly(ViceFid *);
extern int GetWorkerIdleTime();
extern void PrintWorkers();
extern void PrintWorkers(FILE *);
extern void PrintWorkers(int);


#endif	not _VENUS_WORKER_H_
