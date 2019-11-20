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

#ifndef __CYGWIN32__
#include <sys/syscall.h>
#include <sys/mount.h>
#endif

#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __FreeBSD__
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

#ifdef __CYGWIN__
#include <windows.h>
#endif

#ifdef __cplusplus
}
#endif

/* Darwin uses the same venus-kernel interface as BSD */
#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    (defined(__APPLE__) && defined(__MACH__))
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
#include "9pfs.h"

#include "nt_util.h"
#include "getpeereid.h"

extern int venus_relay_addr;

/* static class members */
int worker::muxfd = -1;
int worker::nworkers;
int worker::nprefetchers;
int worker::kernel_version      = 0;
const char *worker::CacheDir    = "";
const char *worker::CachePrefix = "";
const char *worker::kernDevice  = "";
time_t worker::lastresign;
olist worker::FreeMsgs;
olist worker::QueuedMsgs;
olist worker::ActiveMsgs;

int msgent::allocs   = 0;
int msgent::deallocs = 0;

const int WorkerStackSize = 131072;

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
msgent *FindMsg(olist &ol, u_long seq)
{
    msg_iterator next(ol);
    msgent *m;
    while ((m = next())) {
        union inputArgs *inp = (union inputArgs *)m->msg_buf;
        if (inp->ih.unique == seq)
            return (m);
    }

    return (0);
}

msgent::msgent()
{
    allocs++;
}

msgent::~msgent()
{
    deallocs++;
}

msg_iterator::msg_iterator(olist &ol)
    : olist_iterator(ol)
{
}

msgent *msg_iterator::operator()()
{
    return ((msgent *)olist_iterator::operator()());
}

msgent *AllocMsgent(void)
{
    msgent *m = (msgent *)worker::FreeMsgs.get();
    if (!m)
        m = new msgent;
    m->return_fd = -1;
    return m;
}

/* read a msg from the given socket */
void ReadUpcallMsg(int fd, size_t size)
{
    msgent *m = AllocMsgent();
    ssize_t len;
    uid_t euid = 0;
    gid_t egid;
    int rc;

    CODA_ASSERT(size <= VC_MAXMSGSIZE);
    len = read(fd, m->msg_buf, size);

    if (len < (ssize_t)sizeof(struct coda_in_hdr)) {
        eprint("Failed to read upcall");
        worker::FreeMsgs.append(m);
        return;
    }

    if (fd != worker::muxfd) {
        rc = getpeereid(fd, &euid, &egid);
        if (rc) {
            eprint("Unable to check peer credentials");
            /* disconnect peer? */
        }

        if (euid != 0) { /* root can already do whatever it wants */
            union inputArgs *inp = (union inputArgs *)m->msg_buf;
            inp->ih.uid          = euid;
        }
    }

    m->return_fd = fd;
    DispatchWorker(m);
}

ssize_t WriteDowncallMsg(int fd, const char *buf, size_t size)
{
    if (fd == -1)
        return size;

    CODA_ASSERT(size <= VC_MAXMSGSIZE);
    if (fd != worker::muxfd) {
        char hdr[11];
        int len;
        len = sprintf(hdr, "down: %lu\n", size);
        len -= write(fd, hdr, len);
        CODA_ASSERT(len == 0);
    }
#ifdef __CYGWIN32__
    else
        return nt_msg_write(buf, size);
#endif
    return write(fd, buf, size);
}

ssize_t MsgWrite(const char *buf, size_t size)
{
    return WriteDowncallMsg(worker::muxfd, buf, size);
}

