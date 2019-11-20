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
 * Implementation of the Venus Mariner facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <lwp/lock.h>
#include <rpc2/rpc2.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <mkpath.h>
#include <coda_getaddrinfo.h>

/* from venus */
#include "fso.h"
#include "venus.private.h"
#include "venuscb.h"
#include "vproc.h"
#include "worker.h"
#include "realmdb.h"
#include "venusvol.h"
#include "9pfs.h"

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

const int MarinerStackSize      = 65536;
const int MaxMariners           = 25;
static bool plan9server_enabled = false;

int mariner::nmariners;

void MarinerInit()
{
    const char *MarinerSocketPath = GetVenusConf().get_value("marinersocket");
    plan9server_enabled           = GetVenusConf().get_bool_value("9pfs");
    int sock, rc, opt = 1;

    mariner::nmariners = 0;

#ifdef HAVE_SYS_UN_H
    /* use unix domain sockets wherever available */
    struct sockaddr_un s_un;

    /* make sure the path leading to the mariner socket is there */
    mkpath(MarinerSocketPath, 0755);
    unlink(MarinerSocketPath);

    memset(&s_un, 0, sizeof(struct sockaddr_un));
    s_un.sun_family = AF_UNIX;
    strcpy(s_un.sun_path, MarinerSocketPath);

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        eprint("MarinerInit: socket creation failed", errno);
        goto Next;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));

    rc = bind(sock, (struct sockaddr *)&s_un, (socklen_t)sizeof(s_un));
    if (rc < 0) {
        eprint("MarinerInit: socket bind failed", errno);
        close(sock);
        goto Next;
    }

    /* make sure the socket is accessible */
    chmod(MarinerSocketPath, 0777);

    if (listen(sock, 2) < 0) {
        eprint("MarinerInit: socket listen failed", errno);
        close(sock);
        goto Next;
    }
    MUX_add_callback(sock, MarinerMux, NULL);
#endif /* HAVE_SYS_UN_H */

Next:
    if (!GetVenusConf().get_bool_value("MarinerTcp"))
        return;

    struct RPC2_addrinfo hints, *result = NULL, *ai;
    int warned = 0;

    memset(&hints, 0, sizeof(struct RPC2_addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = coda_getaddrinfo(NULL, "venus", &hints, &result);
    if (rc) {
        eprint("MarinerInit: failed to resolve loopback address");
        return;
    }

    /* try to bind all returned addresses */
    for (ai = result; ai != NULL; ai = ai->ai_next) {
        /* Socket at which we rendevous with new clients. */
        sock = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) {
            eprint("MarinerInit: failed to create socket");
            continue;
        }

        /* Make address reusable. */
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));

        /* Bind to it. */
        rc = ::bind(sock, ai->ai_addr, ai->ai_addrlen);
        if (rc < 0) {
            eprint("MarinerInit: bind failed (%d)", errno);
            ::close(sock);
            continue;
        }

        /* Listen for requesters. */
        rc = listen(sock, 2);
        if (rc < 0) {
            eprint("MarinerInit: mariner listen failed (%d)", errno);
            ::close(sock);
            continue;
        }

        if (!warned++)
            eprint(
                "MarinerInit: listening on tcp port enabled, "
                "disable with -noMarinerTcp");
        MUX_add_callback(sock, MarinerMux, NULL);
    }
    RPC2_freeaddrinfo(result);
}

