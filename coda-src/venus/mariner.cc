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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/mariner.cc,v 4.8 1998/08/05 23:50:18 braam Exp $";
#endif /*_BLURB_*/




/*
 *
 * Implementation of the Venus Mariner facility.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#include <lock.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from venus */
#include "fso.h"
#include "simulate.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venus_vnode.h"
#include "vproc.h"
#include "worker.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __cplusplus
}
#endif __cplusplus



const int MarinerStackSize = 65536;
const int MaxMariners = 5;
const char MarinerService[] = "venus";
int MarinerMask = 0;


int mariner::muxfd;
int mariner::nmariners;

void MarinerInit() {
    if (Simulating) return;

    MarinerMask = 0;
    mariner::nmariners = 0;

    /* Socket at which we rendevous with new mariner clients. */
    mariner::muxfd = socket(AF_INET, SOCK_STREAM, 0);
    if (mariner::muxfd < 0 || mariner::muxfd >= NFDS) {
	eprint("MarinerInit: bogus socket (%d)", mariner::muxfd);
	return;
    }

    /* Make address reusable. */
    int on = 1;
#ifndef DJGPP
    if (setsockopt(mariner::muxfd, SOL_SOCKET, SO_REUSEADDR, &on, (int)sizeof(int)) < 0)
	eprint("MarinerInit: setsockopt failed (%d)", errno);
#endif
    /* Look up the well-known CODA mariner service. */
    struct servent *serventp = getservbyname(MarinerService, 0);
    if (!serventp) {
	eprint("MarinerInit: mariner service lookup failed!");
	return;
    }

    /* Bind to it. */
    struct sockaddr_in marsock;
    marsock.sin_family = AF_INET;
    marsock.sin_addr.s_addr = INADDR_ANY;
    marsock.sin_port = serventp->s_port;
    if (bind(mariner::muxfd, (struct sockaddr *)&marsock, (int) sizeof(marsock)) < 0) {
	eprint("MarinerInit: bind failed (%d)", errno);
	return;
    }

    /* Listen for requesters. */
    if (listen(mariner::muxfd, 2) < 0) {
	eprint("MarinerInit: mariner listen failed (%d)", errno);
	return;
    }

    /* Allows the MessageMux to distribute incoming messages to us. */
    MarinerMask |= (1 << mariner::muxfd);
}


void MarinerMux(int mask) {
    LOG(100, ("MarinerMux: mask = %#08x\n", mask));

    /* Handle any new "Mariner Connect" requests. */
    if (mask & (1 << mariner::muxfd)) {
	struct sockaddr_in addr;
	int addrlen = (int)sizeof(struct sockaddr_in);
	int newfd = accept(mariner::muxfd, (sockaddr *)&addr, &addrlen);
	if (newfd < 0)
	    eprint("MarinerMux: accept failed (%d)", errno);
	else if (newfd >= NFDS)
	    ::close(newfd);
	else if (mariner::nmariners >= MaxMariners)
	    ::close(newfd);
#ifndef DJGPP
	else if	(::fcntl(newfd, F_SETFL, O_NDELAY) < 0) {
	    eprint("MarinerMux: fcntl failed (%d)", errno);
	    ::close(newfd);
	}
#else
     	else if (::__djgpp_set_socket_blocking_mode(newfd, 1) < 0) {
		eprint("MarinerMux: set nonblock failed (%d)", errno);
		::close(newfd);
	}
#endif
	else
	    new mariner(newfd);
    }

    /* Dispatch mariners which have pending requests, and kill dying mariners. */
    mariner_iterator next;
    mariner *m;
    while (m = next()) {
	if (m->dying) {
	    delete m;
	    continue;
	}
	if (mask & (1 << m->fd)) {
	    m->DataReady = 1;
	    if (m->idle) {
		if (m->Read() < 0) {
		    delete m;
		    continue;
		}
		m->idle = 0;
		VprocSignal((char *)m);
	    }
	}
    }
}


void MarinerLog(char *fmt ...) {
    va_list ap;
    char buf[180];

    va_start(ap, fmt);
    vsnprintf(buf, 180, fmt, ap);
    va_end(ap);

    int len = (int) strlen(buf);

    mariner_iterator next;
    mariner *m;
    while (m = next())
	if (m->logging) ::write(m->fd, buf, len);
}