void VFSMount()
{
    /* Linux (and NetBSD) Coda filesystems are mounted through forking since
     * they need venus.
     */

#ifdef __BSD44__ /* BSD specific preamble */
    /* Silently unmount the root node in case an earlier venus exited without
     * successfully unmounting. */
    unmount(vproc::venusRoot, 0);
    switch (errno) {
    case 0:
        eprint("unmount(%s) succeeded, continuing", vproc::venusRoot);
        break;

    case EINVAL:
        /* not mounted */
        break;

    case EBUSY:
    default:
        eprint("unmount(%s) failed (%d), exiting", vproc::venusRoot, errno);
        exit(EXIT_FAILURE);
    }

    /* Deduce rootnodeid. */
    struct stat tstat;
    if (::stat(vproc::venusRoot, &tstat) < 0) {
        eprint("stat(%s) failed (%d), exiting", vproc::venusRoot, errno);
        exit(EXIT_FAILURE);
    }
    rootnodeid = tstat.st_ino;
#endif /* __BSD44__ */

#ifdef __linux__ /* Linux specific preamble */
    int islinux20 = 0;
    int mounted   = 0;
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
            if (!ent)
                break;

            if ((STREQ(ent->mnt_fsname, "Coda") ||
                 STREQ(ent->mnt_fsname, "coda")) &&
                STREQ(ent->mnt_dir, vproc::venusRoot)) {
                mounted = 1;
                break;
            }
        }
        endmntent(fd);

        if (mounted) {
            eprint("%s already mounted", vproc::venusRoot);
            if (GetVenusConf().get_bool_value("allow-reattach")) {
                kill(getpid(), SIGUSR1);
                return;
            }
            exit(EXIT_FAILURE);
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
        error = -1;
        /* Issue the VFS mount request. */

#ifdef HAVE_NMOUNT
        { /* FreeBSD-6.x introduced a new mount syscall */
            struct iovec md[6];
            md[0].iov_base = (char *)"fstype";
            md[0].iov_len  = sizeof("fstype");
            md[1].iov_base = (char *)"coda";
            md[1].iov_len  = sizeof("coda");
            md[2].iov_base = (char *)"fspath";
            md[2].iov_len  = sizeof("fspath");
            md[3].iov_base = (char *)vproc::venusRoot;
            md[3].iov_len  = strlen((char *)vproc::venusRoot) + 1;
            md[4].iov_base = (char *)"from";
            md[4].iov_len  = sizeof("from");
            md[5].iov_base = (char *)worker::kernDevice;
            md[5].iov_len  = strlen((char *)worker::kernDevice) + 1;
            error          = nmount(md, 6, 0);
        }
#endif

#if defined(__NetBSD__) && __NetBSD_Version__ >= 499002400 /* 4.99.24 */
        if (error < 0)
            error = mount("coda", vproc::venusRoot, 0,
                          (void *)worker::kernDevice, 256);
        if (error < 0)
            error = mount("cfs", vproc::venusRoot, 0,
                          (void *)worker::kernDevice, 256);
#else
        if (error < 0)
            error = mount("coda", (char *)vproc::venusRoot, 0,
                          (char *)worker::kernDevice);
        if (error < 0)
            error = mount("cfs", (char *)vproc::venusRoot, 0,
                          (char *)worker::kernDevice);
#endif

#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
#define MOUNT_CFS 19
        if (error < 0)
            error = mount(MOUNT_CFS, vproc::venusRoot, 0, worker::kernDevice);
#endif
#endif /* __BSD44__ */

#ifdef __linux__ /* Linux specific mount */
        struct coda_mount_data mountdata;
        mountdata.version = CODA_MOUNT_VERSION;
        mountdata.fd      = worker::muxfd;

        error =
            mount("coda", vproc::venusRoot, "coda",
                  MS_MGC_VAL | MS_NOATIME | MS_NODEV | MS_NOSUID,
                  islinux20 ? (void *)&worker::kernDevice : (void *)&mountdata);

        if (!error) {
            FILE *fd = setmntent("/etc/mtab", "a");
            struct mntent ent;
            if (fd) {
                ent.mnt_fsname = (char *)"coda";
                ent.mnt_dir    = (char *)vproc::venusRoot;
                ent.mnt_type   = (char *)"coda";
                ent.mnt_opts   = (char *)"rw,noatime,nosuid,nodev";
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
            eprint("%s now mounted.", vproc::venusRoot);
            kill(parent, SIGUSR1);
        }

    child_done:
        WorkerCloseMuxfd();
        exit(error < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    /* we just wait around to reap the first child */
    pid_t pid;
    do {
        pid = waitpid(child, NULL, 0);
    } while (pid == -1 && errno == EINTR);
#endif /* __BSD44__ || __linux__ */

#ifdef sun
    {
        int error;
        /* Do a umount just in case it is mounted. */
        error = umount(vproc::venusRoot);
        if (error) {
            if (errno != EINVAL) {
                eprint("unmount(%s) failed (%d), exiting", vproc::venusRoot,
                       errno);
                exit(EXIT_FAILURE);
            }
        } else
            eprint("unmount(%s) succeeded, continuing", vproc::venusRoot);
    }
    /* New mount */
    CODA_ASSERT(
        !mount(worker::kernDevice, vproc::venusRoot, MS_DATA, "coda", NULL, 0));
    /* Update the /etc mount table entry */
    {
        int lfd, mfd;
        int lck;
        FILE *mnttab;
        char tm[25];
        struct mnttab mt;

        lfd = open("/etc/.mnttab.lock", O_WRONLY | O_CREAT, 0600);
        if (lfd >= 0) {
            lck = lockf(lfd, F_LOCK, 0);
            if (lck == 0) {
                mnttab = fopen(MNTTAB, "a+");
                if (mnttab != NULL) {
                    mt.mnt_special = "CODA";
                    mt.mnt_mountp  = (char *)vproc::venusRoot;
                    mt.mnt_fstype  = "CODA";
                    mt.mnt_mntopts = "rw";
                    mt.mnt_time    = tm;
                    sprintf(tm, "%d", time(0));

                    putmntent(mnttab, &mt);

                    fclose(mnttab);
                } else
                    eprint("Could not update /etc/mnttab.");
                (void)lockf(lfd, F_ULOCK, 0);
            } else
                eprint("Could not update /etc/mnttab.");
            close(lfd);
        } else
            eprint("Could not update /etc/mnttab.");
    }
#endif

#ifdef __CYGWIN32__
    /* Mount by starting another thread. */
    eprint("Mounting on %s", vproc::venusRoot);
    nt_mount(vproc::venusRoot);
#endif

    Mounted = 1;
}

void VFSUnmount()
{
    /* Purge the kernel cache so that all cnodes are (hopefully) released. */
    k_Purge();

    WorkerCloseMuxfd();

    if (!Mounted)
        return;

#ifdef __BSD44__
        /* For now we can not unmount, because an coda_root() upcall could
       nail us. */
#ifndef __BSD44__
    /* Issue the VFS unmount request. */
    if (unmount(vproc::venusRoot, 0) < 0) {
        eprint("vfsunmount(%s) failed (%d)", vproc::venusRoot, errno);
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

        res = umount(vproc::venusRoot);
        if (res)
            eprint("Unmount failed.");
        /* Remove CODA entry from /etc/mnttab */

        lfd = open("/etc/.mnttab.lock", O_WRONLY | O_CREAT, 0600);
        if (lfd >= 0) {
            lck = lockf(lfd, F_LOCK, 0);
            if (lck == 0) {
                mnttab = fopen(MNTTAB, "r+");
                if (mnttab != NULL) {
                    newtab = fopen("/etc/newmnttab", "w+");
                    if (newtab != NULL) {
                        while (fgets(line, 1024, mnttab)) {
                            if (strncmp("CODA", line, 4) != 0) {
                                fprintf(newtab, "%s", line);
                            }
                        }
                        fclose(newtab);
                        fclose(mnttab);
                        unlink(MNTTAB);
                        rename("/etc/newmnttab", MNTTAB);
                    } else {
                        eprint("Could not remove Coda from /etc/mnttab");
                        fclose(mnttab);
                    }
                } else
                    eprint("Could not remove CODA from /etc/mnttab.");
                (void)lockf(lfd, F_ULOCK, 0);
            } else
                eprint("Could not remove CODA from /etc/mnttab.");
            close(lfd);
        } else
            eprint("Could not remove CODA from /etc/mnttab.");
        sync();
    }
#endif

#ifdef __CYGWIN32__
    eprint("Unmounting %s", vproc::venusRoot);
    nt_umount(vproc::venusRoot);
#endif
}

int k_Purge()
{
    ssize_t size;

    if (!worker::isReady())
        return (1);

    LOG(1, ("k_Purge: Flush\n"));

    /* Construct a purge message. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    msg.oh.opcode = CODA_FLUSH;
    msg.oh.unique = 0;
    size          = sizeof(struct coda_out_hdr);

    /* Send the message. */
    if (MsgWrite((char *)&msg, size) != size)
        CHOKE("k_Purge: Flush, message write returns %d", errno);

    LOG(1, ("k_Purge: Flush, returns 0\n"));
    VFSStats.VFSOps[CODA_FLUSH].success++;

    return (1);
}

int k_Purge(VenusFid *fid, int severely)
{
    ssize_t size;

    if (!worker::isReady())
        return (1);

    LOG(100, ("k_Purge: fid = (%s), severely = %d\n", FID_(fid), severely));

    int retcode = 0;

    /* Setup message. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    if (severely) {
        msg.coda_purgefid.oh.opcode = CODA_PURGEFID;
        msg.coda_purgefid.oh.unique = 0;
        msg.coda_purgefid.Fid       = *VenusToKernelFid(fid);
        size                        = sizeof(msg.coda_purgefid);
    } else if (ISDIR(*fid)) {
        msg.coda_zapdir.oh.opcode = CODA_ZAPDIR;
        msg.coda_zapdir.oh.unique = 0;
        msg.coda_zapdir.Fid       = *VenusToKernelFid(fid);
        size                      = sizeof(msg.coda_zapdir);
    } else {
        msg.coda_zapfile.oh.opcode = CODA_ZAPFILE;
        msg.coda_zapfile.oh.unique = 0;
        msg.coda_zapfile.Fid       = *VenusToKernelFid(fid);
        size                       = sizeof(msg.coda_zapfile);
    }

    /* Send the message. */
    if (MsgWrite((char *)&msg, size) != size) {
        retcode = errno;
        LOG(0,
            ("k_Purge: %s, message write fails: errno %d\n",
             msg.oh.opcode == CODA_PURGEFID ?
                 "CODA_PURGEFID" :
                 msg.oh.opcode == CODA_ZAPFILE ? "CODA_ZAPFILE" : "CODA_ZAPDIR",
             retcode));
    }

    LOG(100,
        ("k_Purge: %s, returns %d\n",
         msg.oh.opcode == CODA_PURGEFID ?
             "CODA_PURGEFID" :
             msg.oh.opcode == CODA_ZAPFILE ? "CODA_ZAPFILE" : "CODA_ZAPDIR",
         retcode));
    if (retcode == 0) {
        VFSStats.VFSOps[msg.oh.opcode].success++;
    } else {
        VFSStats.VFSOps[msg.oh.opcode].failure++;
    }

    return (retcode == 0);
}

int k_Purge(uid_t uid)
{
    ssize_t size;

    if (!worker::isReady())
        return (1);

    LOG(1, ("k_Purge: uid = %d\n", uid));

    /* Message prefix. */
    union outputArgs msg;
    memset(&msg, 0, sizeof(msg));

    msg.coda_purgeuser.oh.unique = 0;
    msg.coda_purgeuser.oh.opcode = CODA_PURGEUSER;

    /* Message data. */
    msg.coda_purgeuser.uid = uid;
    size                   = sizeof(msg.coda_purgeuser);

    /* Send the message. */
    if (MsgWrite((char *)&msg, size) != size)
        CHOKE("k_Purge: PurgeUser, message write");

    LOG(1, ("k_Purge: PurgeUser, returns 0\n"));
    VFSStats.VFSOps[CODA_PURGEUSER].success++;

    return (1);
}

int k_Replace(VenusFid *fid_1, VenusFid *fid_2)
{
    static bool plan9server_enabled = GetVenusConf().get_bool_value("9pfs");
    if (!fid_1 || !fid_2)
        CHOKE("k_Replace: nil fids");

    LOG(0, ("k_Replace: VenusFid (%s) with VenusFid (%s) in mini-cache\n",
            FID_(fid_1), FID_(fid_2)));

    /* replace in 9pfs fidmaps */
    if (plan9server_enabled) {
        mariner_iterator next;
        mariner *m;
        while ((m = next()))
            if (m->p9srv != NULL)
                m->p9srv->fidmap_replace_cfid(fid_1, fid_2);
    }

    if (!worker::isReady())
        return (1);

    /* Message prefix. */
    struct coda_replace_out msg;
    msg.oh.unique = 0;
    msg.oh.opcode = CODA_REPLACE;

    msg.OldFid = *VenusToKernelFid(fid_1);
    msg.NewFid = *VenusToKernelFid(fid_2);

    /* Send the message. */
    ssize_t size = sizeof(struct coda_replace_out);
    if (MsgWrite((char *)&msg, size) != size)
        CHOKE("k_Replace: message write");

    LOG(0, ("k_Replace: returns 0\n"));
    VFSStats.VFSOps[CODA_REPLACE].success++;

    return (1);
}

/* -------------------------------------------------- */
void WorkerInit()
{
    worker::kernDevice = GetVenusConf().get_value("kerneldevice");
    worker::CacheDir   = GetVenusConf().get_value("cachedir");

    if (vproc::MaxPrefetchers > vproc::MaxWorkers) { /* whoa */
        eprint("WorkerInit: MaxPrefetchers %d, MaxWorkers only %d!",
               vproc::MaxPrefetchers, vproc::MaxWorkers);
        exit(EXIT_FAILURE);
    }

#ifdef __CYGWIN32__
    int sd[2];
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sd)) {
        eprint("WorkerInit: socketpair() returns %d", errno);
        exit(EXIT_FAILURE);
    }
    worker::muxfd = sd[0];
    if (!nt_initialize_ipc(sd[1])) {
        dprint("WorkerInit: nt_initialize_ipc failed.\n");
        exit(EXIT_FAILURE);
    }
    dprint("WorkerInit: muxfd = %d\n", worker::muxfd);
#else
    /* Open the communications channel. */
    worker::muxfd = ::open(worker::kernDevice, O_RDWR, 0);
    if (worker::muxfd == -1) {
        eprint("WorkerInit: open %s failed", worker::kernDevice);
        exit(EXIT_FAILURE);
    }
#endif

#if defined(__BSD44__) || defined(__linux__)
    if (::ioctl(worker::muxfd, CIOC_KERNEL_VERSION, &worker::kernel_version) >=
        0) {
        switch (worker::kernel_version) {
#ifdef NO_64BIT_TIMESPEC
        case 5: /* VASTRO */
            eprint("Kernel module is VASTRO-enabled");
        case 4: /* 64-bit time_t in timespec */
            /* using 64-bit time_t in timespec. But we're using long so this
             * it fine on a 64-bit system, but breaks on a 32-bit */
            if (sizeof(long) != sizeof(uint64_t)) {
                eprint("Kernel module is using 64-bit time_t in timespec");
                exit(EXIT_FAILURE);
            }
        case 3:
            break;
#else
        case 5: /* VASTRO */
            eprint("Kernel module is VASTRO-enabled");
            break;
        case 4: /* 64-bit time_t in timespec */
            break;
        case 3:
            /* using long for time_t in timespec. Our 64-bit time_t will be
             * fine on a 64-bit system, but breaks on a 32-bit */
            if (sizeof(long) == sizeof(uint64_t))
                break;

            eprint("Kernel module does not yet use 64-bit time_t in timespec");
#endif
        case 2: /* 1 & 2 are upwards compatible, but 3 introduced the realms */
        case 1:
        default:
            eprint("Version Skew with kernel, get a newer kernel");
            eprint("Kernel module version is %d\n.", worker::kernel_version);
            exit(EXIT_FAILURE);
        }
    } else {
        eprint("Kernel version ioctl failed.");
    }
#endif

    /* Flush kernel cache(s). */
    k_Purge();

    worker::nworkers     = 0;
    worker::nprefetchers = 0;
    worker::lastresign   = Vtime();

    /* Allows the MessageMux to distribute incoming messages to us. */
    MUX_add_callback(worker::muxfd, WorkerMux, NULL);
}

