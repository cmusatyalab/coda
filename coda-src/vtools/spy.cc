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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <codaconf.h>
#include <coda_getaddrinfo.h>

int Bind(const char *);
void CheckMariner(FILE *);
static void TERM(int, int, struct sigcontext *);
static void usage();

int use_tcp = 0;

int main(int argc, char **argv)
{
    char *host = 0;
    char *uid = 0;

    /* Parse args. */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (strcmp(argv[0], "-host") == 0) {
	    if (argc == 1) usage();
	    argc--; argv++;
	    host = argv[0];
	}
	else if (strcmp(argv[0], "-tcp") == 0) {
            use_tcp = 1;
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
    int venusSocket = Bind(host);
    if (venusSocket < 0) {
	fprintf(stderr, "spy: bind(%s) failed\n", host);
	exit(-1);
    }
    FILE *fp = fdopen(venusSocket, "r");
    char buf[32];
    if (uid == 0)
	strcpy(buf, "reporton\n");
    else
	sprintf(buf, "reporton %s\n", uid);
    ssize_t len = strlen(buf);
    if (write(venusSocket, buf, len) != len) {
	fprintf(stderr, "spy: reporton command failed (%d)\n", errno);
	exit(-1);
    }

    /* Flush output on termination. */
    signal(SIGTERM, (void (*)(int))TERM);

    /* Read until someone kills the connection. */
    CheckMariner(fp);
}


int Bind(const char *host)
{
    int s = -1;

#ifdef HAVE_SYS_UN_H
    if (!use_tcp) {
	struct sockaddr_un s_un;
	const char *MarinerSocketPath;

	codaconf_init("venus.conf");
	MarinerSocketPath = codaconf_lookup("marinersocket",
					    "/usr/coda/spool/mariner");
	memset(&s_un, 0, sizeof(s_un));
	s_un.sun_family = AF_UNIX;
	strcpy(s_un.sun_path, MarinerSocketPath);

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
	    return(-1);
	}
	if (connect(s, (sockaddr *)&s_un, sizeof(s_un)) < 0) {
	    close(s);
	    return(-1);
	}
    } else
#endif /* !HAVE_SYS_UN_H */
    {
	struct RPC2_addrinfo hints, *p, *ai = NULL;
	int rc;

	memset(&hints, 0, sizeof(struct RPC2_addrinfo));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	rc = coda_getaddrinfo(NULL, "venus", &hints, &ai);
	if (rc) return -1;

	for (p = ai; p != NULL; p = p->ai_next) {
	    s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	    if (s < 0) continue;

	    if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;

	    close(s);
	    s = -1;
	}
	RPC2_freeaddrinfo(ai);
    }
    return s;
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

	    printf("%s", buf);

	    fillptr = buf;
	}
    }
}


static void TERM(int sig, int code, struct sigcontext *contextPtr) {
    fflush(stdout);
    fflush(stderr);
    exit(0);
}


void usage() {
    fprintf(stderr, "usage: spy [-tcp] [-host host] [-uid uid]\n");
    exit(-1);
}