/* This should be made an option to a more general logging facility! -JJK */
void MarinerReport(ViceFid *fid, vuid_t vuid) {
    int first = 1;
    char buf[MAXPATHLEN];
    int len;

    mariner_iterator next;
    mariner *m;
    while (m = next())
	if (m->reporting && (m->vuid == ALL_UIDS || m->vuid == vuid)) {
	    if (first) {
		m->u.Init();
		m->u.u_cred.cr_uid = (uid_t)vuid;
		len = MAXPATHLEN;
		m->GetPath(fid, buf, &len);
		if (m->u.u_error == 0) {
		    (buf)[len - 1] = '\n';
		}
		else {
		    (buf)[0] = '\n';
		    len = 0;
		}
		first = 0;
	    }

	    ::write(m->fd, (buf), len);
	}
}


void PrintMariners() {
    PrintMariners(stdout);
}


void PrintMariners(FILE *fp) {
    fflush(fp);
    PrintMariners(fileno(fp));
}


void PrintMariners(int fd) {
    fdprint(fd, "%#08x : %-16s : muxfd = %d, nmariners = %d\n",
	     &mariner::tbl, "Mariners", mariner::muxfd, mariner::nmariners);

    mariner_iterator next;
    mariner *m;
    while (m = next()) m->print(fd);
}


mariner::mariner(int afd) : vproc("Mariner", (PROCBODY) &mariner::main,
			      VPT_Mariner, MarinerStackSize) {
    LOG(100, ("mariner::mariner(%#x): %-16s : lwpid = %d, fd = %d\n",
	     this, name, lwpid, afd));

    nmariners++;	/* Ought to be a lock protecting this! -JJK */

    DataReady = 0;
    dying = 0;
    logging = 0;
    reporting = 0;
    vuid = ALL_UIDS;
    fd = afd;
    bzero(commbuf, MWBUFSIZE);
    MarinerMask |= (1 << fd);

    /* Poke main procedure. */
    VprocSignal((char *)this, 1);
}




/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
mariner::operator=(mariner& m) {
    abort();
    return(0);
}


mariner::~mariner() {
    LOG(100, ("mariner::~mariner: %-16s : lwpid = %d\n", name, lwpid));

    nmariners--;	/* Ought to be a lock protecting this! -JJK */

    if (fd) ::close(fd);
    MarinerMask &= ~(1 << fd);
}


int mariner::Read() {
    if (!DataReady) Choke("mariner::Read: not DataReady!");

    /* Pull the next message out of the socket. */
    int n = ::read(fd, commbuf, (int)(sizeof(commbuf) - 1));
    DataReady = 0;
    if (n <= 0) return(-1);

    /* Erase trailing CR/LF and null-terminate the command. */
    char lastc = commbuf[--n];
    if (lastc != '\012' && lastc != '\015') return(-1);
    while (n > 0 && ((lastc = commbuf[--n]) == '\012' || lastc == '\015'))
	;
    commbuf[n + 1] = 0;

    return(0);
}


/* Duplicates fdprint(). */
int mariner::Write(char *fmt ...) {
    va_list ap;
    char buf[120];

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    int len = (int)strlen(buf);
    int n = ::write(fd, buf, len);
    return(n != len ? -1 : 0);
}


void mariner::AwaitRequest() {
    idle = 1;

    if (DataReady && !dying) {
	if (Read() < 0)
	    dying = 1;
	else {
	    idle = 0;
	    return;
	}
    }

    VprocWait((char *)this);
    if (dying) Choke("mariner::AwaitRequest: signalled while dying!");
}


void mariner::Resign(int code) {
    /* Nothing necessary here. */
    return;
}


