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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <codaconf.h>

#ifdef __cplusplus
}
#endif

/* FreeBSD 2.2.5 defines this in rpc/types.h, all others in netinet/in.h */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

const char MarinerService[] = "venus";
#define MARINERBUFSIZE 1000


int Bind(const char *, const char *);
void CheckMariner(FILE *);
void CheckTheMariner(char *);

int use_tcp = 0;

int main(int argc, char *argv[])
{
    int running = 1;
    int venusSocket;
    char *host = NULL;

    while (--argc) {
        argv++;
        if (strcmp(*argv, "-tcp") == 0) use_tcp = 1;
        else if (host) {
            fprintf(stderr, "Bad args: %s [-tcp] [host]\n", argv[0]);
            exit(-1);
        }
        else host = *argv;
    }

    while (running)
    {
	/* Bind to Venus and ask it to send us Fetch/Store messages. */
	venusSocket = Bind(MarinerService, host);
	if (venusSocket < 0) {
	    fprintf(stderr, "codacon: bind to %s failed, venus not running?\n",
		    host ? host : "localhost");
	    sleep(5);
	    continue;
	}
	FILE *fp = fdopen(venusSocket, "r");
	char *p = "set:fetch\n";
	ssize_t len = strlen(p);
	if (write(venusSocket, p, len) != len) {
	    fprintf(stderr, "codacon: set:fetch command failed (%d)\n", errno);
	} else {
	    /* Read until someone kills the connection. */
	    CheckMariner(fp);
	}

	fclose(fp); close(venusSocket);
    }
}


int Bind(const char *service, const char *host)
{
    int s;
    
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

        memset(&sin, 0, sizeof(sin));

        if (host) {
            hp = gethostbyname(host);
            if (hp == NULL) return(-1);
            memcpy((char *)&sin.sin_addr, hp->h_addr, hp->h_length);
            sin.sin_family = hp->h_addrtype;
        } else {
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            sin.sin_family = AF_INET;
        }

        sp = getservbyname(service, "tcp");
        if (sp == NULL) return(-1);
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
    static char buf[MARINERBUFSIZE];
    static char *fillptr = buf;
    register int c;

/*    errno = 0;*/
    while ((c = getc(fp)) != EOF)  {
	if (c != '\n')  {
	    *fillptr++ = c;
	}
	if (c == '\n' || fillptr >= buf + sizeof(buf) - 15)  {
	    --fillptr; /* back off one from the ending NULL */
	    while (fillptr != buf && *fillptr == ' ')
		--fillptr;
	    if (fillptr != buf)
		++fillptr;

	    time_t curr_time = time(0);
	    struct tm *lt = localtime(&curr_time);
	    sprintf(fillptr, " ( %02d:%02d:%02d )",
		    lt->tm_hour, lt->tm_min, lt->tm_sec);
	    fillptr += 13;

	    *fillptr++ = '\n';
	    *fillptr = '\0';
	    CheckTheMariner(buf);
	    fillptr = buf;
	}
    }
}


void CheckTheMariner(char *buf) {
    char *s;

    if (strncmp(buf, "fetch::", 7) == 0) {
	s = &(buf[7]);
	char done[128];
	if ((sscanf(s, "%*s %s", done) == 1) && strcmp(done, "done") == 0) {
	    /* Nothing, yet. */
	    return;
	}
    }
    else if (strncmp(buf, "store::", 7) == 0) {
	s = &(buf[7]);
	char done[128];
	if ((sscanf(s, "%*s %s", done) == 1) && strcmp(done, "done") == 0) {
	    /* Nothing, yet. */
	    return;
	}
    }
    else if (strncmp(buf, "callback::", 10) == 0) {
	s = &(buf[10]);
    }
    else if (strncmp(buf, "cache::", 7) == 0) {
	s = &(buf[7]);
    }
    else if (strncmp(buf, "mond::", 6) == 0) {
	s = &(buf[6]);
	char done[128];
	if ((sscanf(s, "%*s %s", done) == 1) && strcmp(done, "done") == 0) {
	    /* Nothing, yet. */
	    return;
	}
    }
    else {
	s = &(buf[0]);
    }

    printf(s);
    fflush(stdout);
}
