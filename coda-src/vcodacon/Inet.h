/*	$Id: Inet.h,v 1.3 2006-05-24 20:04:40 jaharkes Exp $	*/

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

#ifndef INET_H
#define INET_H

#include <config.h>

#ifndef WIN32

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define sock_close close
#define last_error errno

#if defined(__CYGWIN__) || defined(__Solaris__)
#define REMLENTYPE int
#else
#define REMLENTYPE unsigned int
#endif

#else

#include <winsock.h>

#define sock_close closesocket
#define last_error WSAGetLastError()
#define ENAMETOOLONG WSAENAMETOOLONG

#define REMLENTYPE int

#endif

#if defined(HAVE_SYS_UN_H) && !defined(WIN32)
#include <sys/un.h>
#endif


class Inet {

  public:

    // Constructor, destructor ...
    Inet();
    ~Inet();

    // Weird assignment ... moves data and invalidates right side.
    Inet &operator = (Inet &From)
      { // Copy ...
	fd      = From.fd;
	neterr  = From.neterr;
	server  = From.server;
	remname = From.remname;
	remaddr = From.remaddr;
	remlen  = From.remlen;
	unixlines = From.unixlines;
	// Invalidate from ...
	From.fd = -1;
	From.remname = (char *)0;
	return *this;
      }

    // Open a tcp connection.  Return false on failure.
    bool TcpOpen (char *host, int port);

#if defined(HAVE_SYS_UN_H) && !defined(WIN32)
    bool TcpOpen (const char *socketpath);
#endif

    // Open a tcp server socket.  Return false on failure.
    bool TcpServer (int port, int qlen);

    // Accept a connect and return a new Inet
    bool Accept (Inet &newn);

    // Close the connection
    void Close ();

    // Read a line of data.  (max characters is length);
    // Returns number of characters read.
    int Readline (char *data, int length);

    // Write a line of data.  (adds the \r \n.)
    int Writeline (char *data);

    // Write characters and integers ...
    int Write (char *data);
    int Write (int data);

    // isOpen -- is a network connection open.
    bool isOpen() { return fd >= 0;} ;

    // ErrNo -- the error number
    int ErrNo () { return neterr; }

    // FileNo -- the file number
    int FileNo () { return fd; }

    // RemoteName -- the host name of the remote machine
    char *RemoteName () { return remname; }

    // RemoteAddr -- the text address form of the remote machine
    char *RemoteAddr () {
      struct sockaddr_in *addr = (struct sockaddr_in*)&remaddr;
      return inet_ntoa(addr->sin_addr);
    }

    // Options set ..
    void SetUnix () { unixlines = true; }
    void SetNet () { unixlines = false; }

  private:
    int fd;
    int neterr;
    bool server;
    char *remname;
    struct sockaddr remaddr;
    REMLENTYPE remlen;

    bool unixlines;

};

#endif

