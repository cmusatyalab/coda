/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#include <netinet/in.h>

#if !defined(__CYGWIN32__) && !defined(DJGPP)
#include <sys/syscall.h>
#include <sys/mount.h>
#endif

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef  __FreeBSD__
#include <sys/param.h>
#endif

#ifdef __linux__
#if !defined(__GLIBC__) || __GLIBC__ < 2
#include <linux/fs.h>
#endif
#include <mntent.h>
#endif

#include <cfs/coda.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vproc.h>

/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"

extern int venus_relay_addr;
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
int KernelMask = 0;	/* subsystem is uninitialized until mask is non-zero */
int kernel_version = 0;
static int Mounted = 0;


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


int MsgRead(msgent *m) 
{
#if defined(DJGPP) || defined(__CYGWIN32__)
        struct sockaddr_in addr;
	int len = sizeof(addr);
	int cc = ::recvfrom(worker::muxfd, m->msg_buf, (int) (VC_MAXMSGSIZE),
			    0, (struct sockaddr *) &addr, &len);
#else
	int cc = read(worker::muxfd, m->msg_buf, (int) (VC_MAXMSGSIZE));
#endif
	if (cc < sizeof(struct coda_in_hdr)) 
		return(-1);

	return(0);
}


int MsgWrite(char *buf, int size) 
{
#if defined(DJGPP) || defined(__CYGWIN32__)
         struct sockaddr_in addr;

         addr.sin_family = AF_INET;
         addr.sin_port = htons(8001);
         addr.sin_addr.s_addr = htonl(venus_relay_addr);
         return ::sendto(worker::muxfd, buf, size, 0, 
			 (struct sockaddr *) &addr, sizeof(addr));
#else 
	return write(worker::muxfd, buf, size);
#endif
}


msgent::msgent() 
{
#ifdef	VENUSDEBUG
    allocs++;
#endif	VENUSDEBUG
}


msgent::~msgent() 
{
#ifdef	VENUSDEBUG
    deallocs++;
#endif	VENUSDEBUG
}


msg_iterator::msg_iterator(olist& ol) : olist_iterator(ol) 
{
}


msgent *msg_iterator::operator()() 
{
    return((msgent *)olist_iterator::operator()());
}


/* test if we can open the kernel device and purge the cache,
   BSD systems like to purge that cache */
void testKernDevice() 
{
#if defined(DJGPP) || defined(__CYGWIN32__)
	return;
#endif
	/* If the open of the kernel device succeeds we know that there is
	   no other living venus. */
	int fd = ::open(kernDevice, O_RDWR, 0);
	if (fd < 0) {
		eprint("Probably another Venus is running! open(%s) failed (%d), exiting",
		       kernDevice, errno);
		exit(-1);
	}

	/* Construct a purge message */
	union outputArgs msg;

	msg.oh.opcode = CODA_FLUSH;
	msg.oh.unique = 0;

	/* Send the message. */
	if (write(fd, (const void *)&msg, sizeof(struct coda_out_hdr))
		  != sizeof(struct coda_out_hdr)) {
		eprint("Write for flush failed (%d), exiting", errno);
		exit(-1);
	}

	/* Close the kernel device. */
	if (close(fd) < 0) {
	    eprint("close(%s) of /dev/cfs0 failed (%d), exiting",
		   kernDevice, errno);
	    exit(-1);
	}
}

