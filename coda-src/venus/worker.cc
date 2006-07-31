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
 *
 * Implementation of the Venus Worker subsystem.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef  __FreeBSD__
#include <sys/param.h>
#endif

#ifdef __linux__
#if !defined(__GLIBC__) || __GLIBC__ < 2
#include <linux/fs.h>
#endif
#include <mntent.h>
#include <sys/utsname.h>
#endif

#include <sys/wait.h>

#ifdef sun
#include <sys/mnttab.h>
#include <sys/filio.h>
#include <sys/types.h>
#endif

#include <vice.h>

#ifdef DJGPP
#include <relay.h>
#endif

#ifdef __CYGWIN__
#include <windows.h>
#endif

#ifdef __cplusplus
}
#endif

/* Darwin uses the same venus-kernel interface as BSD */
#if defined(__APPLE__) && defined(__MACH__)
#define __BSD44__
#endif

/* interfaces */
/* from vicedep */
#include <venusioctl.h>

/* from venus */
#include "comm.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"

#include "nt_util.h"


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
#endif

const int WorkerStackSize = 131072;

int MaxWorkers = UNSET_MAXWORKERS;
int MaxPrefetchers = UNSET_MAXWORKERS;
int KernelFD = -1;	/* subsystem is uninitialized until fd is not -1 */
int kernel_version = 0;
static int Mounted = 0;

/* Only for the crazy people among us.
 * Many things can and will go wrong when venus reattaches to a previously
 * mounted mountpoint. But it does help during development ;) --JH */
int allow_reattach = 0;

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
    while ((m = next()))
	if (((union inputArgs *)m->msg_buf)->ih.unique == seq) return(m);

    return(0);
}


int MsgRead(msgent *m) 
{
#ifdef DJGPP
	size_t cc = read_relay(m->msg_buf);

#elif defined(__CYGWIN32__)
        DWORD size;  
        size_t cc = read(worker::muxfd, (char *)&size, sizeof(size)); 
	CODA_ASSERT(size <= VC_MAXMSGSIZE);
	cc = read(worker::muxfd, m->msg_buf, (int)size);
#else
	size_t cc = read(worker::muxfd, m->msg_buf, (int) (VC_MAXMSGSIZE));
#endif
	if (cc < sizeof(struct coda_in_hdr)) 
		return(-1);

	return(0);
}


size_t MsgWrite(char *buf, int size) 
{
#ifdef DJGPP
	 return write_relay(buf, size);
#elif defined(__CYGWIN32__)
	 return nt_msg_write(buf, size);
#else 
	return write(worker::muxfd, buf, size);
#endif
}


msgent::msgent() 
{
#ifdef VENUSDEBUG
    allocs++;
#endif
}


msgent::~msgent() 
{
#ifdef VENUSDEBUG
    deallocs++;
#endif
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
#else
	int fd = -1;
	char *str, *p, *q = NULL;
	CODA_ASSERT((str = p = strdup(kernDevice)) != NULL);

	for(p = strtok(p, ","); p && fd == -1; p = strtok(NULL, ",")) {
	    fd = ::open(p, O_RDWR, 0);
	    if (fd >= 0) 
		q = p;
	}

	/* If the open of the kernel device succeeds we know that there is
	   no other living venus. */
	if (fd < 0) {
	    eprint("Probably another Venus is running! open failed for %s, exiting",
		   kernDevice);
	    free(str);
	    exit(-1);
	}

	CODA_ASSERT(q);
	kernDevice = strdup(q);
	free(str);

	/* Construct a purge message */
	union outputArgs msg;
	memset(&msg, 0, sizeof(msg));

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
	    eprint("close of %s failed (%d), exiting",
		   kernDevice, errno);
	    exit(-1);
	}
#endif
}