void mariner::main(void *parm) {
    static FILE *rpc2trace = 0;

    /* Wait for ctor to poke us. */
    VprocWait((char *)this);

#define	MAXARGS	10
    int argc = 0;
    char *argv[MAXARGS];
    char argbuf[80 * MAXARGS];
    for (int i = 0; i < MAXARGS; i++)
	argv[i] = &argbuf[80 * i];

    for (;;) {
	/* Wait for new request. */
	AwaitRequest();

	/* Sanity check new request. */
	if (idle) Choke("Mariner: signalled but not dispatched!");

	LOG(100, ("mariner::main: cmd = \"%s\"\n", commbuf));

	/* Read a good command.  Parse and execute. */
	argc = sscanf(commbuf, "%80s %80s %80s %80s %80s %80s %80s %80s %80s %80s",
		      argv[0], argv[1], argv[2], argv[3], argv[4],
		      argv[5], argv[6], argv[7], argv[8], argv[9]);
	if (STREQ(argv[0], "help")) {
	    Write("Commands are:\n");
	    Write("\thelp, debugon, debugoff, dumpcore, quit, rpcon, rpcoff, rpc2t\n");
	    Write("\tera, cop <modes>, umc, set:fetch, clear:fetch\n, reporton <uid>, reportoff\n");
	    Write("\t, fd <fd>pathstat <pathname>, fidstat <fid>, rpc2stat, print <args>\n");
	}
	else if (STREQ(argv[0], "debugon")) {
	    DebugOn();
	    Write("LogLevel is now %d\n", LogLevel);
	}
	else if (STREQ(argv[0], "debugoff")) {
	    DebugOff();
	    Write("LogLevel is now %d\n", LogLevel);
	}
	else if (STREQ(argv[0], "dumpcore")) {
	    Choke("Telnet");
	}
	else if (STREQ(argv[0], "quit")) {
	    dying = 1;
	}
	else if (STREQ(argv[0], "rpcon")) {
	    if (freopen("rpc.log", "w+", stdout) == NULL)
		Write("rpcon failed\n");
	    else
		RPC2_DebugLevel = 100;
	}
	else if (STREQ(argv[0], "rpcoff")) {
	    fflush(stdout);
	    fclose(stdout);
	}
	else if (STREQ(argv[0], "rpc2t")) {
	    if (rpc2trace == 0) {	/* Turn on rpc2 tracing. */
		rpc2trace = fopen("rpc.trace", "w+");
		RPC2_Trace = 1;
		RPC2_InitTraceBuffer(500);
		Write("tracing on\n");
	    }
	    else {
		RPC2_DumpTrace(rpc2trace, 500);
		fclose(rpc2trace);
		rpc2trace = 0;
		Write("tracing off\n");
	    }
	}
	else if (STREQ(argv[0], "era")) {
	    EarlyReturnAllowed = (1 - EarlyReturnAllowed);
	    Write("EarlyReturnAllowed is now %d.\n", EarlyReturnAllowed);
	}
	else if (STREQ(argv[0], "cop") && argc == 2) {
	    /* This is a hack! -JJK */
	    int OldModes = COPModes;
	    COPModes = atoi(argv[1]);
	    if ((ASYNCCOP1 || PIGGYCOP2) && !ASYNCCOP2) {
		Write("Bogus modes (%x)\n", COPModes);
		COPModes = OldModes;
	    }
	    Write("COPModes = %x\n", COPModes);
	}
	else if (STREQ(argv[0], "umc")) {
	    UseMulticast = (1 - UseMulticast);
	    Write("UseMulticast is now %d.\n", UseMulticast);
	}
	else if (STREQ(argv[0], "set:fetch")) {
	    logging = 1;
	}
	else if (STREQ(argv[0], "clear:fetch")) {
	    logging = 0;
	}
	else if (STREQ(argv[0], "reporton")) {
	    reporting = 1;
	    vuid = (argc == 1 ? ALL_UIDS : atoi(argv[1]));
	}
	else if (STREQ(argv[0], "reportoff")) {
	    reporting = 0;
	}
	else if (STREQ(argv[0], "fd") && argc == 2) {
	    struct stat tstat;
	    if (::fstat(atoi(argv[1]), &tstat) < 0)
		Write("Error %d\n", errno);
	    else
		Write("%x:%d\n", tstat.st_dev, tstat.st_ino);
	}
	else if (STREQ(argv[0], "pathstat") && argc == 2) {
	    /* Lookup the object and print it out. */
	    PathStat(argv[1]);
	}
	else if (STREQ(argv[0], "fidstat") && argc == 2) {
	    /* Lookup the object and print it out. */
	    ViceFid fid;
	    if (sscanf(argv[1], "%x.%x.%x", &fid.Volume, &fid.Vnode, &fid.Unique) == 3)
		FidStat(&fid);
	    else
		Write("badly formed fid; try (%%x.%%x.%%x)\n");
	}
	else if (STREQ(argv[0], "rpc2stat")) {
	    Rpc2Stat();
	}
	else if (STREQ(argv[0], "print")) {
	    VenusPrint(fd, argc - 1, &argv[1]);
	}
	else {
	    Write("bad mariner command %-80s\n", argv[0]);
	}

	Resign(0);
	seq++;
    }
}


