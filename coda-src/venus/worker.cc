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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/worker.cc,v 4.8 1997/12/20 23:35:14 braam Exp $";
#endif /*_BLURB_*/




/*
 *
 * Implementation of the Venus Worker subsystem.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#ifndef __FreeBSD__
// Since vproc.h knows struct uio.
#include <sys/uio.h>
#endif
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#ifdef  __FreeBSD__
#include <sys/param.h>
#endif
#ifdef __linux__
#if !defined(__GLIBC__) || __GLIBC__ < 2
#include <linux/fs.h>
#endif
#include <mntent.h>
#endif


#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>
#include <vproc.h>
#include <venus_vnode.h>
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <cfs/coda.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "simulate.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"


/* Temporary!  Move to cnode.h. -JJK */
#define	C_INCON	0x2

/* static class members */
int worker::muxfd;
int worker::nworkers;
int worker::nprefetchers;
time_t worker::lastresign;
olist worker::FreeMsgs;
olist worker::QueuedMsgs;
olist worker::ActiveMsgs;

#ifdef VENUSDEBUG
int msgent::allocs = 0;
int msgent::deallocs = 0;
#endif VENUSDEBUG

const int WorkerStackSize = 131072;

int MaxWorkers = UNSET_MAXWORKERS;
int MaxPrefetchers = UNSET_MAXWORKERS;
int KernelMask = 0;				/* subsystem is uninitialized until mask is non-zero */
PRIVATE int Mounted = 0;


/* -------------------------------------------------- */

/* The rationale for having a pool of messages which are dynamically
assigned to workers rather than giving each worker its own private
message is to allow the mux to process signal messages (i.e.,
interrupts).  In order to allow that with the static scheme, the mux
would have to copy the message into a worker's buffer after
determining that it is not a signal.  That option was deemed to be
less efficient that the message-pool scheme.  Also, it would require
that either we allow an infinite number of worker's to be created, or
force the mux to either block or refuse new requests when MaxWorkers
was reached. */


/* Should not be called on the free list! */
msgent *FindMsg(olist& ol, u_long seq) {
    msg_iterator next(ol);
    msgent *m;
    while (m = next())
	if (((union inputArgs *)m->msg_buf)->ih.unique == seq) return(m);

    return(0);
}


int MsgRead(msgent *m) {
    int cc = read(worker::muxfd, m->msg_buf, (int) (VC_MAXMSGSIZE));
    if (cc < sizeof(struct cfs_in_hdr)) return(-1);

    return(0);
}


int MsgWrite(char *buf, int size) {
    return write(worker::muxfd, buf, size);
}


