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
#endif __cplusplus

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

const char MarinerService[] = "venus";
#define MARINERBUFSIZE 1000


int Bind(const char *, const char *);
void CheckMariner(FILE *);
void CheckTheMariner(char *);


void main(int argc, char *argv[])
{
    int running = 1;
    int venusSocket;

    if (argc > 2) {
	fprintf(stderr, "Bad args: %s [host]\n", argv[0]);
	exit(-1);
    }
    char *host = (argc == 1 ? NULL : argv[1]);

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
	if (write(venusSocket, p, strlen(p)) != strlen(p)) {
	    fprintf(stderr, "codacon: set:fetch command failed (%d)\n", errno);
	} else {
	    /* Read until someone kills the connection. */
	    CheckMariner(fp);
	}

	fclose(fp); close(venusSocket);
    }
}


int Bind(const char *service, const char *host) {
    int s;
    char buf[100];
    struct sockaddr_in server;
    struct servent *sp;
    struct hostent *hp;

    memset(&server, 0, sizeof(server));

    if (host) {
	hp = gethostbyname(host);
	if (hp == NULL) return(-1);
        memcpy((char *)&server.sin_addr, hp->h_addr, hp->h_length);
	server.sin_family = hp->h_addrtype;
    } else {
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	server.sin_family = AF_INET;
    }

    sp = getservbyname(service, "tcp");
    if (sp == NULL) return(-1);
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