void mariner::PathStat(char *path) {
    /* Map pathname to fid. */
    u.Init();
    u.u_cred.cr_uid = (uid_t)V_UID;
#ifdef	__BSD44__
    u.u_cred.cr_groupid = (gid_t)V_GID;
#else
    u.u_cred.cr_gid = (gid_t)V_GID;
#endif
    u.u_priority = 0;
    u.u_cdir = rootfid;
    u.u_nc = 0;
    struct venus_vnode *tvp = 0;
    if (!namev(path, 0, &tvp)) {
	Write("namev(%s) failed (%d)\n", path, u.u_error);
	return;
    }
    ViceFid fid = (VTOC(tvp))->c_fid;
    DISCARD_VNODE(tvp);
    tvp = 0;
    Write("PathStat: %s --> %x.%x.%x\n",
	   path, fid.Volume, fid.Vnode, fid.Unique);

    /* Print status of corresponding fsobj. */
    FidStat(&fid);
}


void mariner::FidStat(ViceFid *fid) {
    /* Set up context. */
    u.Init();
    u.u_cred.cr_uid = (uid_t)V_UID;
#ifdef	__BSD44__
    u.u_cred.cr_groupid = (gid_t)V_GID;
#else
    u.u_cred.cr_gid = (gid_t)V_GID;
#endif
    u.u_priority = FSDB->MaxPri();

    fsobj *f = 0;
    for (;;) {
	Begin_VFS(fid->Volume, (int) VFSOP_VGET);
	if (u.u_error) {
	    Write("Begin_VFS(%x) failed (%d)\n", fid->Volume, u.u_error);
	    break;
	}

	u.u_error = FSDB->Get(&f, fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) {
	    Write("fsdb::Get(%x.%x.%x) failed (%d)\n",
		  fid->Volume, fid->Vnode, fid->Unique, u.u_error);
	    goto FreeLocks;
	}

	Write("FidStat(%x.%x.%x):\n", fid->Volume, fid->Vnode, fid->Unique);
	f->print(fd);

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

    if (u.u_error == EINCONS)
	k_Purge(fid, 1);
}

void mariner::Rpc2Stat() {
    Write("RPC2:\n");
    Write("Packets Sent = %ld\tPacket Retries = %ld\tPackets Received = %ld\n",
    	rpc2_Sent.Total, rpc2_Sent.Retries, rpc2_Recvd.Total);
    Write("\t%Multicasts Sent = %ld\tBusies Sent = %ld\tNaks = %ld\n", 
	  rpc2_Sent.Multicasts, rpc2_Sent.Busies, rpc2_Sent.Naks);
    Write("Bytes sent = %ld\tBytes received = %ld\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    Write("Received Packet Distribution:\n");
    Write("\tRequests = %ld\tGoodRequests = %ld\n",
	  rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests);
    Write("\tReplies = %ld\tGoodReplies = %ld\n",
	  rpc2_Recvd.Replies, rpc2_Recvd.GoodReplies);
    Write("\tBusies = %ld\tGoodBusies = %ld\n",
	  rpc2_Recvd.Busies, rpc2_Recvd.GoodBusies);
    Write("\tMulticasts = %ld\tGoodMulticasts = %ld\n",
	  rpc2_Recvd.Multicasts, rpc2_Recvd.GoodMulticasts);
    Write("\tBogus packets = %ld\n\tNaks = %ld\n",
	  rpc2_Recvd.Bogus, rpc2_Recvd.Naks);
	  
    Write("\nSFTP:\n");
    Write("Packets Sent = %ld\t\tStarts Sent = %ld\t\tDatas Sent = %ld\n",
	  sftp_Sent.Total, sftp_Sent.Starts, sftp_Sent.Datas);
    Write("Data Retries Sent = %ld\t\tAcks Sent = %ld\t\tNaks Sent = %ld\n",
	  sftp_Sent.DataRetries, sftp_Sent.Acks, sftp_Sent.Naks);
    Write("Busies Sent = %ld\t\t\tBytes Sent = %ld\n",
	  sftp_Sent.Busies, sftp_Sent.Bytes);
    Write("Packets Received = %ld\t\tStarts Received = %ld\tDatas Received = %ld\n",
	  sftp_Recvd.Total, sftp_Recvd.Starts, sftp_Recvd.Datas);
    Write("Data Retries Received = %ld\tAcks Received = %ld\tNaks Received = %ld\n",
	  sftp_Recvd.DataRetries, sftp_Recvd.Acks, sftp_Recvd.Naks);
    Write("Busies Received = %ld\t\tBytes Received = %ld\n",
	  sftp_Recvd.Busies, sftp_Recvd.Bytes);
}

mariner_iterator::mariner_iterator() : vproc_iterator(VPT_Mariner) {
}


mariner *mariner_iterator::operator()() {
    return((mariner *)vproc_iterator::operator()());
}