int WorkerCloseMuxfd(void)
{
    int ret = 0;

    if (worker::muxfd != -1)
        ret = close(worker::muxfd);

    /* just in case we still have a parent process waiting for us we don't want
     * to lock up the boot sequence... */
    if (parent_fd != -1)
        close(parent_fd);

    worker::muxfd = parent_fd = -1;

    return ret;
}

worker *FindWorker(u_long seq)
{
    worker_iterator next;
    worker *w;
    while ((w = next()))
        if (w->msg && ((union inputArgs *)w->msg)->ih.unique == seq)
            return (w);

    return (0);
}

worker *GetIdleWorker()
{
    worker_iterator next;
    worker *w;
    while ((w = next()))
        if (w->idle)
            return (w);

    /* No idle workers; can we create a new one? */
    if (worker::nworkers < vproc::MaxWorkers) {
        return (new worker);
    }

    return (0);
}

int IsAPrefetch(msgent *m)
{
    /* determines if a message is a prefetch request */
    union inputArgs *in = (union inputArgs *)m->msg_buf;

    if (in->ih.opcode != CODA_IOCTL)
        return (0);

    return (_IOC_NR(in->coda_ioctl.cmd) == _VIOCPREFETCH);
}

void DispatchWorker(msgent *m)
{
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
        } else {
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
        if (worker::nprefetchers >= vproc::MaxPrefetchers) {
            LOG(1,
                ("DispatchWorker: queuing prefetch (%d workers, %d prefetching)\n",
                 worker::nworkers, worker::nprefetchers));
            worker::QueuedMsgs.append(m);
            return;
        }
    }

    /* Try to find an idle worker to handle this message. */
    worker *w = GetIdleWorker();
    if (w) {
        worker::ActiveMsgs.append(m);
        w->msg    = m;
        w->opcode = (int)in->ih.opcode;
        w->idle   = 0;
        VprocSignal((char *)w);
        return;
    }

    /* No one is able to handle this message now; queue it up for the next free worker. */
    LOG(0, ("DispatchWorker: out of workers (max %d), queueing message\n",
            vproc::MaxWorkers));
    worker::QueuedMsgs.append(m);
}

