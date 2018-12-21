/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef HAVE_INET_ATON
int inet_aton(const char *str, struct in_addr *out)
{
    unsigned long l;
    unsigned int val;
    int i;

    l = 0;
    for (i = 0; *str && i < 4; i++) {
        l <<= 8;
        val = 0;
        while (*str >= '0' && *str <= '9') {
            val *= 10;
            val += *str - '0';
            str++;
        }
        if (*str) {
            if (*str != '.')
                break;
            str++;
        }
        if (val > 255)
            break;
        l |= val;
    }
    if (*str || i != 4)
        return (0);

    out->s_addr = htonl(l);
    return (1);
}
#endif

#ifndef HAVE_INET_NTOA
char *inet_ntoa(struct in_addr in)
{
    static char buff[18];
    char *p;

    p = (char *)&in.s_addr;
    sprintf(buff, "%d.%d.%d.%d", (p[0] & 255), (p[1] & 255), (p[2] & 255),
            (p[3] & 255));
    return (buff);
}
#endif

#ifndef HAVE_INET_PTON
int inet_pton(int af, const char *str, void *dst)
{
    int ret;

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    return inet_aton(str, (struct in_addr *)dst);
}
#endif

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t cnt)
{
    char *buf;

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return NULL;
    }

    buf = inet_ntoa(*(struct in_addr *)src);

    if (cnt < strlen(buf) + 1) {
        errno = ENOSPC;
        return NULL;
    }

    return strcpy(dst, buf);
}
#endif