msgent::msgent() {
#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


msgent::~msgent() {
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
}


msg_iterator::msg_iterator(olist& ol) : olist_iterator(ol) {
}


msgent *msg_iterator::operator()() {
    return((msgent *)olist_iterator::operator()());
}

void testKernDevice() {
    if (Simulating) return;

    /* First try to purge the kernel cache. */
    /* If the open of the kernel device succeeds we know that there is no other living venus. */
	/* Open the kernel device. */
	int fd = ::open(kernDevice, O_RDWR, 0);
	if (fd < 0) {
	    eprint("open(%s) failed (%d), exiting",
		   kernDevice, errno);
	    exit(-1);
	}

	/* Construct a purge message */
	union outputArgs msg;

	msg.oh.opcode = CFS_FLUSH;
	msg.oh.unique = 0;

	/* Send the message. */
	if (write(fd, (char *)&msg, (int)sizeof(u_long) * 2) != sizeof(u_long) * 2) {
	    eprint("write for flush failed (%d), exiting", errno);
	    exit(-1);
	}

	/* Close the kernel device. */
	if (close(fd) < 0) {
	    eprint("close(%s) failed (%d), exiting",
		   kernDevice, errno);
	    exit(-1);
    }
}

void VFSMount() {
    if (Simulating) return;

    /* Linux Coda filesystems are mounted by hand through forking since they need venus. XXX eliminate zombie */ 
#ifndef __linux__
    /* Silently unmount the root node in case an earlier venus exited without successfully unmounting. */
    syscall(SYS_unmount, venusRoot);
    switch(errno) {
	case 0:
	    eprint("unmount(%s) succeeded, continuing", venusRoot);
	    break;

	case EINVAL:
	    /* not mounted */
	    break;

	case EBUSY:
	default:
	    eprint("unmount(%s) failed (%d), exiting", venusRoot, errno);
	    exit(-1);
    }

    /* Deduce rootnodeid. */
    struct stat tstat;
    if (::stat(venusRoot, &tstat) < 0) {
	eprint("stat(%s) failed (%d), exiting", venusRoot, errno);
	exit(-1);
    }
    rootnodeid = tstat.st_ino;
#endif
    /* Issue the VFS mount request. */
#ifdef __MACH__
    if (syscall(SYS_vfsmount, MOUNT_CFS, venusRoot,
		 (caddr_t)0, kernDevice) < 0) {
	eprint("vfsmount(%s, %s) failed (%d), exiting",
	       kernDevice, venusRoot, errno);
	exit(-1);
    }
#endif /* __MACH__ */
#ifdef __BSD44__
#ifndef	MOUNT_CFS
    /* for FreeBSD
 - Add line below into /usr/include/sys/mount.h. Don't forget to adjust
		MOUNT_MAXTYPE.
	# define MOUNT_CFS	old MOUNT_MAXTYPE +1
	In #define INITMOUNTNAMES,
	        "cfs",          /* 19 or so MOUNT_CFS */
#endif
    if (mount(MOUNT_CFS, venusRoot, 0, kernDevice) < 0) {
	eprint("mount(%s, %s) failed (%d), exiting",
	       kernDevice, venusRoot, errno);
	exit(-1);
    }
#endif /* __BSD44__ */
#ifdef __linux__
    {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	if ( sigaction(SIGCHLD, &sa, NULL) ) {
	    eprint("Cannot set signal handler for SIGCHLD");
	    assert(0);
	}
    }
    if ( fork() == 0 ) {
      int error;
      /* child only makes a system call and should not hang on to 
	 a /dev/cfs0 file descriptor */
      error = WorkerCloseMuxfd();
      if ( error ) {
	pid_t parent;
	LOG(1, ("CHILD: cannot close worker::muxfd. Killing parent.\n"));
	parent = getppid();
	kill(parent, SIGKILL);
	exit(1);
      }
      error = mount("coda", venusRoot, "coda",  MS_MGC_VAL , &kernDevice);
      if ( error ) {
	pid_t parent;
	LOG(1, ("CHILD: mount system call failed. Killing parent.\n"));
	parent = getppid();
	kill(parent, SIGKILL);
	exit(1);
      } else {
	FILE *fd;
	struct mntent ent;
	fd = setmntent("/etc/mtab", "a");
	if ( fd > 0 ) { 
	  ent.mnt_fsname="Coda";
	  ent.mnt_dir=venusRoot;
	  ent.mnt_type= "coda";
	  ent.mnt_opts = "rw";
	  ent.mnt_freq = 0;
	  ent.mnt_passno = 0;
	  error = addmntent(fd, & ent);
	  error = endmntent(fd);
	  exit(0);
	}
      }
      exit(1);
    }

#endif
    Mounted = 1;
}


void VFSUnmount() {
    if (Simulating || !Mounted) return;

    /* Purge the kernel cache so that all cnodes are (hopefully) released. */
    k_Purge();
#ifndef	__linux__
    /* Issue the VFS unmount request. */
    if(syscall(SYS_unmount, venusRoot) < 0) {
	eprint("vfsunmount(%s) failed (%d)", venusRoot, errno);
	return;
    }

    /* Sync the cache. */
    /* N.B.  Deadlock will result if the kernel still thinks we're mounted (i.e., if the preceding unmount failed)! */
    sync();
#endif
}


int k_Purge() {
    if (Simulating)
	return(1);
    if (KernelMask == 0) return(1);

    LOG(1, ("k_Purge: Flush\n"));

    /* Construct a purge message. */
    union outputArgs msg;
    
    msg.oh.opcode = CFS_FLUSH;
    msg.oh.unique = 0;
    
    /* Send the message. */
    if (MsgWrite((char *)&msg, (int)sizeof(u_long) * 2) != (int)sizeof(u_long)*2)
	Choke("k_Purge: Flush, message write returns %d", errno);

    LOG(1, ("k_Purge: Flush, returns 0\n"));
    VFSStats.VFSOps[CFS_FLUSH].success++;

    return(1);
}


int k_Purge(ViceFid *fid, int severely) {
    if (Simulating)
	return(1);
    if (KernelMask == 0) return(1);

    LOG(100, ("k_Purge: fid = (%x.%x.%x), severely = %d\n",
	       fid->Volume, fid->Vnode, fid->Unique, severely));

    int retcode = 0;

    /* Setup message. */
    union outputArgs msg;

    if (severely) {
	msg.cfs_purgefid.oh.opcode = CFS_PURGEFID;
	msg.cfs_purgefid.oh.unique = 0;
	msg.cfs_purgefid.CodaFid = *fid;
    } else if (ISDIR(*fid)) {
	msg.cfs_zapdir.oh.opcode = CFS_ZAPDIR;
	msg.cfs_zapdir.oh.unique = 0;
	msg.cfs_zapdir.CodaFid = *fid;
    } else {
	msg.cfs_zapfile.oh.opcode = CFS_ZAPFILE;
	msg.cfs_zapfile.oh.unique = 0;
	msg.cfs_zapfile.CodaFid = *fid;
    }	

    /* Send the message. */
    if (MsgWrite((char *)&msg, (int) sizeof(union outputArgs)) != (int) sizeof(union outputArgs)) {
	retcode = errno;
	if (retcode != ETXTBSY)
	    Choke("k_Purge: %s, message write returns %d", 
	      msg.oh.opcode == CFS_PURGEFID ? "CFS_PURGEFID" :
	      msg.oh.opcode == CFS_ZAPFILE ? "CFS_ZAPFILE" : "CFS_ZAPDIR", retcode);
    }

    LOG(100, ("k_Purge: %s, returns %d\n", 
	      msg.oh.opcode == CFS_PURGEFID ? "CFS_PURGEFID" :
	      msg.oh.opcode == CFS_ZAPFILE ? "CFS_ZAPFILE" : "CFS_ZAPDIR", retcode));
    if (retcode == 0) {
	VFSStats.VFSOps[msg.oh.opcode].success++;
    }
    else {
	VFSStats.VFSOps[msg.oh.opcode].failure++;
    }

    return(retcode == 0);
}


int k_Purge(vuid_t vuid) {
    if (Simulating)
	return(1);
    if (KernelMask == 0) return(1);

    LOG(1, ("k_Purge: vuid = %d\n", vuid));

    /* Message prefix. */
    union outputArgs msg;
    msg.cfs_purgeuser.oh.unique = 0;
    msg.cfs_purgeuser.oh.opcode = CFS_PURGEUSER;

    /* Message data. */
    bzero(&msg.cfs_purgeuser.cred, (int) sizeof(struct coda_cred));
    msg.cfs_purgeuser.cred.cr_uid = vuid;

    /* Send the message. */
    if (MsgWrite((char *)&msg, (int) sizeof(union outputArgs)) != (int) sizeof(union outputArgs))
	Choke("k_Purge: PurgeUser, message write");

    LOG(1, ("k_Purge: PurgeUser, returns 0\n"));
    VFSStats.VFSOps[CFS_PURGEUSER].success++;

    return(1);
}

int k_Replace(ViceFid *fid_1, ViceFid *fid_2) {
    if (Simulating)
	return(1);
    if (KernelMask == 0) return(1);

    if (!fid_1 || !fid_2)
	Choke("k_Replace: nil fids");

    LOG(0, ("k_Replace: ViceFid (%x.%x.%x) with ViceFid (%x.%x.%x) in mini-cache\n", 
	    fid_1->Volume, fid_1->Vnode, fid_1->Unique, fid_2->Volume, 
	    fid_2->Vnode, fid_2->Unique));

    /* Message prefix. */
    struct cfs_replace_out msg;
    msg.oh.unique = 0;
    msg.oh.opcode = CFS_REPLACE;

    msg.OldFid = *fid_1;
    msg.NewFid = *fid_2;
	
    /* Send the message. */
    if (MsgWrite((char *)&msg, sizeof (struct cfs_replace_out)) != sizeof (struct cfs_replace_out))
	Choke("k_Replace: message write");

    LOG(0, ("k_Replace: returns 0\n"));
    VFSStats.VFSOps[CFS_REPLACE].success++;

    return(1);
}

/* -------------------------------------------------- */

void WorkerInit() {
    if (Simulating) return;

    if (MaxWorkers == UNSET_MAXWORKERS)
	MaxWorkers = DFLT_MAXWORKERS;

    if (MaxPrefetchers == UNSET_MAXWORKERS)
	MaxPrefetchers = DFLT_MAXPREFETCHERS;
    else
	if (MaxPrefetchers > MaxWorkers) /* whoa */
	    Choke("WorkerInit: MaxPrefetchers %d, MaxWorkers only %d!",
		  MaxPrefetchers, MaxWorkers);

    /* Open the communications channel. */
    worker::muxfd = ::open(kernDevice, O_RDWR, 0);
    if (worker::muxfd < 0)
	Choke("WorkerInit: open(%s, O_RDWR, 0) returns %d", kernDevice, errno);
    if (worker::muxfd >= NFDS)
	Choke("WorkerInit: worker::muxfd >= %d!", NFDS);

    /* Why do we stat the device? -JJK */
    struct stat tstat;
    fstat(worker::muxfd, &tstat);

    /* Flush kernel cache(s). */
    k_Purge();

    /* Allows the MessageMux to distribute incoming messages to us. */
    KernelMask |= (1 << worker::muxfd);

    worker::nworkers = 0;
    worker::nprefetchers = 0;
    worker::lastresign = Vtime();
}

int WorkerCloseMuxfd() {
  int error;
  return close(worker::muxfd);
}

worker *FindWorker(u_long seq) {
    worker_iterator next;
    worker *w;
    while (w = next())
	if (w->msg && ((union inputArgs *)w->msg)->ih.unique == seq) return(w);

    return(0);
}


worker *GetIdleWorker() {
    worker_iterator next;
    worker *w;
    while (w = next())
	if (w->idle) return(w);

    /* No idle workers; can we create a new one? */
    if (worker::nworkers < MaxWorkers) {
	return(new worker);
    }

    return(0);
}

int IsAPrefetch(msgent *m) {
    /* determines if a message is a prefetch request */
    union inputArgs *in = (union inputArgs *)m->msg_buf;
    
    if (in->ih.opcode != CFS_IOCTL)
	return(0);

    return (in->cfs_ioctl.cmd == VIOCPREFETCH);
}

void DispatchWorker(msgent *m) {
    /* We filter out signals (i.e., interrupts) before passing messages on to workers. */
    union inputArgs *in = (union inputArgs *)m->msg_buf;
    
    if (in->ih.opcode == CFS_SIGNAL) {
	eprint("DispatchWorker: signal received (seq = %d)", in->ih.unique);

	worker *signallee = FindWorker(in->ih.unique);
	if (signallee) {
	    if (!signallee->returned) {
		LOG(1, ("DispatchWorker: signalled worker %x\n", signallee));
		signallee->interrupted = 1;

		/* Poke the vproc in case it is waiting on some event. */
		Rtry_Signal();
		Conn_Signal();
		Srvr_Signal();
		Mgrp_Signal();
	    }
	}
	else {
	    msgent *qm = FindMsg(worker::QueuedMsgs, in->ih.unique);
	    if (qm) {
		LOG(1, ("DispatchWorker: signalled queued msg\n"));
		worker::QueuedMsgs.remove(qm);
		worker::FreeMsgs.append(qm);
	    }
	}

	worker::FreeMsgs.append(m);
	return;
    }

    /* 
     * Limit the number of workers handling prefetches. There should be
     * a separate (lower priority) queue for these requests. -- lily
     */
    if (IsAPrefetch(m)) {
	if (worker::nprefetchers >= MaxPrefetchers) {
	    LOG(1, ("DispatchWorker: queuing prefetch (%d workers, %d prefetching)\n", 
	        worker::nworkers, worker::nprefetchers));
            worker::QueuedMsgs.append(m);
	    return;    
	}
    }

    /* Try to find an idle worker to handle this message. */
    worker *w = GetIdleWorker();
    if (w) {
	worker::ActiveMsgs.append(m);
	w->msg = m;
	w->opcode = (int) in->ih.opcode;
	w->idle = 0;
	VprocSignal((char *)w);
	return;
    }

    /* No one is able to handle this message now; queue it up for the next free worker. */
    LOG(0, ("DispatchWorker: out of workers (max %d), queueing message\n", MaxWorkers));
    worker::QueuedMsgs.append(m);
}


void WorkerMux(int mask) {
    if (!(mask & KernelMask)) return;

    /* Get a free buffer and read a message from the kernel into it. */
    msgent *fm = (msgent *)worker::FreeMsgs.get();
    if (!fm) fm = new msgent;
    if (MsgRead(fm) < 0) {
	eprint("WorkerMux: worker read error");
	worker::FreeMsgs.append(fm);
	return;
    }

    DispatchWorker(fm);
}


void WorkerReturnEarly(ViceFid *fid) {
    worker_iterator next;
    worker *w;
    while (w = next())
	if (FID_EQ(w->StoreFid, *fid)) {
	    w->StoreFid = NullFid;
	    w->Return(0);
	}
}


int GetWorkerIdleTime() {
    /* Return 0 if any call is in progress. */
    worker_iterator next;
    worker *w;
    while (w = next())
	if (!w->idle) return(0);

    return(Vtime() - worker::lastresign);
}


void PrintWorkers() {
    PrintWorkers(stdout);
}


void PrintWorkers(FILE *fp) {
    fflush(fp);
    PrintWorkers(fileno(fp));
}


void PrintWorkers(int fd) {
    fdprint(fd, "%#08x : %-16s : muxfd = %d, nworkers = %d\n",
	     &worker::tbl, "Workers", worker::muxfd, worker::nworkers);

    worker_iterator next;
    worker *w;
    while (w = next()) w->print(fd);
}


/* -------------------------------------------------- */

worker::worker() : vproc("Worker", (PROCBODY) &worker::main, VPT_Worker, WorkerStackSize) {
    LOG(100, ("worker::worker(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    nworkers++;	    /* Ought to be a lock protecting this! -JJK */

    returned = 0;
    StoreFid = NullFid;
    msg = 0;
    opcode = 0;
    
    /* Poke main procedure. */
    VprocSignal((char *)this, 1);
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
worker::worker(worker& w) : vproc(*((vproc *)&w)) {
    abort();
}


worker::operator=(worker& w) {
    abort();
    return(0);
}


worker::~worker() {
    LOG(100, ("worker::~worker: %-16s : lwpid = %d\n", name, lwpid));

    nworkers--;	    /* Ought to be a lock protecting this! -JJK */
}


/* Called by workers to get next service request. */
void worker::AwaitRequest() {
    idle = 1;

    msgent *m = (msgent *)QueuedMsgs.get();

    /* limit the number of workers handling prefetches. see DispatchWorker. */
    if (m && IsAPrefetch(m) && worker::nprefetchers >= MaxPrefetchers) {
	/* re-queue and look for a non-prefetch message */
	LOG(1, ("worker::AwaitRequest: requeueing prefetch (%d workers, %d prefetching)\n", 
	    worker::nworkers, worker::nprefetchers));
	QueuedMsgs.append(m);
	for (int i = 0; i < QueuedMsgs.count(); i++) {
	    m = (msgent *)QueuedMsgs.get();
	    if (m && IsAPrefetch(m)) {
		QueuedMsgs.append(m);
		m = NULL;
	    } else
		break;
        }
    }

    if (m) {
	LOG(1000, ("worker::AwaitRequest: dequeuing message\n"));
	ActiveMsgs.append(m);
	msg = m;
	opcode = (int) ((union inputArgs *)m->msg_buf)->ih.opcode;
	idle = 0;
	return;
    }

    VprocWait((char *)this);
}


/* Called by workers after completing a service request. */
void worker::Resign(msgent *msg, int size) {
    if (returned) {
	char *opstr = VenusOpStr((int) ((union outputArgs*)msg->msg_buf)->oh.opcode);
	char *retstr = VenusRetStr((int) ((union outputArgs *)msg->msg_buf)->oh.result);
	
#ifdef	TIMING
	float elapsed = SubTimes(u.u_tv2, u.u_tv1);
	LOG(1, ("[Return Done] %s : returns %s, elapsed = %3.1f\n",
		opstr, retstr, elapsed));
#else	TIMING
	LOG(1, ("[Return Done] %s : returns %s\n", opstr, retstr))
#endif	TIMING
    }
    else {
	if (((union outputArgs *)msg->msg_buf)->oh.result == EINCONS) {
/*	    ((union outputArgs *)msg->msg_buf)->oh.result = ENOENT;*/
	    Choke("worker::Resign: result == EINCONS");
	}

	Return(msg, size);
    }

    ActiveMsgs.remove(msg);
    FreeMsgs.append(msg);
    msg = 0;
    opcode = 0;
    
    lastresign = Vtime();
}


void worker::Return(msgent *msg, int size) {
    if (returned)
	Choke("worker::Return: already returned!");

    char *opstr = VenusOpStr((int) ((union outputArgs*)msg->msg_buf)->oh.opcode);
    char *retstr = VenusRetStr((int) ((union outputArgs*)msg->msg_buf)->oh.result);

#ifdef	TIMING
    float elapsed = SubTimes(u.u_tv2, u.u_tv1);
    LOG(1, ("%s : returns %s, elapsed = %3.1f msec\n", opstr, retstr, elapsed));
#else	TIMING
    LOG(1, ("%s : returns %s\n", opstr, retstr));
#endif	TIMING

    /* There is no reply to an interrupted operation. */
    if (!interrupted) {
	int cc = MsgWrite(msg->msg_buf, size);
	int errn = errno;
	if (cc != size) {
	    eprint("worker::Return: message write error %d (op = %d, seq = %d), wrote %d of %d bytes\n",
		   errno, ((union outputArgs*)msg->msg_buf)->oh.opcode,
		   ((union outputArgs*)msg->msg_buf)->oh.unique, cc, size);  

	    /* Guard against a race in which the kernel is signalling us, but we entered this */
	    /* block before the signal reached us.  In this case the error code from the MsgWrite */
	    /* will be ESRCH.  No other error code is legitimate. */
	    if (errn != ESRCH) Choke("worker::Return: errno (%d) from MsgWrite", errno);
	    interrupted = 1;
	}
    }

    returned = 1;
}


void worker::Return(int code) {
    ((union outputArgs*)msg->msg_buf)->oh.result = code; 
    Return(msg, (int)sizeof (struct cfs_out_hdr));
}


void worker::main(void *parm) {
    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

    for (;;) {
	/* Wait for new request. */
	AwaitRequest();

	/* Sanity check new request. */
	if (idle) Choke("Worker: signalled but not dispatched!");
	if (!msg) Choke("Worker: no message!");

	union inputArgs *in = (union inputArgs *)msg->msg_buf;
	union outputArgs *out = (union outputArgs *)msg->msg_buf;
	
	interrupted = 0;
	returned = 0;
	StoreFid = NullFid;

	/* Fill in the user-specific context. */
	u.Init();
	u.u_priority = FSDB->StdPri();
	u.u_flags = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);

	/* This switch corresponds to the kernel trap handler. */
	switch (in->ih.opcode) {
	    int size;
#define GOTTA_BE_ME(in) \
	    u.u_cred = (in)->ih.cred; u.u_pid = (in)->ih.pid; u.u_pgid = (in)->ih.pgid;
	    
	    case CFS_ACCESS:
		{
		GOTTA_BE_ME(in);
		LOG(100, ("CFS_ACCESS: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		/* Original code used to have a local declaration of a struct venus_cnode here.
		   This avoided heap allocation and freeing, but only worked in Mach (because
		   the cnode contains the vnode rather than the vnode being separately allocated).
		   No choice in BSD44 but to use {MAKE,DISCARD}_VNODE.
		   This hurts peformance because it adds 2 new's and 2 delete's for even
		   the most trivial Venus request.   But it has the benefit of avoiding
		   the use of CN_INIT() altogether, making the code easier to understand.
		   This comment also applies to all the other arms of this switch.  
 		   (Satya, 8/16/96)
		*/
		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_access.VFid, 0);
		access(vtarget, in->cfs_access.flags);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_CLOSE:
		{
		GOTTA_BE_ME(in);
		LOG(100, ("CFS_CLOSE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_close.VFid, 0);
		close(vtarget, in->cfs_close.flags);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	  case CFS_CREATE:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_CREATE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid,u.u_pgid));

		struct venus_vnode *vparent;
		MAKE_VNODE(vparent, in->cfs_create.VFid, 0);
		struct venus_vnode *target = 0;
		create(vparent, (char *)in + (int)in->cfs_create.name,
		       &in->cfs_create.attr, in->cfs_create.excl,
		       in->cfs_create.mode, &target);
		DISCARD_VNODE(vparent);

		if (u.u_error == 0) {
		    out->cfs_create.VFid = VTOC(target)->c_fid;
		    out->cfs_create.attr = in->cfs_create.attr;
		    DISCARD_VNODE(target);
		    target = 0;
		    size = (int)sizeof (struct cfs_create_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CFS_FSYNC:	
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_FSYNC: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_fsync.VFid, 0);
		fsync(vtarget);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_GETATTR:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_GETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_getattr.VFid, 0);
		va_init(&out->cfs_getattr.attr);
		getattr(vtarget, &out->cfs_getattr.attr);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_getattr_out));
		break;
		}

	    case CFS_INACTIVE: 
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_INACTIVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_inactive.VFid, 0);
		inactive(vtarget);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_IOCTL:
		{
		char outbuf[VC_MAXDATASIZE];
		struct ViceIoctl data;
		data.in = (char *)in + (int)in->cfs_ioctl.data;
		data.in_size = 0;
		data.out = outbuf;	/* Can't risk overcopying. Sigh. -dcs */
		data.out_size =	0;

		GOTTA_BE_ME(in);

		LOG(100, ("CFS_IOCTL: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		if (in->cfs_ioctl.cmd == VIOCPREFETCH)
		    worker::nprefetchers++;

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_ioctl.VFid, 0);
		data.in_size = in->cfs_ioctl.len;
		ioctl(vtarget, in->cfs_ioctl.cmd, &data, in->cfs_ioctl.rwflag);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		out->cfs_ioctl.len = data.out_size;
		out->cfs_ioctl.data = (char *)(sizeof (struct cfs_ioctl_out));
		bcopy(data.out, (char *)out + (int)out->cfs_ioctl.data, data.out_size);
		Resign(msg, (int) sizeof (struct cfs_ioctl_out) + data.out_size);

		if (in->cfs_ioctl.cmd == VIOCPREFETCH)
		    worker::nprefetchers--;

		break;
		}

	    case CFS_LINK:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_LINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vsource;
		MAKE_VNODE(vsource, in->cfs_link.sourceFid, 0);
		struct venus_vnode *vp_target;
		MAKE_VNODE(vp_target, in->cfs_link.destFid, 0);
		link(vsource, vp_target, (char *)in + (int)in->cfs_link.tname);
		DISCARD_VNODE(vsource);
		DISCARD_VNODE(vp_target);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_LOOKUP:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_LOOKUP: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vparent;
		MAKE_VNODE(vparent, in->cfs_lookup.VFid, 0);
		struct venus_vnode *target = 0;
		lookup(vparent, (char *)in + (int)in->cfs_lookup.name, &target);
		DISCARD_VNODE(vparent);

		out->oh.result = u.u_error;
		if (u.u_error == 0) {
		    out->cfs_lookup.VFid = VTOC(target)->c_fid;
		    out->cfs_lookup.vtype = VN_TYPE(target);
		    if (out->cfs_lookup.vtype == C_VLNK &&
			VTOC(target)->c_flags & C_INCON)
			    out->cfs_lookup.vtype |= CFS_NOCACHE;
		    DISCARD_VNODE(target);
		    target = 0;
		    size = sizeof (struct cfs_lookup_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		
		Resign(msg,  size);

		break;
		}

	    case CFS_MKDIR:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_MKDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vparent;
		MAKE_VNODE(vparent, in->cfs_mkdir.VFid, 0);
		struct venus_vnode *target = 0;
		mkdir(vparent, (char *)in + (int)in->cfs_mkdir.name, &in->cfs_mkdir.attr, &target);
		DISCARD_VNODE(vparent);

		if (u.u_error == 0) {
		    out->cfs_mkdir.VFid = VTOC(target)->c_fid;
		    out->cfs_mkdir.attr = in->cfs_mkdir.attr;
		    DISCARD_VNODE(target);
		    target = 0;
		    size = sizeof (struct cfs_mkdir_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CFS_OPEN:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_OPEN: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		/* If an abort happens, we create a message and pass it off to
		 * another worker. Why not do the close ourselves? Oh well,
		 * I'll leave it this way... -- DCS
		 */
		ViceFid saveFid = in->cfs_open.VFid;
		int saveFlags = in->cfs_open.flags;
		
		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_open.VFid, 0);
		struct venus_cnode *cp = VTOC(vtarget);
		open(&vtarget, in->cfs_open.flags);
		
		if (u.u_error == 0) {
		    MarinerReport(&cp->c_fid, CRTORUID(u.u_cred));
		}

		if (u.u_error == 0) {
		    out->cfs_open.dev = cp->c_device;
		    out->cfs_open.inode = cp->c_inode;
		    size = sizeof (struct cfs_open_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		DISCARD_VNODE(vtarget);
		
		out->oh.result = u.u_error;
		Resign(msg, size);

		/* If open was aborted by user we must abort our OPEN too (if it was successful). */
		if (interrupted && out->oh.result == 0) {
		    eprint("worker::main: aborting open (%x.%x.%x)",
			  saveFid.Volume, saveFid.Vnode, saveFid.Unique);

		    /* NOTE: This may be bogus. It will definately cause a "message write
		     * error" since the uniquifier is bogus. No harm done, I guess.
		     * But why not just call close directly? -- DCS */
		    /* Fashion a CLOSE message. */
		    msgent *fm = (msgent *)worker::FreeMsgs.get();
		    if (!fm) fm = new msgent;
		    union inputArgs *dog = (union inputArgs *)fm->msg_buf;
		    
		    dog->cfs_close.ih.unique = (u_long)-1;
		    dog->cfs_close.ih.opcode = CFS_CLOSE;
		    dog->cfs_close.VFid = saveFid;
		    dog->cfs_close.flags = saveFlags;
		    
		    /* Dispatch it. */
		    DispatchWorker(fm);
		}

		break;
		}

	    case CFS_READDIR:	
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_READDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_readdir.VFid, 0);
		struct iovec aiov;
		aiov.iov_base = (char *)out + sizeof (struct cfs_readdir_out);
		aiov.iov_len = in->cfs_readdir.count;
		struct uio auio;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = in->cfs_readdir.offset;
		auio.uio_resid = in->cfs_readdir.count;
		readdir(vtarget, &auio);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		out->cfs_readdir.size = in->cfs_readdir.count - auio.uio_resid;
		out->cfs_readdir.data = (char*)(sizeof (struct cfs_readdir_out));
		Resign(msg, (int) sizeof (struct cfs_readdir_out) + out->cfs_readdir.size);
		break;
		}

	    case CFS_READLINK: 
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_READLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_readlink.VFid, 0);

		struct iovec aiov;
		aiov.iov_base = (char *)out + sizeof (struct cfs_readlink_out);
		aiov.iov_len = CFS_MAXPATHLEN;
		struct uio auio;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_resid = MAXPATHLEN;
		readlink(vtarget, &auio);

		if (u.u_error == 0) {
		    MarinerReport(&(VTOC(vtarget)->c_fid), CRTORUID(u.u_cred));
		}
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		out->cfs_readlink.count = MAXPATHLEN - auio.uio_resid;
		out->cfs_readlink.data = (char *)(sizeof (struct cfs_readlink_out));
		Resign(msg, (int) sizeof (struct cfs_readlink_out) + out->cfs_readlink.count);
		break;
		}

	    case CFS_REMOVE:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_REMOVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		struct venus_vnode *vparent;
		MAKE_VNODE(vparent, in->cfs_remove.VFid, 0);
		remove(vparent, (char *)in + (int)in->cfs_remove.name);
		DISCARD_VNODE(vparent);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_RENAME:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_RENAME: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vp_source;
		MAKE_VNODE(vp_source, in->cfs_rename.sourceFid, 0);
		struct venus_vnode *vp_target;
		MAKE_VNODE(vp_target, in->cfs_rename.destFid, 0);
		rename(vp_source, (char *)in + (int)in->cfs_rename.srcname,
		       vp_target, (char *)in + (int)in->cfs_rename.destname);
		DISCARD_VNODE(vp_source);
		DISCARD_VNODE(vp_target);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_RMDIR:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_RMDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vparent;
		MAKE_VNODE(vparent, in->cfs_rmdir.VFid, 0);
		rmdir(vparent, (char *)in + (int)in->cfs_rmdir.name);
		DISCARD_VNODE(vparent);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_ROOT:
		{
		GOTTA_BE_ME(in);

		struct venus_vnode *target = 0;
		root(&target);

		if (u.u_error == 0) {
		    out->cfs_root.VFid = VTOC(target)->c_fid;
		    DISCARD_VNODE(target);
		    target = 0;
		    size = sizeof (struct cfs_root_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		    
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CFS_SETATTR:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_SETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vtarget;
		MAKE_VNODE(vtarget, in->cfs_setattr.VFid, 0);
		setattr(vtarget, &in->cfs_setattr.attr);
		DISCARD_VNODE(vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_SYMLINK:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_SYMLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *vp_target;
		MAKE_VNODE(vp_target, in->cfs_symlink.VFid, 0);
		symlink(vp_target, (char *)in + (int)in->cfs_symlink.srcname, &in->cfs_symlink.attr, (char *)in + (int)in->cfs_symlink.tname);
		DISCARD_VNODE(vp_target);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_SYNC:
		{
		GOTTA_BE_ME(in);
		sync();

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		break;
		}

	    case CFS_VGET:
		{
		GOTTA_BE_ME(in);

		LOG(100, ("CFS_VGET: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_vnode *target = 0;
		struct cfid fid;
		fid.cfid_len = (unsigned short)sizeof(ViceFid);
		fid.cfid_fid = in->cfs_vget.VFid;
		vget(&target, &fid);

		out->oh.result = u.u_error;
		if (u.u_error == 0) {
		    out->cfs_vget.VFid = VTOC(target)->c_fid;
		    out->cfs_vget.vtype = VN_TYPE(target);
		    if (out->cfs_vget.vtype == C_VLNK && VTOC(target)->c_flags & C_INCON)
			out->cfs_vget.vtype |= CFS_NOCACHE;
		    DISCARD_VNODE(target);
		    target = 0;
		    size = sizeof (struct cfs_vget_out);
		} else
		    size = sizeof (struct cfs_out_hdr);
		
		Resign(msg, size);

		break;
		}

	    default:	 /* Toned this down a bit, used to be a choke -- DCS */
		{	/* But make sure someone sees it! */
		eprint("worker::main Got a bogus opcode %d", in->ih.opcode);
		dprint("worker::main Got a bogus opcode %d\n", in->ih.opcode);
		MarinerLog("worker::main Got a bogus opcode %d\n", in->ih.opcode);

		out->oh.result = EOPNOTSUPP;
		Resign(msg, (int) sizeof (struct cfs_out_hdr));
		}
	}
    }
}


worker_iterator::worker_iterator() : vproc_iterator(VPT_Worker) {
}


worker *worker_iterator::operator()() {
    return((worker *)vproc_iterator::operator()());
}