void WorkerMux(int fd, void *udata)
{
    size_t size = VC_MAXMSGSIZE;

#ifdef __CYGWIN32__
    /* CYGWIN uses a stream oriented socket for kernel messages, there are no
     * message boundaries and we need to read the size of the message first. */
    DWORD msg_size;
    ssize_t len;
    len = read(fd, (char *)&msg_size, sizeof(msg_size));
    CODA_ASSERT(len == sizeof(msg_size));
    size = msg_size;
#endif

    ReadUpcallMsg(fd, size);
}

time_t GetWorkerIdleTime()
{
    /* Return 0 if any call is in progress. */
    worker_iterator next;
    worker *w;
    while ((w = next()))
        if (!w->idle)
            return (0);

    return (Vtime() - worker::lastresign);
}

void PrintWorkers()
{
    PrintWorkers(stdout);
}

void PrintWorkers(FILE *fp)
{
    fflush(fp);
    PrintWorkers(fileno(fp));
}

void PrintWorkers(int fd)
{
    fdprint(fd, "%#08x : %-16s : muxfd = %d, nworkers = %d\n", &worker::tbl,
            "Workers", worker::muxfd, worker::nworkers);

    worker_iterator next;
    worker *w;
    while ((w = next()))
        w->print(fd);
}

int GetKernelModuleVersion()
{
    return worker::kernel_version;
}

/* -------------------------------------------------- */

