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
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <codaconf.h>

const char MarinerService[] = "venus";


int Bind(const char *, const char *);
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


int Bind(const char *service, const char *host) {
    int s;
    char buf[100];

#ifdef HAVE_SYS_UN_H
    if (!use_tcp) {
        struct sockaddr_un s_un;
        char *MarinerSocketPath;
        
        codaconf_init("venus.conf");
        MarinerSocketPath = conf_lookup("marinersocket",
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
        struct sockaddr_in sin;
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
        memset(&sin, 0, sizeof(sin));
        memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
        sin.sin_family = hp->h_addrtype;
        sin.sin_port = sp->s_port;
        if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            return(-1);
        }
        if (connect(s, (sockaddr *)&sin, sizeof(sin)) < 0) {
            close(s);
            return(-1);
        }
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


static void TERM(int sig, int code, struct sigcontext *contextPtr) {
    fflush(stdout);
    fflush(stderr);
    exit(0);
}


void usage() {
    fprintf(stderr, "usage: spy [-tcp] [-host host] [-uid uid]\n");
    exit(-1);
}
