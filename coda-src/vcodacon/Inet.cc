/*
    Copyright (C) 2000 Philip A. Nelson

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.  (COPYING.LIB)

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to:

      The Free Software Foundation, Inc.
      59 Temple Place, Suite 330
      Boston, MA 02111-1307 USA.

    You may contact the author by:
       e-mail:  philnelson at acm.org
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "Inet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(WIN32) || defined(__CYGWIN__)

#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#else

#include <winsock.h>

static bool didinit = false;
static WSADATA windata;
static int numinet = 0;

#endif

#if defined(HAVE_SYS_UN_H) && !defined(WIN32)
#include <sys/un.h>
#endif

#if defined(WIN32) || defined(linux) || defined(__CYGWIN__) || \
    defined(__Solaris__)
#define NO_SIN_LEN
#endif

Inet::Inet()
{
    fd        = -1;
    neterr    = 0;
    server    = false;
    remname   = 0;
    remlen    = sizeof(struct sockaddr);
    unixlines = false;
#if defined(WIN32)
    if (!didinit) {
        if (WSAStartup(1 << 1, &windata) != 0) {
            exit(EXIT_SUCCESS);
        }
        didinit = true;
    }
    numinet++;
#endif
};

Inet::~Inet()
{
    if (fd >= 0)
        sock_close(fd);
#if defined(WIN32)
    numinet--;
    if (numinet == 0) {
        WSACleanup();
        didinit = true;
    }
#endif
}

// Open a tcp connection.  Return false on failure.

bool Inet::TcpOpen(char *host, int port)
{
    struct hostent *name_list;
    struct sockaddr_in *addr = (struct sockaddr_in *)&remaddr;

    // Not closed before???
    if (fd != -1) {
        neterr = -1;
        return false;
    }

    // New socket and connection
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        neterr = last_error;
        fd     = -1;
        return false;
    }

    name_list = gethostbyname(host);
    if (name_list == (struct hostent *)0) {
        neterr = last_error;
        sock_close(fd);
        fd = -1;
        return false;
    }

    //addr.  put in addr and port!
    memset(addr, 0, sizeof(struct sockaddr));
    addr->sin_family = AF_INET;
#if !defined(NO_SIN_LEN)
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_port = htons(port);
    memcpy((void *)&addr->sin_addr, name_list->h_addr, sizeof(addr->sin_addr));

    if (connect(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) < 0) {
        neterr = last_error;
        sock_close(fd);
        fd = -1;
        return false;
    }

    // Success!
    remname = strdup(host);
    return true;
}

#if defined(HAVE_SYS_UN_H) && !defined(WIN32)
// Open a tcp connection to a UNIX file socket
bool Inet::TcpOpen(const char *SocketPath)
{
    struct sockaddr_un sock;

    // error checking
    if (strlen(SocketPath) + 1 > sizeof(sock.sun_path)) {
        neterr = ENAMETOOLONG;
        fd     = -1;
        return false;
    }

    // New socket
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        neterr = last_error;
        fd     = -1;
        return false;
    }

    // printf ("TcpOpen fd is %d, SocketPath is '%s'.\n", fd, SocketPath);

    // set up the path
    memset(&sock, 0, sizeof(sock));
    sock.sun_family = AF_UNIX;
    strcpy(sock.sun_path, SocketPath);

    // Now connect
    if (connect(fd, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
        neterr = last_error;
        sock_close(fd);
        fd = -1;
        return false;
    }

    // Success!
    remname = strdup(SocketPath);
    return true;
}
#endif

// tcp server socket.
bool Inet::TcpServer(int port, int qlen)
{
    struct sockaddr_in *addr = (struct sockaddr_in *)&remaddr;

    // Not closed before???
    if (fd != -1) {
        neterr = -1;
        return false;
    }

    // New socket and binding ...
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        neterr = last_error;
        fd     = -1;
        return false;
    }

    memset(addr, 0, sizeof(struct sockaddr));
    addr->sin_family = AF_INET;
#if !defined(NO_SIN_LEN)
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in))) {
        neterr = last_error;
        sock_close(fd);
        fd = -1;
        return false;
    }

    listen(fd, qlen);
    server = true;
    return true;
}

// Accept a new connection ...
bool Inet::Accept(Inet &newn)
{
    struct hostent *name_list;

    if (fd < 0 || !server) {
        newn.neterr = -1;
        return false;
    }

    // close the "new" one if open.
    if (newn.fd != -1)
        newn.Close();

    newn.fd = accept(fd, &newn.remaddr, &newn.remlen);
    if (newn.fd < 0) {
        newn.fd     = -1;
        newn.neterr = last_error;
        return false;
    }

    name_list = gethostbyaddr((char *)&newn.remaddr, newn.remlen, AF_INET);
    if (name_list != 0)
        newn.remname = strdup(name_list->h_name);
    else
        newn.remname = strdup("Look_up_failure");

    newn.server = false;
    return true;
}

// Close the connection
void Inet::Close()
{
    sock_close(fd);
    fd     = -1;
    neterr = 0;
    if (remname != 0)
        delete[] remname;
    server = false;
}

// Read a line of data.  (max characters is length);
// Returns number of characters read.

int Inet::Readline(char *data, int length)
{
    int i = 0;

    if (fd < 0 || server) {
        neterr = -1;
        return -1;
    }

    while (i < length - 1) {
        if (recv(fd, &data[i], 1, 0) != 1) {
            neterr  = last_error;
            data[i] = 0;
            return i;
        }
        if (unixlines)
            if (data[i] == '\n') {
                data[i] = 0;
                return i;
            } else if (i > 0 && data[i] == '\n' && data[i - 1] == '\r') {
                data[i - 1] = 0;
                return i - 1;
            }
        i++;
    }
    data[length] = 0;
    return length - 1;
}

// Write a string.
int Inet::Write(char *data)
{
    int num;

    if (fd < 0 || server) {
        neterr = -1;
        return -1;
    }
    num = send(fd, data, strlen(data), 0);
    if (num < 1) {
        neterr = last_error;
        return num;
    }
    return num;
}

// Write an integer
int Inet::Write(int data)
{
    char val[20];
    sprintf(val, "%d", data);
    return Write(val);
}

// Write a line of data.  (adds the \r \n.)  String must be null terminated.
int Inet::Writeline(char *data)
{
    if (fd < 0 || server) {
        neterr = -1;
        return -1;
    }
    if (unixlines)
        return Write(data) + Write("\n");
    else
        return Write(data) + Write("\r\n");
}