void MarinerMux(int fd, void *udata)
{
    int newfd = -1, rc, flags;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    char host[NI_MAXHOST], port[NI_MAXSERV];

    LOG(100, ("MarinerMux: fd = %d\n", fd));

    newfd = ::accept(fd, (sockaddr *)&sa, &salen);
    if (newfd < 0) {
        eprint("MarinerMux: accept failed (%s)", strerror(errno));
        return;
    }

    rc = getnameinfo((struct sockaddr *)&sa, salen, host, sizeof(host), port,
                     sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    int broken_port = sa.ss_family == AF_UNIX;
    LOG(0,
        ("MarinerMux: new client connection from [%s]:%s\n",
         rc ? "resolve failed" : host, (rc || broken_port) ? "unknown" : port));

    if (mariner::nmariners >= MaxMariners) {
        eprint("MarinerMux: client connection limit exceeded");
        ::close(newfd);
        return;
    }

    /* set socket to non-blocking */
    flags = ::fcntl(newfd, F_GETFL, 0);
    if (flags != -1)
        ::fcntl(newfd, F_SETFL, flags | O_NONBLOCK);

    new mariner(newfd);
}

void MarinerLog(const char *fmt, ...)
{
    va_list ap;
    char buf[CODA_MAXNAMLEN + 1];
    int len;
    mariner_iterator next;
    mariner *m;

    va_start(ap, fmt);
    vsnprintf(buf, CODA_MAXNAMLEN, fmt, ap);
    va_end(ap);

    buf[CODA_MAXNAMLEN] = '\0';
    len                 = (int)strlen(buf);

    while ((m = next()))
        if (m->IsLogging())
            m->write(buf, len);
}

/* This should be made an option to a more general logging facility! -JJK */
void MarinerReport(VenusFid *fid, uid_t uid)
{
    int first = 1;
    char buf[MAXPATHLEN];
    int len;

    mariner_iterator next;
    mariner *m;
    while ((m = next()))
        if (m->reporting && (m->uid == ANYUSER_UID || m->uid == uid)) {
            if (first) {
                m->u.Init();
                m->u.u_uid = uid;
                len        = MAXPATHLEN;
                m->GetPath(fid, buf, &len);
                if (m->u.u_error == 0) {
                    (buf)[len - 1] = '\n';
                } else {
                    (buf)[0] = '\n';
                    len      = 0;
                }
                first = 0;
            }
            m->write(buf, len);
        }
}

void MarinerReportVolState(const char *volname, const char *realm,
                           const char *state, int cml_entries,
                           const struct VolFlags *vflags)
{
    char buf[CODA_MAXNAMLEN + 1];
    mariner_iterator next;
    mariner *m;
    int len;

    len = snprintf(buf, CODA_MAXNAMLEN, "volstate::%s@%s %s %d%s%s%s%s%s",
                   volname, realm, state, cml_entries,
                   vflags->unauthenticated ? " unauth" : "",
                   vflags->reint_conflict ? " conflict" : "",
                   vflags->asr_running ? " asr" : "",
                   vflags->reintegrating ? " reint" : "",
                   vflags->transition_pending ? " transition" : "");

    if (len < 0)
        return;
    if (len > CODA_MAXNAMLEN)
        len = CODA_MAXNAMLEN;
    buf[len] = '\n';
    len++;

    while ((m = next()))
        if (m->WantVolState())
            m->write(buf, len);
}

void PrintMariners()
{
    PrintMariners(stdout);
}

void PrintMariners(FILE *fp)
{
    fflush(fp);
    PrintMariners(fileno(fp));
}

void PrintMariners(int fd)
{
    fdprint(fd, "%#08x : %-16s : nmariners = %d\n", &mariner::tbl, "Mariners",
            mariner::nmariners);

    mariner_iterator next;
    mariner *m;
    while ((m = next()))
        m->print(fd);
}

mariner::mariner(int afd)
    : vproc("Mariner", NULL, VPT_Mariner, MarinerStackSize)
{
    LOG(100, ("mariner::mariner(%#x): %-16s : lwpid = %d, fd = %d\n", this,
              name, lwpid, afd));

    nmariners++; /* Ought to be a lock protecting this! -JJK */

    dying         = 0;
    logging       = 0;
    reporting     = 0;
    want_volstate = 0;
    uid           = ANYUSER_UID;
    fd            = afd;
    p9srv         = NULL;
    memset(commbuf, 0, MWBUFSIZE);

    Lock_Init(&write_lock);

    /* Poke main procedure. */
    start_thread();
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
int mariner::operator=(mariner &m)
{
    abort();
    return (0);
}

mariner::~mariner()
{
    LOG(100, ("mariner::~mariner: %-16s : lwpid = %d\n", name, lwpid));

    nmariners--; /* Ought to be a lock protecting this! -JJK */
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}

/* non-blocking read all 'len' bytes */
ssize_t mariner::read_until_done(void *buf, size_t len)
{
    unsigned char *ptr = (unsigned char *)buf;
    size_t bytes_read  = 0;
    fd_set fds;
    ssize_t n;

    while (bytes_read < len) {
        n = ::read(fd, ptr + bytes_read, len - bytes_read);
        if (n < 0 && errno != EAGAIN)
            return -1;

        if (n > 0) {
            bytes_read += n;
            if (bytes_read >= len) {
                assert(bytes_read == len);
                break;
            }
        }

        /* wait for more */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        n = ::IOMGR_Select(fd + 1, &fds, NULL, NULL, NULL);
        if (n < 0)
            return -1;
    }
    return bytes_read;
}

/* non-blocking write everything in the buffer */
ssize_t mariner::write_until_done(const void *buf, size_t len)
{
    unsigned char *ptr   = (unsigned char *)buf;
    size_t bytes_written = 0;
    fd_set fds;
    ssize_t n;

    if (len == 0)
        return 0;

    ObtainWriteLock(&write_lock);

    while (bytes_written < len) {
        n = ::write(fd, ptr + bytes_written, len - bytes_written);
        if (n < 0 && errno != EAGAIN)
            goto err_out;

        if (n > 0) {
            bytes_written += n;
            if (bytes_written >= len) {
                assert(bytes_written == len);
                break;
            }
        }

        /* wait until the fd becomes writeable again */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        n = IOMGR_Select(fd + 1, NULL, &fds, NULL, NULL);
        if (n < 0)
            goto err_out;
    }

    ReleaseWriteLock(&write_lock);
    return bytes_written;

err_out:
    ReleaseWriteLock(&write_lock);
    return -1;
}

/* Duplicates fdprint(). */
int mariner::Write(const char *fmt, ...)
{
    va_list ap;
    char buf[180];

    va_start(ap, fmt);
    vsnprintf(buf, 180, fmt, ap);
    va_end(ap);

    int len = strlen(buf);
    return write_until_done(buf, len);
}

int mariner::AwaitRequest()
{
    unsigned int idx = 0;
    ssize_t n;

    idle = 1;

    while (idx < (sizeof(commbuf) - 1) && !dying) {
        n = read_until_done(&commbuf[idx], 1);
        if (n <= 0)
            return -1;

        if (commbuf[idx] == '\r')
            continue;

        if (commbuf[idx] == '\n') {
            commbuf[idx] = '\0';
            /* end of line seen, return to caller */
            break;
        }
        idx++;

        /* check for 9PFS Tversion request magic */
        /* we really only want to do this for the first 19 bytes of a new
         * connection, and the '\r' stripping and early exit of the loop when a
         * '\n' is found can mangle the incoming 9pfs Tversion message. */
        if (plan9server_enabled && idx == P9_MAGIC_LEN) {
            if (memcmp(&commbuf[1], p9_magic1, sizeof(p9_magic1) - 1) == 0 &&
                memcmp(&commbuf[12], p9_magic12, sizeof(p9_magic12) - 1) == 0) {
                /* make sure we no longer send any normal mariner output */
                logging = reporting = want_volstate = 0;

                LOG(0, ("Found 9pfs version magic\n"));

                p9srv = new plan9server(this);
                p9srv->main_loop((unsigned char *)commbuf, idx);
                delete p9srv;
                p9srv = NULL;

                /* when the 9pfs main loop is done, we want to also exit the
                 * mariner main loop */
                return -1;
            }
        }
    }
    idle = 0;
    return 0;
}

void mariner::Resign(int code)
{
    /* Nothing necessary here. */
    return;
}

void mariner::main(void)
{
    static FILE *rpc2trace = 0;

#define MAXARGS 10
    int argc = 0;
    char *argv[MAXARGS];
    char argbuf[80 * MAXARGS];
    for (int i = 0; i < MAXARGS; i++)
        argv[i] = &argbuf[80 * i];

    for (;;) {
        /* Wait for new request. */
        if (AwaitRequest())
            break;

        LOG(100, ("mariner::main: cmd = \"%s\"\n", commbuf));

        /* Read a good command.  Parse and execute. */
        argc = sscanf(commbuf,
                      "%80s %80s %80s %80s %80s %80s %80s %80s %80s %80s",
                      argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                      argv[6], argv[7], argv[8], argv[9]);
        if (argc < 1 || STREQ(argv[0], "help")) {
            Write(
                "Commands are:\n"
                "\thelp, debugon, debugoff, dumpcore, quit, rpc2t\n"
                "\tcop <modes>, set:fetch, clear:fetch, reporton <uid>, reportoff\n"
                "\tset:volstate, clear:volstate, fd <fd>pathstat <pathname>\n"
                "\tfidstat <fid>, rpc2stat, print <args>\n");
        } else if (STREQ(argv[0], "debugon")) {
            DebugOn();
            Write("GetLogLevel() is now %d\n", GetLogLevel());
        } else if (STREQ(argv[0], "debugoff")) {
            DebugOff();
            Write("GetLogLevel() is now %d\n", GetLogLevel());
        } else if (STREQ(argv[0], "dumpcore")) {
            CHOKE("Telnet");
        } else if (STREQ(argv[0], "quit")) {
            dying = 1;
        } else if (STREQ(argv[0], "rpc2t")) {
            if (rpc2trace == 0) { /* Turn on rpc2 tracing. */
                rpc2trace = fopen("rpc.trace", "w+");
                extern long int RPC2_Trace;
                RPC2_Trace = 1;
                RPC2_InitTraceBuffer(500);
                Write("tracing on\n");
            } else {
                RPC2_DumpTrace(rpc2trace, 500);
                fclose(rpc2trace);
                rpc2trace = 0;
                Write("tracing off\n");
            }
        } else if (STREQ(argv[0], "cop") && argc == 2) {
            /* This is a hack! -JJK */
            int COPModes = atoi(argv[1]);
            if ((HAS_ASYNCCOP1(COPModes) || HAS_PIGGYCOP2(COPModes)) &&
                !HAS_ASYNCCOP2(COPModes)) {
                Write("Bogus modes (%x)\n", COPModes);
            }
            Write("COPModes = %x\n", COPModes);
            SetCOPModes(COPModes);
        } else if (STREQ(argv[0], "set:fetch")) {
            logging = 1;
        } else if (STREQ(argv[0], "clear:fetch")) {
            logging = 0;
        } else if (STREQ(argv[0], "reporton")) {
            reporting = 1;
            uid       = (argc == 1 ? ANYUSER_UID : atoi(argv[1]));
        } else if (STREQ(argv[0], "reportoff")) {
            reporting = 0;
        } else if (STREQ(argv[0], "set:volstate")) {
            repvol_iterator next;
            repvol *v;

            want_volstate = 1;

            while ((v = next()))
                v->ReportVolState();
        } else if (STREQ(argv[0], "clear:volstate")) {
            want_volstate = 0;
        } else if (STREQ(argv[0], "fd") && argc == 2) {
            struct stat tstat;
            if (::fstat(atoi(argv[1]), &tstat) < 0)
                Write("Error %d\n", errno);
            else
                Write("%x:%d\n", tstat.st_dev, tstat.st_ino);
        } else if (STREQ(argv[0], "pathstat") && argc == 2) {
            /* Lookup the object and print it out. */
            PathStat(argv[1]);
        } else if (STREQ(argv[0], "fidstat") && argc == 2) {
            /* Lookup the object and print it out. */
            VenusFid fid;
            char tmp;
            if (sscanf(argv[1], "%x.%x.%x@%c", &fid.Volume, &fid.Vnode,
                       &fid.Unique, &tmp) == 4) {
                /* strrchr should succeed now because sscanf succeeded. */
                char *realmname = strrchr(argv[1], '@') + 1;
                Realm *realm    = REALMDB->GetRealm(realmname);
                fid.Realm       = realm->Id();
                FidStat(&fid);
                realm->PutRef();
            } else
                Write("badly formed fid; try (%%x.%%x.%%x@%%s)\n");
        } else if (STREQ(argv[0], "rpc2stat")) {
            Rpc2Stat();
        } else if (STREQ(argv[0], "print")) {
            VenusPrint(fd, argc - 1, (const char **)&argv[1]);
        } else {
            Write("bad mariner command '%-80s'\n", argv[0]);
        }

        Resign(0);
        seq++;
    }
}

void mariner::PathStat(char *path)
{
    /* Map pathname to fid. */
    u.Init();
    u.u_uid      = V_UID;
    u.u_priority = 0;
    u.u_cdir     = rootfid;
    u.u_nc       = 0;
    struct venus_cnode tcp;
    if (!namev(path, 0, &tcp)) {
        Write("namev(%s) failed (%d)\n", path, u.u_error);
        return;
    }
    VenusFid fid = tcp.c_fid;

    Write("PathStat: %s --> %s\n", path, FID_(&fid));

    /* Print status of corresponding fsobj. */
    FidStat(&fid);
}

void mariner::FidStat(VenusFid *fid)
{
    /* Set up context. */
    u.Init();
    u.u_uid      = V_UID;
    u.u_priority = FSDB->MaxPri();

    fsobj *f = 0;
    for (;;) {
        Begin_VFS(fid, CODA_VGET);
        if (u.u_error) {
            Write("Begin_VFS(%s) failed (%d)\n", FID_(fid), u.u_error);
            break;
        }

        u.u_error = FSDB->Get(&f, fid, u.u_uid, RC_STATUS, NULL, NULL, NULL, 1);
        if (u.u_error) {
            Write("fsdb::Get(%s) failed (%d)\n", FID_(fid), u.u_error);
            goto FreeLocks;
        }

        Write("FidStat(%s):\n", FID_(fid));
        f->print(fd);

    FreeLocks:
        FSDB->Put(&f);
        int retry_call = 0;
        End_VFS(&retry_call);
        if (!retry_call)
            break;
    }

    if (u.u_error == EINCONS)
        k_Purge(fid, 1);
}

void mariner::Rpc2Stat()
{
    Write("RPC2:\n");
    Write("Packets Sent = %ld\tPacket Retries = %ld\tPackets Received = %ld\n",
          rpc2_Sent.Total, rpc2_Sent.Retries, rpc2_Recvd.Total);
    Write("\t%Multicasts Sent = %ld\tBusies Sent = %ld\tNaks = %ld\n",
          rpc2_Sent.Multicasts, rpc2_Sent.Busies, rpc2_Sent.Naks);
    Write("Bytes sent = %ld\tBytes received = %ld\n", rpc2_Sent.Bytes,
          rpc2_Recvd.Bytes);
    Write("Received Packet Distribution:\n");
    Write("\tRequests = %ld\tGoodRequests = %ld\n", rpc2_Recvd.Requests,
          rpc2_Recvd.GoodRequests);
    Write("\tReplies = %ld\tGoodReplies = %ld\n", rpc2_Recvd.Replies,
          rpc2_Recvd.GoodReplies);
    Write("\tBusies = %ld\tGoodBusies = %ld\n", rpc2_Recvd.Busies,
          rpc2_Recvd.GoodBusies);
    Write("\tMulticasts = %ld\tGoodMulticasts = %ld\n", rpc2_Recvd.Multicasts,
          rpc2_Recvd.GoodMulticasts);
    Write("\tBogus packets = %ld\n\tNaks = %ld\n", rpc2_Recvd.Bogus,
          rpc2_Recvd.Naks);

    Write("\nSFTP:\n");
    Write("Packets Sent = %ld\t\tStarts Sent = %ld\t\tDatas Sent = %ld\n",
          sftp_Sent.Total, sftp_Sent.Starts, sftp_Sent.Datas);
    Write("Data Retries Sent = %ld\t\tAcks Sent = %ld\t\tNaks Sent = %ld\n",
          sftp_Sent.DataRetries, sftp_Sent.Acks, sftp_Sent.Naks);
    Write("Busies Sent = %ld\t\t\tBytes Sent = %ld\n", sftp_Sent.Busies,
          sftp_Sent.Bytes);
    Write(
        "Packets Received = %ld\t\tStarts Received = %ld\tDatas Received = %ld\n",
        sftp_Recvd.Total, sftp_Recvd.Starts, sftp_Recvd.Datas);
    Write(
        "Data Retries Received = %ld\tAcks Received = %ld\tNaks Received = %ld\n",
        sftp_Recvd.DataRetries, sftp_Recvd.Acks, sftp_Recvd.Naks);
    Write("Busies Received = %ld\t\tBytes Received = %ld\n", sftp_Recvd.Busies,
          sftp_Recvd.Bytes);
}

mariner_iterator::mariner_iterator()
    : vproc_iterator(VPT_Mariner)
{
}

mariner *mariner_iterator::operator()()
{
    return ((mariner *)vproc_iterator::operator()());
}
