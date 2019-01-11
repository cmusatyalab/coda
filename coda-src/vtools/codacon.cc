/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <codaconf.h>
#include <coda_getaddrinfo.h>

#define MARINERBUFSIZE 1000

static int Bind(const char *);
static void CheckMariner(FILE *);
static void CheckTheMariner(char *);

int use_tcp = 0;

int main(int argc, char *argv[])
{
    int running = 1;
    int venusSocket;
    char *host = NULL;

    while (--argc) {
        argv++;
        if (strcmp(*argv, "-tcp") == 0)
            use_tcp = 1;
        else if (host) {
            fprintf(stderr, "Bad args: %s [-tcp] [host]\n", argv[0]);
            exit(EXIT_FAILURE);
        } else
            host = *argv;
    }

    while (running) {
        /* Bind to Venus and ask it to send us Fetch/Store messages. */
        venusSocket = Bind(host);
        if (venusSocket < 0) {
            fprintf(stderr, "codacon: bind to %s failed, venus not running?\n",
                    host ? host : "localhost");
            sleep(5);
            continue;
        }
        FILE *fp      = fdopen(venusSocket, "r");
        const char *p = "set:fetch\n";
        ssize_t len   = strlen(p);
        if (write(venusSocket, p, len) != len) {
            fprintf(stderr, "codacon: set:fetch command failed (%d)\n", errno);
        } else {
            /* Read until someone kills the connection. */
            CheckMariner(fp);
        }

        fclose(fp);
        close(venusSocket);
    }
}

static int Bind(const char *host)
{
    int s = -1;

#ifdef HAVE_SYS_UN_H
    if (!use_tcp) {
        struct sockaddr_un s_un;
        const char *MarinerSocketPath;

        codaconf_init("venus.conf");
        MarinerSocketPath =
            codaconf_lookup("marinersocket", "/usr/coda/spool/mariner");
        memset(&s_un, 0, sizeof(s_un));
        s_un.sun_family = AF_UNIX;
        strcpy(s_un.sun_path, MarinerSocketPath);

        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            return (-1);
        }
        if (connect(s, (sockaddr *)&s_un, sizeof(s_un)) < 0) {
            close(s);
            return (-1);
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
        if (rc)
            return -1;

        for (p = ai; p != NULL; p = p->ai_next) {
            s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (s < 0)
                continue;

            if (connect(s, p->ai_addr, p->ai_addrlen) == 0)
                break;

            close(s);
            s = -1;
        }
        RPC2_freeaddrinfo(ai);
    }
    return s;
}

static void CheckMariner(FILE *fp)
{
    static char buf[MARINERBUFSIZE];
    static char *fillptr = buf;
    register int c;

    /*    errno = 0;*/
    while ((c = getc(fp)) != EOF) {
        if (c != '\n') {
            *fillptr++ = c;
        }
        if (c == '\n' || fillptr >= buf + sizeof(buf) - 15) {
            --fillptr; /* back off one from the ending NULL */
            while (fillptr != buf && *fillptr == ' ')
                --fillptr;
            if (fillptr != buf)
                ++fillptr;

            time_t curr_time = time(0);
            struct tm *lt    = localtime(&curr_time);
            sprintf(fillptr, " ( %02d:%02d:%02d )", lt->tm_hour, lt->tm_min,
                    lt->tm_sec);
            fillptr += 13;

            *fillptr++ = '\n';
            *fillptr   = '\0';
            CheckTheMariner(buf);
            fillptr = buf;
        }
    }
}

static void CheckTheMariner(char *buf)
{
    char *string, *prefix = NULL;

    /* search for a :: separator between the prefix and the actual message */
    string = strstr(buf, "::");
    if (string) {
        *string = '\0';
        string += 2;
        prefix = buf;

        /* Some prefixes we do not want to display if the remaining string
	 * contains ' done ' */
        if ((strcmp(prefix, "fetch") == 0 || strcmp(prefix, "store") == 0 ||
             strcmp(prefix, "mond") == 0) &&
            strstr(string, " done ") != NULL)
            return;
    } else
        string = buf;

    printf("%s", string);
    fflush(stdout);
}