worker::worker()
    : vproc("Worker", NULL, VPT_Worker, WorkerStackSize)
{
    LOG(100, ("worker::worker(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    nworkers++; /* Ought to be a lock protecting this! -JJK */

    returned = 0;
    StoreFid = NullFid;
    msg      = 0;
    opcode   = 0;

    /* Poke main procedure. */
    start_thread();
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
worker::worker(worker &w)
    : vproc(*((vproc *)&w))
{
    abort();
}

int worker::operator=(worker &w)
{
    abort();
    return (0);
}

worker::~worker()
{
    LOG(100, ("worker::~worker: %-16s : lwpid = %d\n", name, lwpid));

    nworkers--; /* Ought to be a lock protecting this! -JJK */
}

/* Called by workers to get next service request. */
void worker::AwaitRequest()
{
    idle = 1;

    msgent *m = (msgent *)QueuedMsgs.get();

    /* limit the number of workers handling prefetches. see DispatchWorker. */
    if (m && IsAPrefetch(m) && worker::nprefetchers >= vproc::MaxPrefetchers) {
        /* re-queue and look for a non-prefetch message */
        LOG(1,
            ("worker::AwaitRequest: requeueing prefetch (%d workers, %d prefetching)\n",
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
        msg                  = m;
        union inputArgs *inp = (union inputArgs *)m->msg_buf;
        opcode               = inp->ih.opcode;
        idle                 = 0;
        return;
    }

    VprocWait((char *)this);
}

/* Called by workers after completing a service request. */
void worker::Resign(msgent *msg, int size)
{
    if (returned) {
        union outputArgs *outp = (union outputArgs *)msg->msg_buf;
        const char *opstr      = VenusOpStr(outp->oh.opcode);
        const char *retstr     = VenusRetStr(outp->oh.result);

#ifdef TIMING
        float elapsed;
        elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
        LOG(1, ("[Return Done] %s : returns %s, elapsed = %3.1f\n", opstr,
                retstr, elapsed));
#else /* !TIMING */
        LOG(1, ("[Return Done] %s : returns %s\n", opstr, retstr))
#endif
    } else {
        union outputArgs *outp = (union outputArgs *)msg->msg_buf;
        if (outp->oh.result == EINCONS) {
            /*	    outp->oh.result = ENOENT;*/
            CHOKE("worker::Resign: result == EINCONS");
        }

        Return(msg, size);
    }

    ActiveMsgs.remove(msg);
    FreeMsgs.append(msg);
    msg    = NULL;
    opcode = 0;

    lastresign = Vtime();
}

void worker::Return(msgent *msg, size_t size)
{
    size_t cc;

    if (returned)
        CHOKE("worker::Return: already returned!");

    union outputArgs *outp = (union outputArgs *)msg->msg_buf;
    const char *opstr      = VenusOpStr(outp->oh.opcode);
    const char *retstr     = VenusRetStr(outp->oh.result);

#ifdef TIMING
    float elapsed;
    if (u.u_tv2.tv_sec != 0) {
        elapsed = SubTimes(&(u.u_tv2), &(u.u_tv1));
        LOG(1, ("%s : returns %s, elapsed = %3.1f msec\n", opstr, retstr,
                elapsed));
    } else {
        LOG(1, ("%s : returns %s, elapsed = unknown msec (Returning early)\n",
                opstr, retstr));
    }
#else /* !TIMING */
    LOG(1, ("%s : returns %s\n", opstr, retstr));
#endif

    /* There is no reply to an interrupted operation. */
    if (interrupted)
        goto out;

    cc = WriteDowncallMsg(msg->return_fd, msg->msg_buf, size);
    if (cc != size) {
        int err                = errno;
        union outputArgs *outp = (union outputArgs *)msg->msg_buf;
        eprint(
            "worker::Return: message write error %d (op = %d, seq = %d), wrote %d of %d bytes\n",
            errno, outp->oh.opcode, outp->oh.unique, cc, size);

        /* Guard against a race in which the kernel is signalling us, but we
	 * entered this block before the signal reached us. In this case the
	 * error code from WriteDowncallMsg will be ESRCH. No other error code
	 * is legitimate. */
        if (err != ESRCH)
            CHOKE("worker::Return: errno (%d) from WriteDowncallMsg", err);
        interrupted = 1;
    }
out:
    returned = 1;
}

void worker::Return(int code)
{
    union outputArgs *outp = (union outputArgs *)msg->msg_buf;
    outp->oh.result        = code;
    Return(msg, (int)sizeof(struct coda_out_hdr));
}

inline void worker::op_coda_access(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_ACCESS: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_access.Fid, 0);
    access(&vtarget, in->coda_access.flags);
}

inline void worker::op_coda_close(union inputArgs *in, union outputArgs *out,
                                  int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_CLOSE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_close.Fid, 0);
    close(&vtarget, in->coda_close.flags);
}

inline void worker::op_coda_create(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    struct venus_cnode vtarget;
    struct venus_cnode vparent;

    LOG(100, ("CODA_CREATE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vparent, in->coda_create.Fid, 0);
    create(&vparent, (char *)in + (int)in->coda_create.name,
           &in->coda_create.attr, in->coda_create.excl, in->coda_create.mode,
           &vtarget);

    if (u.u_error == 0) {
        out->coda_create.Fid  = *VenusToKernelFid(&vtarget.c_fid);
        out->coda_create.attr = in->coda_create.attr;
        *msg_size             = (int)sizeof(struct coda_create_out);
    }
}

inline void worker::op_coda_fsync(union inputArgs *in, union outputArgs *out,
                                  int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_FSYNC: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_fsync.Fid, 0);
}

inline void worker::op_coda_getattr(union inputArgs *in, union outputArgs *out,
                                    int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_GETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_getattr.Fid, 0);
    va_init(&out->coda_getattr.attr);
    getattr(&vtarget, &out->coda_getattr.attr);
    *msg_size = sizeof(struct coda_getattr_out);
}

inline void worker::op_coda_ioctl(union inputArgs *in, union outputArgs *out,
                                  int *msg_size)
{
    struct venus_cnode vtarget;
    char outbuf[VC_MAXDATASIZE];
    struct ViceIoctl data;
    int cmd            = in->coda_ioctl.cmd;
    unsigned char type = _IOC_TYPE(cmd);
    unsigned long nr   = _IOC_NR(cmd);

    data.in       = (char *)in + (intptr_t)in->coda_ioctl.data;
    data.in_size  = 0;
    data.out      = outbuf; /* Can't risk overcopying. Sigh. -dcs */
    data.out_size = 0;

    LOG(100, ("CODA_IOCTL: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

    if (type != 'V') {
        u.u_error            = EOPNOTSUPP;
        out->coda_ioctl.data = (char *)sizeof(struct coda_ioctl_out);
        out->coda_ioctl.len  = 0;
        *msg_size            = sizeof(struct coda_ioctl_out);
        return;
    }

    if (nr == _VIOC_UNLOADKERNEL) {
        out->oh.result       = 0;
        out->coda_ioctl.data = (char *)sizeof(struct coda_ioctl_out);
        out->coda_ioctl.len  = 0;
        /* we have to Resign here because we will exit before
         * leaving the switch */
        Resign(msg, sizeof(struct coda_ioctl_out));

        LOG(0, ("TERM: Venus exiting\n"));
        RecovFlush(1);
        RecovTerminate();
        VFSUnmount();
        fflush(GetLogFile());
        fflush(stderr);

        exit(EXIT_SUCCESS);
    }

    if (nr == _VIOCPREFETCH)
        worker::nprefetchers++;

    MAKE_CNODE(vtarget, in->coda_ioctl.Fid, 0);
    data.in_size = in->coda_ioctl.len;

    ioctl(&vtarget, nr, &data, in->coda_ioctl.rwflag);

    out->coda_ioctl.len  = data.out_size;
    out->coda_ioctl.data = (char *)(sizeof(struct coda_ioctl_out));
    memcpy((char *)out + (intptr_t)out->coda_ioctl.data, data.out,
           data.out_size);

    if (nr == _VIOCPREFETCH)
        worker::nprefetchers--;

    *msg_size = sizeof(struct coda_ioctl_out) + data.out_size;
}

inline void worker::op_coda_link(union inputArgs *in, union outputArgs *out,
                                 int *msg_size)
{
    struct venus_cnode vparent;
    struct venus_cnode vtarget;

    LOG(100, ("CODA_LINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    /* target = linked object, parent = destination directory */
    MAKE_CNODE(vtarget, in->coda_link.sourceFid, 0);
    MAKE_CNODE(vparent, in->coda_link.destFid, 0);
    link(&vtarget, &vparent, (char *)in + (int)in->coda_link.tname);
}

inline void worker::op_coda_lookup(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    struct venus_cnode vtarget;
    struct venus_cnode vparent;

    LOG(100, ("CODA_LOOKUP: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

    MAKE_CNODE(vparent, in->coda_lookup.Fid, 0);
    lookup(&vparent, (char *)in + (int)in->coda_lookup.name, &vtarget,
           (int)in->coda_lookup.flags);

    if (u.u_error == 0) {
        out->coda_lookup.Fid   = *VenusToKernelFid(&vtarget.c_fid);
        out->coda_lookup.vtype = vtarget.c_type;
        if (vtarget.c_type == C_VLNK && vtarget.c_flags & C_FLAGS_INCON)
            out->coda_lookup.vtype |= CODA_NOCACHE;
        *msg_size = sizeof(struct coda_lookup_out);
    }
}

inline void worker::op_coda_mkdir(union inputArgs *in, union outputArgs *out,
                                  int *msg_size)
{
    struct venus_cnode vparent;
    struct venus_cnode vtarget;

    LOG(100, ("CODA_MKDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vparent, in->coda_mkdir.Fid, 0);
    mkdir(&vparent, (char *)in + (int)in->coda_mkdir.name, &in->coda_mkdir.attr,
          &vtarget);

    if (u.u_error == 0) {
        out->coda_mkdir.Fid  = *VenusToKernelFid(&vtarget.c_fid);
        out->coda_mkdir.attr = in->coda_mkdir.attr;
        *msg_size            = sizeof(struct coda_mkdir_out);
    }
}

inline void worker::op_coda_open(union inputArgs *in, union outputArgs *out,
                                 int *msg_size, CodaFid *saveFid,
                                 int *saveFlags)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_OPEN: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

    /* Remember some info for dealing with interrupted open calls */
    *saveFid   = in->coda_open.Fid;
    *saveFlags = in->coda_open.flags;

    MAKE_CNODE(vtarget, in->coda_open.Fid, 0);
    open(&vtarget, in->coda_open.flags);

    if (u.u_error == 0) {
        struct stat tstat;
        MarinerReport(&vtarget.c_fid, u.u_uid);
        vtarget.c_cf->Stat(&tstat);
        out->coda_open.dev   = tstat.st_dev;
        out->coda_open.inode = tstat.st_ino;
        *msg_size            = sizeof(struct coda_open_out);
    }
}

inline void worker::op_coda_open_by_fd(union inputArgs *in,
                                       union outputArgs *out, int *msg_size,
                                       int *openfd, CodaFid *saveFid,
                                       int *saveFlags,
                                       struct venus_cnode *vtarget)
{
    LOG(100,
        ("CODA_OPEN_BY_FD: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    /* Remember some info for dealing with interrupted open calls */
    *saveFid   = in->coda_open_by_fd.Fid;
    *saveFlags = in->coda_open_by_fd.flags;

    MAKE_CNODE(*vtarget, in->coda_open_by_fd.Fid, 0);
    open(vtarget, in->coda_open_by_fd.flags);

    if (u.u_error == 0) {
        int flags = (in->coda_open_by_fd.flags & C_O_WRITE) ? O_RDWR : O_RDONLY;

        MarinerReport(&vtarget->c_fid, u.u_uid);
        *openfd                 = vtarget->c_cf->Open(flags);
        out->coda_open_by_fd.fd = *openfd;
        LOG(10, ("CODA_OPEN_BY_FD: fd = %d\n", *openfd));
        *msg_size = sizeof(struct coda_open_by_fd_out);
    }
}

inline void worker::op_coda_open_by_path(union inputArgs *in,
                                         union outputArgs *out, int *msg_size,
                                         CodaFid *saveFid, int *saveFlags)
{
    struct venus_cnode vtarget;

    LOG(100,
        ("CODA_OPEN_BY_PATH: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

#ifdef __CYGWIN32__
    char *slash;
#endif

    /* Remember some info for dealing with interrupted open calls */
    *saveFid   = in->coda_open_by_path.Fid;
    *saveFlags = in->coda_open_by_path.flags;

    MAKE_CNODE(vtarget, in->coda_open_by_path.Fid, 0);

    open(&vtarget, in->coda_open_by_path.flags);

    if (u.u_error == 0) {
        MarinerReport(&vtarget.c_fid, u.u_uid);

        char *begin = (char *)(&out->coda_open_by_path.path + 1);
        out->coda_open_by_path.path = begin - (char *)out;
        sprintf(begin, "%s%s/%s", CachePrefix, CacheDir, vtarget.c_cf->Name());

#ifdef __CYGWIN32__
        slash = begin;
        for (slash = begin; *slash; slash++) {
            if (*slash == '/')
                *slash = '\\';
        }
#endif
        *msg_size = sizeof(struct coda_open_by_path_out) + strlen(begin) + 1;
        LOG(100,
            ("CODA_OPEN_BY_PATH: returning '%s', size=%d\n", begin, *msg_size));
    }
}

inline void worker::op_coda_readlink(union inputArgs *in, union outputArgs *out,
                                     int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100,
        ("CODA_READLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_readlink.Fid, 0);
    struct coda_string string;
    string.cs_buf    = (char *)out + sizeof(struct coda_readlink_out);
    string.cs_maxlen = CODA_MAXPATHLEN - 1;
    readlink(&vtarget, &string);

    if (u.u_error == 0)
        MarinerReport(&(vtarget.c_fid), u.u_uid);

    out->coda_readlink.count = string.cs_len;
    /* readlink.data is an offset, with the wrong type .. sorry */
    out->coda_readlink.data = (char *)(sizeof(struct coda_readlink_out));

    *msg_size = sizeof(struct coda_readlink_out) + out->coda_readlink.count;
}

inline void worker::op_coda_remove(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    struct venus_cnode vparent;

    LOG(100, ("CODA_REMOVE: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vparent, in->coda_remove.Fid, 0);
    remove(&vparent, (char *)in + (int)in->coda_remove.name);
}

inline void worker::op_coda_rename(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    struct venus_cnode vparent;
    struct venus_cnode vtarget;

    LOG(100, ("CODA_RENAME: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    /* parent = source directory, target = destination directory */
    MAKE_CNODE(vparent, in->coda_rename.sourceFid, 0);
    MAKE_CNODE(vtarget, in->coda_rename.destFid, 0);
    rename(&vparent, (char *)in + (int)in->coda_rename.srcname, &vtarget,
           (char *)in + (int)in->coda_rename.destname);
}

inline void worker::op_coda_rmdir(union inputArgs *in, union outputArgs *out,
                                  int *msg_size)
{
    struct venus_cnode vparent;

    LOG(100, ("CODA_RMDIR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vparent, in->coda_rmdir.Fid, 0);
    rmdir(&vparent, (char *)in + (int)in->coda_rmdir.name);
}

inline void worker::op_coda_root(union inputArgs *in, union outputArgs *out,
                                 int *msg_size)
{
    struct venus_cnode vtarget;

    root(&vtarget);

    if (u.u_error == 0) {
        out->coda_root.Fid = *VenusToKernelFid(&vtarget.c_fid);
        *msg_size          = sizeof(struct coda_root_out);
    }
}

inline void worker::op_coda_setattr(union inputArgs *in, union outputArgs *out,
                                    int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_SETATTR: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    MAKE_CNODE(vtarget, in->coda_setattr.Fid, 0);
    setattr(&vtarget, &in->coda_setattr.attr);
}

inline void worker::op_coda_symlink(union inputArgs *in, union outputArgs *out,
                                    int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_SYMLINK: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));

    MAKE_CNODE(vtarget, in->coda_symlink.Fid, 0);
    symlink(&vtarget, (char *)in + (int)in->coda_symlink.srcname,
            &in->coda_symlink.attr, (char *)in + (int)in->coda_symlink.tname);
}

inline void worker::op_coda_vget(union inputArgs *in, union outputArgs *out,
                                 int *msg_size)
{
    struct venus_cnode vtarget;

    LOG(100, ("CODA_VGET: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    VenusFid vfid;
    KernelToVenusFid(&vfid, &in->coda_vget.Fid);
    vget(&vtarget, &vfid, RC_DATA);

    if (u.u_error == 0) {
        out->coda_vget.Fid   = *VenusToKernelFid(&vtarget.c_fid);
        out->coda_vget.vtype = vtarget.c_type;
        if (vtarget.c_type == C_VLNK && vtarget.c_flags & C_FLAGS_INCON)
            out->coda_vget.vtype |= CODA_NOCACHE;
        *msg_size = sizeof(struct coda_vget_out);
    }
}

inline void worker::op_coda_statfs(union inputArgs *in, union outputArgs *out,
                                   int *msg_size)
{
    LOG(100, ("CODA_STATFS: u.u_pid = %d u.u_pgid = %d\n", u.u_pid, u.u_pgid));
    statfs(&(out->coda_statfs.stat));

    if (u.u_error == 0) {
        *msg_size = sizeof(struct coda_statfs_out);
    }
}

inline void worker::op_coda_access_intent(union inputArgs *in,
                                          union outputArgs *out, int *msg_size)
{
    struct venus_cnode vtarget;
    struct coda_access_intent_in *coda_access_intent = &in->coda_access_intent;

    /* Sanity check */
    CODA_ASSERT(worker::kernel_version >= 5);

    LOG(100,
        ("CODA_ACCESS_INTENT: u.u_pid = %d u.u_pgid = %d pos = %d count = %d, mode = %d \n",
         u.u_pid, u.u_pgid, coda_access_intent->pos, coda_access_intent->count,
         coda_access_intent->mode));

    MAKE_CNODE(vtarget, coda_access_intent->Fid, 0);

    switch (coda_access_intent->mode) {
    case CODA_ACCESS_TYPE_READ:
        read(&vtarget, coda_access_intent->pos, coda_access_intent->count);
        break;
    case CODA_ACCESS_TYPE_WRITE:
        write(&vtarget, coda_access_intent->pos, coda_access_intent->count);
        break;
    case CODA_ACCESS_TYPE_MMAP:
        mmap(&vtarget, coda_access_intent->pos, coda_access_intent->count);
        break;
    case CODA_ACCESS_TYPE_READ_FINISH:
        read_finish(&vtarget, coda_access_intent->pos,
                    coda_access_intent->count);
        break;
    case CODA_ACCESS_TYPE_WRITE_FINISH:
        write_finish(&vtarget, coda_access_intent->pos,
                     coda_access_intent->count);
        break;
    default:
        // Do nothing
        break;
    }
}

void worker::main(void)
{
    struct venus_cnode vtarget;
    CodaFid saveFid;
    int saveFlags = 0;
    int opcode    = 0;
    int size      = 0;
    int openfd    = -1;

    for (;;) {
        openfd = -1;

        /* Wait for new request. */
        AwaitRequest();

        /* Sanity check new request. */
        if (idle)
            CHOKE("Worker: signalled but not dispatched!");
        if (!msg)
            CHOKE("Worker: no message!");

        union inputArgs *in   = (union inputArgs *)msg->msg_buf;
        union outputArgs *out = (union outputArgs *)msg->msg_buf;

        /* we reinitialize these on every loop */
        size        = sizeof(struct coda_out_hdr);
        interrupted = 0;
        returned    = 0;
        StoreFid    = NullFid;

        /* Fill in the user-specific context. */
        u.Init();
        u.u_priority = FSDB->StdPri();
        u.u_flags    = (FOLLOW_SYMLINKS | TRAVERSE_MTPTS | REFERENCE);

        /* GOTTA BE ME */
        u.u_uid  = (in)->ih.uid;
        u.u_pid  = (in)->ih.pid;
        u.u_pgid = (in)->ih.pgid;

        opcode = in->ih.opcode;

        /* This switch corresponds to the kernel trap handler. */
        switch (opcode) {
        case CODA_ACCESS:
            op_coda_access(in, out, &size);
            break;
        case CODA_CLOSE:
            op_coda_close(in, out, &size);
            break;
        case CODA_CREATE:
            op_coda_create(in, out, &size);
            break;
        case CODA_FSYNC:
            op_coda_fsync(in, out, &size);
            break;
        case CODA_GETATTR:
            op_coda_getattr(in, out, &size);
            break;
        case CODA_IOCTL:
            op_coda_ioctl(in, out, &size);
            break;
        case CODA_LINK:
            op_coda_link(in, out, &size);
            break;
        case CODA_LOOKUP:
            op_coda_lookup(in, out, &size);
            break;
        case CODA_MKDIR:
            op_coda_mkdir(in, out, &size);
            break;
        case CODA_OPEN:
            op_coda_open(in, out, &size, &saveFid, &saveFlags);
            break;
        case CODA_OPEN_BY_FD:
            op_coda_open_by_fd(in, out, &size, &openfd, &saveFid, &saveFlags,
                               &vtarget);
            break;
        case CODA_OPEN_BY_PATH:
            op_coda_open_by_path(in, out, &size, &saveFid, &saveFlags);
            break;
        case CODA_READLINK:
            op_coda_readlink(in, out, &size);
            break;
        case CODA_REMOVE:
            op_coda_remove(in, out, &size);
            break;
        case CODA_RENAME:
            op_coda_rename(in, out, &size);
            break;
        case CODA_RMDIR:
            op_coda_rmdir(in, out, &size);
            break;
        case CODA_ROOT:
            op_coda_root(in, out, &size);
            break;
        case CODA_SETATTR:
            op_coda_setattr(in, out, &size);
            break;
        case CODA_SYMLINK:
            op_coda_symlink(in, out, &size);
            break;
        case CODA_VGET:
            op_coda_vget(in, out, &size);
            break;
        case CODA_STATFS:
            op_coda_statfs(in, out, &size);
            break;
        case CODA_ACCESS_INTENT:
            op_coda_access_intent(in, out, &size);
            break;
        default: /* Toned this down a bit, used to be a choke -- DCS */
            /* But make sure someone sees it! */
            eprint("worker::main Got a bogus opcode %d", in->ih.opcode);
            dprint("worker::main Got a bogus opcode %d\n", in->ih.opcode);
            MarinerLog("worker::main Got a bogus opcode %d\n", in->ih.opcode);
            u.u_error = EOPNOTSUPP;
            break;
        }

        out->oh.result = u.u_error;
        Resign(msg, size);

        /* extra case added to deal with dangling references to a collapse */
        if (opcode == CODA_OPEN_BY_FD && openfd != -1)
            vtarget.c_cf->Close(openfd);

        if (opcode == CODA_OPEN || opcode == CODA_OPEN_BY_FD ||
            opcode == CODA_OPEN_BY_PATH) {
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
                msgent *fm           = AllocMsgent();
                union inputArgs *dog = (union inputArgs *)fm->msg_buf;

                dog->coda_close.ih.unique = (unsigned)-1;
                dog->coda_close.ih.opcode = CODA_CLOSE;
                dog->coda_close.Fid       = saveFid;
                dog->coda_close.flags     = saveFlags;

                /* Dispatch it. */
                DispatchWorker(fm);
            }
        }
    }
}

worker_iterator::worker_iterator()
    : vproc_iterator(VPT_Worker)
{
}

worker *worker_iterator::operator()()
{
    return ((worker *)vproc_iterator::operator()());
}
