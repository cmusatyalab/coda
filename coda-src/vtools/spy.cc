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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vtools/spy.cc,v 4.3 1997/02/26 16:04:23 rvb Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus


const char MarinerService[] = "codacon";


int Bind(const char *, const char *);
void CheckMariner(FILE *);
PRIVATE void TERM(int, int, struct sigcontext *);
PRIVATE void usage();


main(int argc, char **argv) {
    char *host = 0;
    char *uid = 0;

    /* Parse args. */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (strcmp(argv[0], "-host") == 0) {
	    if (argc == 1) usage();
	    argc--; argv++;
	    host = argv[0];
	}
	else if (strcmp(argv[0], "-uid") == 0) {
	    if (argc == 1) usage();
	    argc--; argv++;
	    uid = argv[0];
	}
	else
	    usage();
    }

    /* Bind to Venus and ask it to send us Reports of open files. */
    int venusSocket = Bind(MarinerService, host);
    if (venusSocket < 0) {
	fprintf(stderr, "spy: bind(%s, %s) failed\n", MarinerService, host);
	exit(-1);
    }
    FILE *fp = fdopen(venusSocket, "r");
    char buf[32];
    if (uid == 0)
	strcpy(buf, "reporton\n");
    else
	sprintf(buf, "reporton %s\n", uid);
    if (write(venusSocket, buf, strlen(buf)) != strlen(buf)) {
	fprintf(stderr, "spy: reporton command failed (%d)\n", errno);
	exit(-1);
    }

    /* Flush output on termination. */
    signal(SIGTERM, (void (*)(int))TERM);

    /* Read until someone kills the connection. */
    CheckMariner(fp);
}


int Bind(const char *service, const char *host) {
    int s;
    char buf[100];
    struct sockaddr_in server;
    struct servent *sp;
    struct hostent *hp;

    if (host == NULL) {
        gethostname(buf, sizeof(buf));
        host = buf;
    }
    sp = getservbyname(service, "tcp");
    if (sp == NULL){
	return(-1);
    }
    hp = gethostbyname(host);
    if (hp == NULL){
	return(-1);
    }
    bzero((char *)&server, sizeof(server));
    bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    server.sin_family = hp->h_addrtype;
    server.sin_port = sp->s_port;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	return(-1);
    }
    if (connect(s, (sockaddr *)&server, sizeof(server)) < 0) {
        close(s);
        return(-1);
    }
    return(s);
}


void CheckMariner(FILE *fp) {
    static char buf[MAXPATHLEN];
    static char *fillptr = buf;
    register int c;

/*    errno = 0;*/
    while ((c = getc(fp)) != EOF)  {
	if (c != '\n')  {
	    *fillptr++ = c;
	}
	if (c == '\n' || fillptr == buf + sizeof(buf) - 2)  {
	    *fillptr++ = '\n';
	    *fillptr = '\0';

	    printf(buf);

	    fillptr = buf;
	}
    }
}


PRIVATE void TERM(int sig, int code, struct sigcontext *contextPtr) {
    fflush(stdout);
    fflush(stderr);
    exit(0);
}


void usage() {
    fprintf(stderr, "usage: spy [-host host] [-uid uid]\n");
    exit(-1);
}
