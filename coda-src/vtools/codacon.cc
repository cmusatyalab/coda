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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vtools/codacon.cc,v 4.5 98/11/06 18:27:42 jaharkes Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
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