void VFSMount()
{
    /* Linux (and NetBSD) Coda filesystems are mounted through forking since
     * they need venus.
     */

#ifdef __BSD44__ /* BSD specific preamble */
    /* Silently unmount the root node in case an earlier venus exited without
     * successfully unmounting. */
    unmount(venusRoot,0);
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
#endif /* __BSD44__ */

#ifdef __linux__ /* Linux specific preamble */
    int islinux20 = 0;
    int mounted = 0;
    struct utsname un;
    struct mntent *ent;
    FILE *fd;

    /* Test if we are running on a 2.0 kernel. In that case we need to pass
       different mount arguments. */
    uname(&un);
    islinux20 = strncmp(un.release, "2.0.", 4) == 0 ? 1 : 0;

    fd = setmntent("/etc/mtab", "r");
    if (fd) { 
	while (1) {
	    ent = getmntent(fd);
	    if (!ent) break;

	    if ((STREQ(ent->mnt_fsname, "Coda") ||
		 STREQ(ent->mnt_fsname, "coda")) &&
		STREQ(ent->mnt_dir, venusRoot)) {
		mounted = 1;
		break;
	    }
	}
	endmntent(fd);

	if (mounted) {
	    eprint("%s already mounted", venusRoot);
	    if (allow_reattach) {
		kill(getpid(), SIGUSR1);
		return;
	    }
	    exit(-1);
	}
    }
#endif /* __linux__ */

#if defined(__BSD44__) || defined(__linux__)
    pid_t child = fork();
    if (child == 0) {
	pid_t parent;
	int error = 0;
	
	parent = getppid();

	/* Use a double fork to avoid having to reap zombie processes
	 * http://www.faqs.org/faqs/unix-faq/faq/part3/section-13.html
	 *
	 * The child will be reaped by Venus immediately, and as a result the
	 * grandchild has no parent and init will take care of the 'orphan'.
	 */
	if (fork() != 0)
	    goto child_done;

#ifdef __BSD44__ /* BSD specific mount */
	/* Issue the VFS mount request. */
	error = mount("coda", venusRoot, 0, kernDevice);
	if (error < 0)
	    error = mount("cfs", venusRoot, 0, kernDevice);
#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
#define MOUNT_CFS 19
	if (error < 0)
	    error = mount(MOUNT_CFS, venusRoot, 0, kernDevice);
#endif
#endif /* __BSD44__ */

#ifdef __linux__ /* Linux specific mount */
	struct coda_mount_data mountdata;
	mountdata.version = CODA_MOUNT_VERSION;
	mountdata.fd = worker::muxfd;

	error = mount("coda", venusRoot, "coda",
		      MS_MGC_VAL | MS_NOATIME | MS_NODEV | MS_NOSUID,
		      islinux20 ? (void *)&kernDevice : (void *)&mountdata);


	if (!error) {
	    FILE *fd = setmntent("/etc/mtab", "a");
	    struct mntent ent;
	    if (fd) { 
		ent.mnt_fsname = "coda";
		ent.mnt_dir    = venusRoot;
		ent.mnt_type   = "coda";
		ent.mnt_opts   = "rw,noatime,nosuid,nodev";
		ent.mnt_freq   = 0;
		ent.mnt_passno = 0;
		addmntent(fd, &ent);
		endmntent(fd);
	    }
	}
#endif /* __linux__ */

	if (error < 0) {
	    LOG(0, ("CHILD: mount system call failed. Killing parent.\n"));
	    eprint("CHILD: mount system call failed. Killing parent.\n");
	    kill(parent, SIGKILL);
	} else {
	    eprint("%s now mounted.", venusRoot);
	    kill(parent, SIGUSR1);
	}

child_done:
	WorkerCloseMuxfd();
	exit(error < 0 ? 1 : 0);
    }

    /* we just wait around to reap the first child */
    pid_t pid;
    do {
	pid = waitpid(child, NULL, 0);
    } while (pid == -1 && errno == EINTR);
#endif /* __BSD44__ || __linux__ */

#ifdef sun
    { int error;
      /* Do a umount just in case it is mounted. */
      error = umount(venusRoot);
      if (error) {
	if (errno != EINVAL) {
	    eprint("unmount(%s) failed (%d), exiting", venusRoot, errno);
	    exit(-1);
	}
      } else
	eprint("unmount(%s) succeeded, continuing", venusRoot);
    }
    /* New mount */
    CODA_ASSERT (!mount(kernDevice, venusRoot, MS_DATA, "coda", NULL, 0));
    /* Update the /etc mount table entry */
    { int lfd, mfd;
      int lck;
      FILE *mnttab;
      char tm[25];
      struct mnttab mt;

      lfd = open ("/etc/.mnttab.lock", O_WRONLY|O_CREAT, 0600);
      if (lfd >= 0) {

	lck = lockf(lfd, F_LOCK, 0);
	if (lck == 0) {
	
	  mnttab = fopen(MNTTAB, "a+");
	  if (mnttab != NULL) {
	    mt.mnt_special = "CODA";
	    mt.mnt_mountp = venusRoot;
	    mt.mnt_fstype = "CODA";
	    mt.mnt_mntopts = "rw";
	    mt.mnt_time = tm;
	    sprintf (tm, "%d", time(0));
	    
	    putmntent(mnttab,&mt);
	    
	    fclose(mnttab);
	  } else
	    eprint ("Could not update /etc/mnttab.");
	  (void) lockf(lfd, F_ULOCK, 0);
	} else
	  eprint ("Could not update /etc/mnttab.");
	close(lfd);
      } else
	eprint ("Could not update /etc/mnttab.");
    }
#endif

#ifdef DJGPP
    int res;
    eprint ("Mounting on %s", venusRoot);
    res = mount_relay(venusRoot);
    if (res)
	    eprint("Mount OK");
    else{
	    eprint ("Mount failed");
	    close_relay();
	    exit(0);
    }
#endif

#ifdef __CYGWIN32__
    /* Mount by starting another thread. */
    eprint ("Mounting on %s", venusRoot);
    nt_mount (venusRoot);
#endif

    Mounted = 1;
}