void VFSMount() {
    /* Linux Coda filesystems are mounted by hand through forking since they need venus. XXX eliminate zombie */ 
#ifdef __BSD44__
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

#ifdef __BSD44__
    if (mount("coda", venusRoot, 0, kernDevice) < 0) {
	if (mount("cfs", venusRoot, 0, kernDevice) < 0)
#if	defined(__FreeBSD__) && !defined(__FreeBSD_version)
#define MOUNT_CFS 19
	    if (mount(MOUNT_CFS, venusRoot, 0, kernDevice) < 0)
#endif
	{
	    eprint("mount(%s, %s) failed (%d), exiting",
		   kernDevice, venusRoot, errno);
	    exit(-1);
	}
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
	    CODA_ASSERT(0);
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


void VFSUnmount() 
{
	if (!Mounted) 
		return;

    /* Purge the kernel cache so that all cnodes are (hopefully)
       released. */
	k_Purge();
#ifdef	__BSD44__
    /* For now we can not unmount, because an coda_root() upcall could
       nail us. */
#ifndef	__BSD44__
    /* Issue the VFS unmount request. */
    if(syscall(SYS_unmount, venusRoot) < 0) {
	eprint("vfsunmount(%s) failed (%d)", venusRoot, errno);
	return;
    }
#else
    return;
#endif

    /* Sync the cache. */
    /* N.B.  Deadlock will result if the kernel still thinks we're mounted (i.e., if the preceding unmount failed)! */
    sync();
#endif
}


int k_Purge() {
    if (KernelMask == 0) return(1);

    LOG(1, ("k_Purge: Flush\n"));

    /* Construct a purge message. */
    union outputArgs msg;
    
    msg.oh.opcode = CODA_FLUSH;
    msg.oh.unique = 0;
    
    /* Send the message. */
    if (MsgWrite((char *)&msg, sizeof(msg)) != sizeof(msg))
	    CHOKE("k_Purge: Flush, message write returns %d", errno);

    LOG(1, ("k_Purge: Flush, returns 0\n"));
    VFSStats.VFSOps[CODA_FLUSH].success++;

    return(1);
}


int k_Purge(ViceFid *fid, int severely) {
    if (KernelMask == 0) return(1);

    LOG(100, ("k_Purge: fid = (%x.%x.%x), severely = %d\n",
	       fid->Volume, fid->Vnode, fid->Unique, severely));

    int retcode = 0;

    /* Setup message. */
    union outputArgs msg;

    if (severely) {
	msg.coda_purgefid.oh.opcode = CODA_PURGEFID;
	msg.coda_purgefid.oh.unique = 0;
	msg.coda_purgefid.CodaFid = *fid;
    } else if (ISDIR(*fid)) {
	msg.coda_zapdir.oh.opcode = CODA_ZAPDIR;
	msg.coda_zapdir.oh.unique = 0;
	msg.coda_zapdir.CodaFid = *fid;
    } else {
	msg.coda_zapfile.oh.opcode = CODA_ZAPFILE;
	msg.coda_zapfile.oh.unique = 0;
	msg.coda_zapfile.CodaFid = *fid;
    }	

    /* Send the message. */
    if (MsgWrite((char *)&msg, (int) sizeof(msg)) !=  (int) sizeof(msg)) {
	retcode = errno;
	CHOKE("k_Purge: %s, message write fails: errno %d", 
	      msg.oh.opcode == CODA_PURGEFID ? "CODA_PURGEFID" :
	      msg.oh.opcode == CODA_ZAPFILE ? "CODA_ZAPFILE" : "CODA_ZAPDIR", retcode);
    }

    LOG(100, ("k_Purge: %s, returns %d\n", 
	      msg.oh.opcode == CODA_PURGEFID ? "CODA_PURGEFID" :
	      msg.oh.opcode == CODA_ZAPFILE ? "CODA_ZAPFILE" : "CODA_ZAPDIR", retcode));
    if (retcode == 0) {
	VFSStats.VFSOps[msg.oh.opcode].success++;
    }
    else {
	VFSStats.VFSOps[msg.oh.opcode].failure++;
    }

    return(retcode == 0);
}


int k_Purge(vuid_t vuid) {
    if (KernelMask == 0) return(1);

    LOG(1, ("k_Purge: vuid = %d\n", vuid));

    /* Message prefix. */
    union outputArgs msg;
    msg.coda_purgeuser.oh.unique = 0;
    msg.coda_purgeuser.oh.opcode = CODA_PURGEUSER;

    /* Message data. */
    bzero((void *)&msg.coda_purgeuser.cred, (int) sizeof(struct coda_cred));
    msg.coda_purgeuser.cred.cr_uid = vuid;

    /* Send the message. */
    if (MsgWrite((char *)&msg, (int) sizeof(union outputArgs)) != (int) sizeof(union outputArgs))
	CHOKE("k_Purge: PurgeUser, message write");

    LOG(1, ("k_Purge: PurgeUser, returns 0\n"));
    VFSStats.VFSOps[CODA_PURGEUSER].success++;

    return(1);
}

int k_Replace(ViceFid *fid_1, ViceFid *fid_2) {
    if (KernelMask == 0) return(1);

    if (!fid_1 || !fid_2)
	CHOKE("k_Replace: nil fids");

    LOG(0, ("k_Replace: ViceFid (%x.%x.%x) with ViceFid (%x.%x.%x) in mini-cache\n", 
	    fid_1->Volume, fid_1->Vnode, fid_1->Unique, fid_2->Volume, 
	    fid_2->Vnode, fid_2->Unique));

    /* Message prefix. */
    struct coda_replace_out msg;
    msg.oh.unique = 0;
    msg.oh.opcode = CODA_REPLACE;

    msg.OldFid = *fid_1;
    msg.NewFid = *fid_2;
	
    /* Send the message. */
    if (MsgWrite((char *)&msg, sizeof (struct coda_replace_out)) != sizeof (struct coda_replace_out))
	CHOKE("k_Replace: message write");

    LOG(0, ("k_Replace: returns 0\n"));
    VFSStats.VFSOps[CODA_REPLACE].success++;

    return(1);
}

/* -------------------------------------------------- */
void WorkerInit() {
    if (MaxWorkers == UNSET_MAXWORKERS)
        MaxWorkers = DFLT_MAXWORKERS;

    if (MaxPrefetchers == UNSET_MAXWORKERS)
        MaxPrefetchers = DFLT_MAXPREFETCHERS;
    else
        if (MaxPrefetchers > MaxWorkers) { /* whoa */
            eprint("WorkerInit: MaxPrefetchers %d, MaxWorkers only %d!",
                  MaxPrefetchers, MaxWorkers);
            exit(-1);
        }

#if defined(DJGPP) || defined(__CYGWIN32__)
    worker::muxfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (worker::muxfd < 0) {
            eprint("WorkerInit: socket() returns %d", errno);
            exit(-1);
    }
    if (worker::muxfd >= NFDS) {
            eprint("WorkerInit: worker::muxfd >= %d!", NFDS);
            exit(-1);
    }

    dprint("WorkerInit: muxfd = %d", worker::muxfd);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8000);
    if (::bind(worker::muxfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            eprint("WorkerInit: bind() returns %d", errno);
            exit(-1);
    }
#else 
    /* Open the communications channel. */
    worker::muxfd = ::open(kernDevice, O_RDWR, 0);
    if (worker::muxfd < 0) {
        eprint("WorkerInit: open(%s, O_RDWR, 0) returns %d", kernDevice, errno);
        exit(-1);
    }
    if (worker::muxfd >= NFDS) {
        eprint("WorkerInit: worker::muxfd >= %d!", NFDS);
        exit(-1);
    }
#ifdef  __BSD44__
    if (::ioctl(worker::muxfd, CIOC_KERNEL_VERSION, &kernel_version) < 0 ) {
        eprint("WorkerInit errno %d: Version IOCTL FAILED!  Get a newer Kernel!"
, errno);
        exit(-1);
    } switch (kernel_version) {
        case 1:
        case 2: /* luckily 1 & 2 are upwards compatible */
            break;
        default:
        eprint("WorkerInit: Version Skew with kernel!  Get a newer kernel!");
        eprint("WorkerInit: Kernel version is %d\n.", kernel_version);
        exit(-1);
    }
#endif
#endif
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
    
    if (in->ih.opcode != CODA_IOCTL)
	return(0);

    return (in->coda_ioctl.cmd == VIOCPREFETCH);
}

void DispatchWorker(msgent *m) {
    /* We filter out signals (i.e., interrupts) before passing messages on to workers. */
    union inputArgs *in = (union inputArgs *)m->msg_buf;
    
    if (in->ih.opcode == CODA_SIGNAL) {
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
	if (FID_EQ(&w->StoreFid, fid)) {
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
	    CHOKE("worker::Resign: result == EINCONS");
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
	CHOKE("worker::Return: already returned!");

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
	    if (errn != ESRCH) CHOKE("worker::Return: errno (%d) from MsgWrite", errno);
	    interrupted = 1;
	}
    }

    returned = 1;
}


void worker::Return(int code) {
    ((union outputArgs*)msg->msg_buf)->oh.result = code; 
    Return(msg, (int)sizeof (struct coda_out_hdr));
}


void worker::main(void *parm) {
    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

    for (;;) {
	/* Wait for new request. */
	AwaitRequest();

	/* Sanity check new request. */
	if (idle) CHOKE("Worker: signalled but not dispatched!");
	if (!msg) CHOKE("Worker: no message!");

	union inputArgs *in = (union inputArgs *)msg->msg_buf;
	union outputArgs *out = (union outputArgs *)msg->msg_buf;
	
	interrupted = 0;
	returned = 0;
	StoreFid = NullFid;

	/* Fill in the user-specific context. */
	u.Init();
	u.u_priority = FSDB->StdPri();
	u.u_flags = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);

	/* GOTTA BE ME */
	u.u_cred = (in)->ih.cred;
	u.u_pid  = (in)->ih.pid;
	u.u_pgid = (in)->ih.pgid;

	/* This switch corresponds to the kernel trap handler. */

	switch (in->ih.opcode) {
	    int size;
	    
	    case CODA_ACCESS:
		{
		LOG(100, ("CODA_ACCESS: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_access.VFid, 0);
		access(&vtarget, in->coda_access.flags);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_CLOSE:
		{
		LOG(100, ("CODA_CLOSE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_close.VFid, 0);
		close(&vtarget, in->coda_close.flags);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	  case CODA_CREATE:
		{
		LOG(100, ("CODA_CREATE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid,u.u_pgid));

		struct venus_cnode vparent;
		MAKE_CNODE(vparent, in->coda_create.VFid, 0);
		struct venus_cnode target;
		create(&vparent, (char *)in + (int)in->coda_create.name,
		       &in->coda_create.attr, in->coda_create.excl,
		       in->coda_create.mode, &target);

		if (u.u_error == 0) {
		    out->coda_create.VFid = target.c_fid;
		    out->coda_create.attr = in->coda_create.attr;
		    size = (int)sizeof (struct coda_create_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CODA_FSYNC:	
		{
		LOG(100, ("CODA_FSYNC: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_fsync.VFid, 0);
		fsync(&vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_GETATTR:
		{
		LOG(100, ("CODA_GETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_getattr.VFid, 0);
		va_init(&out->coda_getattr.attr);
		getattr(&vtarget, &out->coda_getattr.attr);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_getattr_out));
		break;
		}

	    case CODA_INACTIVE: 
		{
		LOG(100, ("CODA_INACTIVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_inactive.VFid, 0);
		inactive(&vtarget);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_IOCTL:
		{
		char outbuf[VC_MAXDATASIZE];
		struct ViceIoctl data;
		data.in = (char *)in + (int)in->coda_ioctl.data;
		data.in_size = 0;
		data.out = outbuf;	/* Can't risk overcopying. Sigh. -dcs */
		data.out_size =	0;

		LOG(100, ("CODA_IOCTL: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		if (in->coda_ioctl.cmd == VIOCPREFETCH)
		    worker::nprefetchers++;

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_ioctl.VFid, 0);
		data.in_size = in->coda_ioctl.len;
		ioctl(&vtarget, in->coda_ioctl.cmd, &data, in->coda_ioctl.rwflag);

		out->oh.result = u.u_error;
		out->coda_ioctl.len = data.out_size;
		out->coda_ioctl.data = (char *)(sizeof (struct coda_ioctl_out));
		bcopy(data.out, (char *)out + (int)out->coda_ioctl.data, data.out_size);
		Resign(msg, (int) sizeof (struct coda_ioctl_out) + data.out_size);

		if (in->coda_ioctl.cmd == VIOCPREFETCH)
		    worker::nprefetchers--;

		break;
		}

	    case CODA_LINK:
		{
		LOG(100, ("CODA_LINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vsource;
		MAKE_CNODE(vsource, in->coda_link.sourceFid, 0);
		struct venus_cnode vp_target;
		MAKE_CNODE(vp_target, in->coda_link.destFid, 0);
		link(&vsource, &vp_target, (char *)in + (int)in->coda_link.tname);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_LOOKUP:
		{
		LOG(100, ("CODA_LOOKUP: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vparent;
		MAKE_CNODE(vparent, in->coda_lookup.VFid, 0);
		struct venus_cnode target;

		lookup(&vparent, (char *)in + (int)in->coda_lookup.name, &target, (int)in->coda_lookup.flags);

		out->oh.result = u.u_error;
		if (u.u_error == 0) {
		    out->coda_lookup.VFid = target.c_fid;
		    out->coda_lookup.vtype = target.c_type;
		    if (out->coda_lookup.vtype == C_VLNK &&
			target.c_flags & C_INCON)
			    out->coda_lookup.vtype |= CODA_NOCACHE;
		    size = sizeof (struct coda_lookup_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		
		Resign(msg,  size);

		break;
		}

	    case CODA_MKDIR:
		{
		LOG(100, ("CODA_MKDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vparent;
		MAKE_CNODE(vparent, in->coda_mkdir.VFid, 0);
		struct venus_cnode target;
		mkdir(&vparent, (char *)in + (int)in->coda_mkdir.name, &in->coda_mkdir.attr, &target);

		if (u.u_error == 0) {
		    out->coda_mkdir.VFid = target.c_fid;
		    out->coda_mkdir.attr = in->coda_mkdir.attr;
		    size = sizeof (struct coda_mkdir_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CODA_OPEN:
		{
		LOG(100, ("CODA_OPEN: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		/* If an abort happens, we create a message and pass it off to
		 * another worker. Why not do the close ourselves? Oh well,
		 * I'll leave it this way... -- DCS
		 */
		ViceFid saveFid = in->coda_open.VFid;
		int saveFlags = in->coda_open.flags;
		
		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_open.VFid, 0);
		open(&vtarget, in->coda_open.flags);
		
		if (u.u_error == 0) {
		    MarinerReport(&vtarget.c_fid, CRTORUID(u.u_cred));
		}

		if (u.u_error == 0) {
		    out->coda_open.dev = vtarget.c_device;
		    out->coda_open.inode = vtarget.c_inode;
		    size = sizeof (struct coda_open_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		
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
		    
		    dog->coda_close.ih.unique = (u_long)-1;
		    dog->coda_close.ih.opcode = CODA_CLOSE;
		    dog->coda_close.VFid = saveFid;
		    dog->coda_close.flags = saveFlags;
		    
		    /* Dispatch it. */
		    DispatchWorker(fm);
		}

		break;
		}

	    case CODA_OPEN_BY_PATH:
		{
		LOG(100, ("CODA_OPEN_BY_PATH: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		ViceFid saveFid = in->coda_open_by_path.VFid;
		int saveFlags = in->coda_open_by_path.flags;
		
		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_open_by_path.VFid, 0);

		open(&vtarget, in->coda_open_by_path.flags);
		
		if (u.u_error == 0) {
		    MarinerReport(&vtarget.c_fid, CRTORUID(u.u_cred));
		}

		if (u.u_error == 0) {
			char *slash;
			char *begin = (char *)(&out->coda_open_by_path.path + 1);
			out->coda_open_by_path.path = begin - (char *)out;
			sprintf(begin, "%s%s/%s", CF_PREFIX, CacheDir, 
				vtarget.c_cfname);
			LOG(100, ("CODA_OPEN_BY_PATH: returning %s", begin));
#if defined(DJGPP) || defined(__CYGWIN32__)
			slash = begin;
			for (slash = begin ; *slash ; slash++ ) {
				if ( *slash == '/' ) 
					*slash='\\';
			}
#endif
			size = sizeof (struct coda_open_by_path_out) + 
				strlen(begin) + 1;
		} else
			size = sizeof (struct coda_out_hdr);
		
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
		    
		    dog->coda_close.ih.unique = (u_long)-1;
		    dog->coda_close.ih.opcode = CODA_CLOSE;
		    dog->coda_close.VFid = saveFid;
		    dog->coda_close.flags = saveFlags;
		    
		    /* Dispatch it. */
		    DispatchWorker(fm);
		}

		break;
		}


	    case CODA_READLINK: 
		{
		LOG(100, ("CODA_READLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_readlink.VFid, 0);
		struct coda_string string;
		string.cs_buf = (char *)out + sizeof(struct coda_readlink_out);
		string.cs_maxlen = CODA_MAXPATHLEN;
		readlink(&vtarget, &string);

		if (u.u_error == 0) {
		    MarinerReport(&(vtarget.c_fid), CRTORUID(u.u_cred));
		}

		out->oh.result = u.u_error;
		out->coda_readlink.count = string.cs_len;
		/* readlink.data is an offset, with the wrong type .. sorry */
		out->coda_readlink.data = (char *)(sizeof (struct coda_readlink_out));
		Resign(msg, (int) sizeof (struct coda_readlink_out) + out->coda_readlink.count);
		break;
		}

	    case CODA_REMOVE:
		{
		LOG(100, ("CODA_REMOVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		struct venus_cnode vparent;
		MAKE_CNODE(vparent, in->coda_remove.VFid, 0);
		remove(&vparent, (char *)in + (int)in->coda_remove.name);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_RENAME:
		{
		LOG(100, ("CODA_RENAME: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vp_source;
		MAKE_CNODE(vp_source, in->coda_rename.sourceFid, 0);
		struct venus_cnode vp_target;
		MAKE_CNODE(vp_target, in->coda_rename.destFid, 0);
		rename(&vp_source, (char *)in + (int)in->coda_rename.srcname,
		       &vp_target, (char *)in + (int)in->coda_rename.destname);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_RMDIR:
		{
		LOG(100, ("CODA_RMDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vparent;
		MAKE_CNODE(vparent, in->coda_rmdir.VFid, 0);
		rmdir(&vparent, (char *)in + (int)in->coda_rmdir.name);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_ROOT:
		{
		struct venus_cnode target;
		root(&target);

		if (u.u_error == 0) {
		    out->coda_root.VFid = target.c_fid;
		    size = sizeof (struct coda_root_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		    
		out->oh.result = u.u_error;
		Resign(msg, size);
		break;
		}

	    case CODA_SETATTR:
		{
		LOG(100, ("CODA_SETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vtarget;
		MAKE_CNODE(vtarget, in->coda_setattr.VFid, 0);
		setattr(&vtarget, &in->coda_setattr.attr);

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_SYMLINK:
		{
		LOG(100, ("CODA_SYMLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode vp_target;
		MAKE_CNODE(vp_target, in->coda_symlink.VFid, 0);
		symlink(&vp_target, (char *)in + (int)in->coda_symlink.srcname, &in->coda_symlink.attr, (char *)in + (int)in->coda_symlink.tname);
	

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_SYNC:
		{
		sync();

		out->oh.result = u.u_error;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		break;
		}

	    case CODA_VGET:
		{
		LOG(100, ("CODA_VGET: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		struct venus_cnode target;
		struct cfid fid;
		fid.cfid_len = (unsigned short)sizeof(ViceFid);
		fid.cfid_fid = in->coda_vget.VFid;
		vget(&target, &fid);

		out->oh.result = u.u_error;
		if (u.u_error == 0) {
		    out->coda_vget.VFid = target.c_fid;
		    out->coda_vget.vtype = target.c_type;
		    if (out->coda_vget.vtype == C_VLNK && target.c_flags & C_INCON)
			out->coda_vget.vtype |= CODA_NOCACHE;
		    size = sizeof (struct coda_vget_out);
		} else
		    size = sizeof (struct coda_out_hdr);
		
		Resign(msg, size);

		break;
		}

	    default:	 /* Toned this down a bit, used to be a choke -- DCS */
		{	/* But make sure someone sees it! */
		eprint("worker::main Got a bogus opcode %d", in->ih.opcode);
		dprint("worker::main Got a bogus opcode %d\n", in->ih.opcode);
		MarinerLog("worker::main Got a bogus opcode %d\n", in->ih.opcode);

		out->oh.result = EOPNOTSUPP;
		Resign(msg, (int) sizeof (struct coda_out_hdr));
		}
	}
    }
}


worker_iterator::worker_iterator() : vproc_iterator(VPT_Worker) {
}


worker *worker_iterator::operator()() {
    return((worker *)vproc_iterator::operator()());
}