int VFSUnload()
{
#ifdef DJGPP
  	int i = 0, res;
	do {
		i++;
		eprint ("UNLOAD result %d.", res = unload_vxd("CODADEV"));    
    
	} while (i < 10 && res == 0);
	if (i==10 && res ==0) return -1;
#endif
	return 0;
}

void VFSUnmount() 
{
#ifdef DJGPP
    int res;
    
    if (!Mounted) return;

    res = unmount_relay();
    eprint (res ? "Unmount OK" : "Unmount failed");

    res = close_relay();
    eprint (res ? "Close relay OK" : "Close relay failed");

    res = VFSUnload();
    eprint (res == 0 ? "Kernel module unloaded" :
	               "Kernel module could not be unloaded");

    if (res == 0) KernelFD = -1;	

    return;
#endif
    /* Purge the kernel cache so that all cnodes are (hopefully) released. */
    k_Purge();

    WorkerCloseMuxfd();

    if (!Mounted) return;

#ifdef	__BSD44__
    /* For now we can not unmount, because an coda_root() upcall could
       nail us. */
#ifndef	__BSD44__
    /* Issue the VFS unmount request. */
    if(unmount(venusRoot,0) < 0) {
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

#ifdef sun
    {
      int res;
      char line[1024];
      int lfd, mfd;
      int lck;
      FILE *mnttab;
      FILE *newtab;

      res = umount (venusRoot);
      if (res)
        eprint ("Unmount failed.");
      /* Remove CODA entry from /etc/mnttab */

      lfd = open ("/etc/.mnttab.lock", O_WRONLY|O_CREAT, 0600);
      if (lfd >= 0) {

	lck = lockf(lfd, F_LOCK, 0);
	if (lck == 0) {
	
	  mnttab = fopen(MNTTAB, "r+");
	  if (mnttab != NULL) {
	    newtab = fopen("/etc/newmnttab", "w+");
	    if (newtab != NULL) {
	      while (fgets(line, 1024, mnttab)) {
		if (strncmp("CODA", line, 4) != 0) {
		  fprintf (newtab, "%s", line);
		}
	      }
	      fclose(newtab);
	      fclose(mnttab);
	      unlink(MNTTAB);
	      rename("/etc/newmnttab", MNTTAB);
	    } else {
	      eprint ("Could not remove Coda from /etc/mnttab");
	      fclose(mnttab);
	    }
	  } else
	    eprint ("Could not remove CODA from /etc/mnttab.");
	  (void) lockf(lfd, F_ULOCK, 0);
	} else
	  eprint ("Could not remove CODA from /etc/mnttab.");
	close(lfd);
      } else
	eprint ("Could not remove CODA from /etc/mnttab.");
      sync();
    }
#endif

#ifdef __CYGWIN32__
    eprint ("Unmounting %s", venusRoot);
    nt_umount (venusRoot);
#endif


}


int k_Purge() {
    size_t size;

    if (KernelFD == -1) return(1);

    LOG(1, ("k_Purge: Flush\n"));

    /* Construct a purge message. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.oh.opcode = CODA_FLUSH;
    msg.oh.unique = 0;
    size = sizeof(struct coda_out_hdr);
    
    /* Send the message. */
    if (MsgWrite((char *)&msg, size) != size)
	    CHOKE("k_Purge: Flush, message write returns %d", errno);

    LOG(1, ("k_Purge: Flush, returns 0\n"));
    VFSStats.VFSOps[CODA_FLUSH].success++;

    return(1);
}


int k_Purge(VenusFid *fid, int severely) {
    size_t size;

    if (KernelFD == -1) return(1);

    LOG(100, ("k_Purge: fid = (%s), severely = %d\n", FID_(fid), severely));

    int retcode = 0;

    /* Setup message. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    if (severely) {
	msg.coda_purgefid.oh.opcode = CODA_PURGEFID;
	msg.coda_purgefid.oh.unique = 0;
	msg.coda_purgefid.Fid = *VenusToKernelFid(fid);
	size = sizeof(msg.coda_purgefid);
    } else if (ISDIR(*fid)) {
	msg.coda_zapdir.oh.opcode = CODA_ZAPDIR;
	msg.coda_zapdir.oh.unique = 0;
	msg.coda_zapdir.Fid = *VenusToKernelFid(fid);
	size = sizeof(msg.coda_zapdir);
    } else {
	msg.coda_zapfile.oh.opcode = CODA_ZAPFILE;
	msg.coda_zapfile.oh.unique = 0;
	msg.coda_zapfile.Fid = *VenusToKernelFid(fid);
	size = sizeof(msg.coda_zapfile);
    }	

    /* Send the message. */
    if (MsgWrite((char *)&msg, (int) size) != size) {
	retcode = errno;
	LOG(0, ("k_Purge: %s, message write fails: errno %d\n", 
	      msg.oh.opcode == CODA_PURGEFID ? "CODA_PURGEFID" :
	      msg.oh.opcode == CODA_ZAPFILE ? "CODA_ZAPFILE" : "CODA_ZAPDIR",
	      retcode));
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


int k_Purge(uid_t uid)
{
    size_t size;

    if (KernelFD == -1) return(1);

    LOG(1, ("k_Purge: uid = %d\n", uid));

    /* Message prefix. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    msg.coda_purgeuser.oh.unique = 0;
    msg.coda_purgeuser.oh.opcode = CODA_PURGEUSER;

    /* Message data. */
    msg.coda_purgeuser.uid = uid;
    size = sizeof(msg.coda_purgeuser);

    /* Send the message. */
    if (MsgWrite((char *)&msg, size) != size)
	CHOKE("k_Purge: PurgeUser, message write");

    LOG(1, ("k_Purge: PurgeUser, returns 0\n"));
    VFSStats.VFSOps[CODA_PURGEUSER].success++;

    return(1);
}

int k_Replace(VenusFid *fid_1, VenusFid *fid_2) {
    if (KernelFD == -1) return(1);

    if (!fid_1 || !fid_2)
	CHOKE("k_Replace: nil fids");

    LOG(0, ("k_Replace: VenusFid (%s) with VenusFid (%s) in mini-cache\n", 
	    FID_(fid_1), FID_(fid_2)));

    /* Message prefix. */
    struct coda_replace_out msg;
    msg.oh.unique = 0;
    msg.oh.opcode = CODA_REPLACE;

    msg.OldFid = *VenusToKernelFid(fid_1);
    msg.NewFid = *VenusToKernelFid(fid_2);
	
    /* Send the message. */
    if (MsgWrite((char *)&msg, sizeof (struct coda_replace_out)) != sizeof (struct coda_replace_out))
	CHOKE("k_Replace: message write");

    LOG(0, ("k_Replace: returns 0\n"));
    VFSStats.VFSOps[CODA_REPLACE].success++;

    return(1);
}

/* -------------------------------------------------- */
void WorkerInit()
{
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

#ifdef DJGPP
    if (!init_relay()){
	    LOG(0, ("init_relay failed.\n"));
	    exit(-1);
    }
    worker::muxfd = MCFD;
    dprint("WorkerInit: muxfd = %d\n", worker::muxfd);
#elif defined(__CYGWIN32__)
    int sd[2];
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sd)) {
	    eprint("WorkerInit: socketpair() returns %d", errno);
	    exit(-1);
    }
    worker::muxfd = sd[0];
    if (!nt_initialize_ipc (sd[1])) {
            dprint("WorkerInit: nt_initialize_ipc failed.\n");
            exit (-1);
    }
    dprint("WorkerInit: muxfd = %d\n", worker::muxfd);
#else 
    /* Open the communications channel. */
    worker::muxfd = ::open(kernDevice, O_RDWR, 0);
    if (worker::muxfd == -1) {
        eprint("WorkerInit: open %s failed", kernDevice);
        exit(-1);
    }
#endif

    if (worker::muxfd >= NFDS) {
        eprint("WorkerInit: worker::muxfd >= %d!", NFDS);
        exit(-1);
    }

#if defined(__BSD44__) || defined(__linux__)
    if (::ioctl(worker::muxfd, CIOC_KERNEL_VERSION, &kernel_version) >= 0 ) {
        switch (kernel_version) {
        case 3:
            break;
        case 2: /* 1 & 2 are upwards compatible, but 3 introduced the realms */
        case 1:
        default:
            eprint("WorkerInit: Version Skew with kernel! Get a newer kernel!");
            eprint("WorkerInit: Kernel version is %d\n.", kernel_version);
            exit(-1);
        }
    } else {
        eprint("Kernel version ioctl failed.");
    }
#endif

    /* Flush kernel cache(s). */
    k_Purge();

    /* Allows the MessageMux to distribute incoming messages to us. */
    KernelFD = worker::muxfd;

    worker::nworkers = 0;
    worker::nprefetchers = 0;
    worker::lastresign = Vtime();
}


int WorkerCloseMuxfd(void)
{
    int ret = 0;

    if (KernelFD)
	ret = close(worker::muxfd);

    worker::muxfd = KernelFD = -1;

    return ret;
}

worker *FindWorker(u_long seq) {
    worker_iterator next;
    worker *w;
    while ((w = next()))
	if (w->msg && ((union inputArgs *)w->msg)->ih.unique == seq) return(w);

    return(0);
}


worker *GetIdleWorker() {
    worker_iterator next;
    worker *w;
    while ((w = next()))
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

    return (_IOC_NR(in->coda_ioctl.cmd) == _VIOCPREFETCH);
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


void WorkerMux(fd_set *mask)
{
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


time_t GetWorkerIdleTime() {
    /* Return 0 if any call is in progress. */
    worker_iterator next;
    worker *w;
    while ((w = next()))
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
    while ((w = next())) w->print(fd);
}


/* -------------------------------------------------- */

worker::worker() : vproc("Worker", NULL, VPT_Worker, WorkerStackSize)
{
    LOG(100, ("worker::worker(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    nworkers++;	    /* Ought to be a lock protecting this! -JJK */

    returned = 0;
    StoreFid = NullFid;
    msg = 0;
    opcode = 0;
    
    /* Poke main procedure. */
    start_thread();
}


/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
worker::worker(worker& w) : vproc(*((vproc *)&w)) {
    abort();
}


int worker::operator=(worker& w) {
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
	const char *opstr = VenusOpStr((int) ((union outputArgs*)msg->msg_buf)->oh.opcode);
	const char *retstr = VenusRetStr((int) ((union outputArgs *)msg->msg_buf)->oh.result);
	
#ifdef TIMING
	float elapsed;
	elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
	LOG(1, ("[Return Done] %s : returns %s, elapsed = %3.1f\n",
		opstr, retstr, elapsed));
#else /* !TIMING */
	LOG(1, ("[Return Done] %s : returns %s\n", opstr, retstr))
#endif
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
    msg = NULL;
    opcode = 0;
    
    lastresign = Vtime();
}


void worker::Return(msgent *msg, size_t size) {
    if (returned)
	CHOKE("worker::Return: already returned!");

    const char *opstr = VenusOpStr((int) ((union outputArgs*)msg->msg_buf)->oh.opcode);
    const char *retstr = VenusRetStr((int) ((union outputArgs*)msg->msg_buf)->oh.result);

#ifdef	TIMING
    float elapsed;
    if (u.u_tv2.tv_sec != 0) {
	elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
	LOG(1, ("%s : returns %s, elapsed = %3.1f msec\n",
		opstr, retstr, elapsed));
    } else {
	LOG(1, ("%s : returns %s, elapsed = unknown msec (Returning early)\n",
		opstr, retstr));
    }
#else /* !TIMING */
    LOG(1, ("%s : returns %s\n", opstr, retstr));
#endif

    /* There is no reply to an interrupted operation. */
    if (!interrupted) {
	size_t cc = MsgWrite(msg->msg_buf, size);
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

void worker::main(void)
{
    struct venus_cnode vparent;
    struct venus_cnode vtarget;
    CodaFid saveFid;
    int     saveFlags = 0;
    int     opcode;
    int     size;
    int     openfd;

    for (;;) {
	openfd = -1;
	/* Wait for new request. */
	AwaitRequest();

	/* Sanity check new request. */
	if (idle) CHOKE("Worker: signalled but not dispatched!");
	if (!msg) CHOKE("Worker: no message!");

	union inputArgs *in = (union inputArgs *)msg->msg_buf;
	union outputArgs *out = (union outputArgs *)msg->msg_buf;
	
        /* we reinitialize these on every loop */
        size = sizeof(struct coda_out_hdr);
	interrupted = 0;
	returned = 0;
	StoreFid = NullFid;

	/* Fill in the user-specific context. */
	u.Init();
	u.u_priority = FSDB->StdPri();
	u.u_flags = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);

	/* GOTTA BE ME */
	u.u_uid  = (in)->ih.uid;
	u.u_pid  = (in)->ih.pid;
	u.u_pgid = (in)->ih.pgid;

        opcode = in->ih.opcode;
	/* This switch corresponds to the kernel trap handler. */
	switch (opcode) {
	    case CODA_ACCESS:
		{
		LOG(100, ("CODA_ACCESS: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_access.Fid, 0);
		access(&vtarget, in->coda_access.flags);
		break;
		}

	    case CODA_CLOSE:
		{
		LOG(100, ("CODA_CLOSE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_close.Fid, 0);
		close(&vtarget, in->coda_close.flags);
		break;
		}

	  case CODA_CREATE:
		{
		LOG(100, ("CODA_CREATE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid,u.u_pgid));
		MAKE_CNODE(vparent, in->coda_create.Fid, 0);
		create(&vparent, (char *)in + (int)in->coda_create.name,
		       &in->coda_create.attr, in->coda_create.excl,
		       in->coda_create.mode, &vtarget);

		if (u.u_error == 0) {
		    out->coda_create.Fid = *VenusToKernelFid(&vtarget.c_fid);
		    out->coda_create.attr = in->coda_create.attr;
		    size = (int)sizeof (struct coda_create_out);
		}
		break;
		}

	    case CODA_FSYNC:	
		{
		LOG(100, ("CODA_FSYNC: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_fsync.Fid, 0);
		//fsync(&vtarget);
		break;
		}

	    case CODA_GETATTR:
		{
		LOG(100, ("CODA_GETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_getattr.Fid, 0);
		va_init(&out->coda_getattr.attr);
		getattr(&vtarget, &out->coda_getattr.attr);
                size = sizeof(struct coda_getattr_out);
		break;
		}

	    case CODA_IOCTL:
		{
		char outbuf[VC_MAXDATASIZE];
		struct ViceIoctl data;
                int cmd = in->coda_ioctl.cmd;
		unsigned char type = _IOC_TYPE(cmd);
		unsigned char nr = _IOC_NR(cmd);

		data.in = (char *)in + (intptr_t)in->coda_ioctl.data;
		data.in_size = 0;
		data.out = outbuf;	/* Can't risk overcopying. Sigh. -dcs */
		data.out_size =	0;

		LOG(100, ("CODA_IOCTL: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		if (type != 'V') {
		    u.u_error = EOPNOTSUPP;
		    out->coda_ioctl.data =
			(char *)sizeof(struct coda_ioctl_out); 
                    out->coda_ioctl.len = 0;		
		    size = sizeof(struct coda_ioctl_out);
		    break;
		}

		if (nr == _VIOC_UNLOADKERNEL) {
                    out->oh.result = 0;
		    out->coda_ioctl.data =
			(char *)sizeof(struct coda_ioctl_out); 
                    out->coda_ioctl.len = 0;		
                    /* we have to Resign here because we will exit before
                     * leaving the switch */
                    Resign(msg, sizeof(struct coda_ioctl_out));

                    LOG(0, ("TERM: Venus exiting\n"));
                    RecovFlush(1);
                    RecovTerminate();
                    VFSUnmount();
                    fflush(logFile);
                    fflush(stderr);
                    
                    exit(0);	
		}
			
		if (nr == _VIOCPREFETCH)
		    worker::nprefetchers++;

		MAKE_CNODE(vtarget, in->coda_ioctl.Fid, 0);
		data.in_size = in->coda_ioctl.len;

		ioctl(&vtarget, nr, &data, in->coda_ioctl.rwflag);

		out->coda_ioctl.len = data.out_size;
		out->coda_ioctl.data = (char *)(sizeof (struct coda_ioctl_out));
		memcpy((char *)out + (intptr_t)out->coda_ioctl.data, data.out, data.out_size);
		if (nr == _VIOCPREFETCH)
		    worker::nprefetchers--;

		size = sizeof (struct coda_ioctl_out) + data.out_size;
		break;
		}

	    case CODA_LINK:
		{
		LOG(100, ("CODA_LINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
                /* target = linked object, parent = destination directory */
		MAKE_CNODE(vtarget, in->coda_link.sourceFid, 0);
		MAKE_CNODE(vparent, in->coda_link.destFid, 0);
		link(&vtarget, &vparent, (char *)in + (int)in->coda_link.tname);
		break;
		}

	    case CODA_LOOKUP:
		{
		LOG(100, ("CODA_LOOKUP: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		MAKE_CNODE(vparent, in->coda_lookup.Fid, 0);
		lookup(&vparent, (char *)in + (int)in->coda_lookup.name, &vtarget, (int)in->coda_lookup.flags);

		if (u.u_error == 0) {
		    out->coda_lookup.Fid = *VenusToKernelFid(&vtarget.c_fid);
		    out->coda_lookup.vtype = vtarget.c_type;
		    if (vtarget.c_type == C_VLNK && vtarget.c_flags & C_INCON)
			    out->coda_lookup.vtype |= CODA_NOCACHE;
		    size = sizeof (struct coda_lookup_out);
		}
		break;
		}

	    case CODA_MKDIR:
		{
		LOG(100, ("CODA_MKDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vparent, in->coda_mkdir.Fid, 0);
		mkdir(&vparent, (char *)in + (int)in->coda_mkdir.name,
                      &in->coda_mkdir.attr, &vtarget);

		if (u.u_error == 0) {
		    out->coda_mkdir.Fid = *VenusToKernelFid(&vtarget.c_fid);
		    out->coda_mkdir.attr = in->coda_mkdir.attr;
		    size = sizeof (struct coda_mkdir_out);
		}
		break;
		}

	    case CODA_OPEN:
		{
		LOG(100, ("CODA_OPEN: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid)); 
                /* Remember some info for dealing with interrupted open calls */
                saveFid = in->coda_open.Fid;
                saveFlags = in->coda_open.flags;
		
		MAKE_CNODE(vtarget, in->coda_open.Fid, 0);
		open(&vtarget, in->coda_open.flags);
		
		if (u.u_error == 0) {
		    struct stat tstat;
		    MarinerReport(&vtarget.c_fid, u.u_uid);
		    vtarget.c_cf->Stat(&tstat);
		    out->coda_open.dev = tstat.st_dev;
		    out->coda_open.inode = tstat.st_ino;
		    size = sizeof (struct coda_open_out);
		}
		break;
		}

	    case CODA_OPEN_BY_FD:
		{
                LOG(100, ("CODA_OPEN_BY_FD: u.u_pid = %d u.u_pgid = %d\n",
                          u.u_pid, u.u_pgid));
                /* Remember some info for dealing with interrupted open calls */
                saveFid = in->coda_open_by_fd.Fid;
                saveFlags = in->coda_open_by_fd.flags;
		
		MAKE_CNODE(vtarget, in->coda_open_by_fd.Fid, 0);
		open(&vtarget, in->coda_open_by_fd.flags);
		
		if (u.u_error == 0) {
		    int flags = (in->coda_open_by_fd.flags & C_O_WRITE) ?
			O_RDWR : O_RDONLY;

		    MarinerReport(&vtarget.c_fid, u.u_uid);
		    openfd = vtarget.c_cf->Open(flags);
		    out->coda_open_by_fd.fd = openfd;
                    LOG(10, ("CODA_OPEN_BY_FD: fd = %d\n", openfd));
		    size = sizeof (struct coda_open_by_fd_out);
		}
		break;
		}

	    case CODA_OPEN_BY_PATH:
		{
		LOG(100, ("CODA_OPEN_BY_PATH: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid)); 
#if defined(DJGPP) || defined(__CYGWIN32__)
		char *slash;
#endif
                /* Remember some info for dealing with interrupted open calls */
		saveFid = in->coda_open_by_path.Fid;
		saveFlags = in->coda_open_by_path.flags;
		
		MAKE_CNODE(vtarget, in->coda_open_by_path.Fid, 0);

		open(&vtarget, in->coda_open_by_path.flags);
		
		if (u.u_error == 0) {
		    MarinerReport(&vtarget.c_fid, u.u_uid);

                    char *begin = (char *)(&out->coda_open_by_path.path + 1);
                    out->coda_open_by_path.path = begin - (char *)out;
                    sprintf(begin, "%s%s/%s", CachePrefix, CacheDir, 
                            vtarget.c_cf->Name());
#if defined(DJGPP) || defined(__CYGWIN32__)
                    slash = begin;
                    for (slash = begin ; *slash ; slash++ ) {
                        if ( *slash == '/' ) 
                            *slash='\\';
                    }
#endif
                    size = sizeof (struct coda_open_by_path_out) + 
                        strlen(begin) + 1;
                    LOG(100, ("CODA_OPEN_BY_PATH: returning '%s', size=%d\n",
			      begin, size));
		}
		break;
		}

	    case CODA_READLINK: 
		{
		LOG(100, ("CODA_READLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_readlink.Fid, 0);
		struct coda_string string;
		string.cs_buf = (char *)out + sizeof(struct coda_readlink_out);
		string.cs_maxlen = CODA_MAXPATHLEN;
		readlink(&vtarget, &string);

		if (u.u_error == 0)
		    MarinerReport(&(vtarget.c_fid), u.u_uid);

		out->coda_readlink.count = string.cs_len;
		/* readlink.data is an offset, with the wrong type .. sorry */
		out->coda_readlink.data = (char *)(sizeof (struct coda_readlink_out));

		size = sizeof(struct coda_readlink_out) + out->coda_readlink.count;
		break;
		}

	    case CODA_REMOVE:
		{
		LOG(100, ("CODA_REMOVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vparent, in->coda_remove.Fid, 0);
		remove(&vparent, (char *)in + (int)in->coda_remove.name);
		break;
		}

	    case CODA_RENAME:
		{
		LOG(100, ("CODA_RENAME: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
                /* parent = source directory, target = destination directory */
		MAKE_CNODE(vparent, in->coda_rename.sourceFid, 0);
		MAKE_CNODE(vtarget, in->coda_rename.destFid, 0);
		rename(&vparent, (char *)in + (int)in->coda_rename.srcname,
		       &vtarget, (char *)in + (int)in->coda_rename.destname);
		break;
		}

	    case CODA_RMDIR:
		{
		LOG(100, ("CODA_RMDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vparent, in->coda_rmdir.Fid, 0);
		rmdir(&vparent, (char *)in + (int)in->coda_rmdir.name);
		break;
		}

	    case CODA_ROOT:
		{
		root(&vtarget);

		if (u.u_error == 0) {
		    out->coda_root.Fid = *VenusToKernelFid(&vtarget.c_fid);
		    size = sizeof (struct coda_root_out);
		}
		break;
		}

	    case CODA_SETATTR:
		{
		LOG(100, ("CODA_SETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		MAKE_CNODE(vtarget, in->coda_setattr.Fid, 0);
		setattr(&vtarget, &in->coda_setattr.attr);
		break;
		}

	    case CODA_SYMLINK:
		{
		LOG(100, ("CODA_SYMLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

		MAKE_CNODE(vtarget, in->coda_symlink.Fid, 0);
                symlink(&vtarget, (char *)in + (int)in->coda_symlink.srcname, &in->coda_symlink.attr, (char *)in + (int)in->coda_symlink.tname);
		break;
		}

	    case CODA_VGET:
		{
		LOG(100, ("CODA_VGET: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
		struct cfid fid;
		fid.cfid_len = (unsigned short)sizeof(VenusFid);
		KernelToVenusFid(&fid.cfid_fid, &in->coda_vget.Fid);
		vget(&vtarget, &fid, RC_DATA);

		if (u.u_error == 0) {
		    out->coda_vget.Fid = *VenusToKernelFid(&vtarget.c_fid);
		    out->coda_vget.vtype = vtarget.c_type;
		    if (vtarget.c_type == C_VLNK && vtarget.c_flags & C_INCON)
                        out->coda_vget.vtype |= CODA_NOCACHE;
		    size = sizeof (struct coda_vget_out);
		}
		break;
                }

	    case CODA_STATFS:
		{
		statfs(&(out->coda_statfs.stat));
		if (u.u_error == 0)
		    size = sizeof (struct coda_statfs_out);
		break;
		}

	    default:	 /* Toned this down a bit, used to be a choke -- DCS */
		{	/* But make sure someone sees it! */
		eprint("worker::main Got a bogus opcode %d", in->ih.opcode);
		dprint("worker::main Got a bogus opcode %d\n", in->ih.opcode);
		MarinerLog("worker::main Got a bogus opcode %d\n", in->ih.opcode);
		u.u_error = EOPNOTSUPP;
                break;
		}
	}

        out->oh.result = u.u_error;
        Resign(msg, size);

        if (opcode == CODA_OPEN_BY_FD && openfd != -1)
	    vtarget.c_cf->Close(openfd);

        if (opcode == CODA_OPEN ||
            opcode == CODA_OPEN_BY_FD ||
            opcode == CODA_OPEN_BY_PATH)
        {
            /* If open was aborted by user we must abort our OPEN too
             *  (if it was successful). */
            if (interrupted && u.u_error == 0) {
		VenusFid fid;
		KernelToVenusFid(&fid, &saveFid);
                eprint("worker::main: aborting open (%s)", &fid);

                /* NOTE: This may be bogus. It will definately cause a
                 * "message write error" since the uniquifier is bogus. No
                 * harm done, I guess. But why not just call close directly?
                 * -- DCS */
                /* Fashion a CLOSE message. */
                msgent *fm = (msgent *)worker::FreeMsgs.get();
                if (!fm) fm = new msgent;
                union inputArgs *dog = (union inputArgs *)fm->msg_buf;

                dog->coda_close.ih.unique = (u_long)-1;
                dog->coda_close.ih.opcode = CODA_CLOSE;
                dog->coda_close.Fid = saveFid;
                dog->coda_close.flags = saveFlags;

                /* Dispatch it. */
                DispatchWorker(fm);
            }
        }
    }
}


worker_iterator::worker_iterator() : vproc_iterator(VPT_Worker) {
}


worker *worker_iterator::operator()() {
    return((worker *)vproc_iterator::operator()());
}

