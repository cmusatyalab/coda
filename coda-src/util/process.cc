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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/Attic/process.cc,v 4.1 1997/01/08 21:51:06 rvb Exp $";
#endif /*_BLURB_*/









/*
 *
 * process.c -- Implementation of process abstraction
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__NetBSD__)
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__

#ifdef __cplusplus
}
#endif __cplusplus

#include "process.h"


process::process(char *n, PROCBODY f) {
    pid = 0;
    name = new char[strlen(n) + 1];
    strcpy(name, n);
    func = f;
}


process::process(process& p) {
    abort();
}


process::operator=(process& p) {
    abort();
    return(0); /* keep C++ happy */
}


process::~process() {
    delete name;
}


void process::print() {
    print(stdout);
}


void process::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void process::print(int fd) {
    char buf[40];
    sprintf(buf, "%#08x : %-16s : pid = %d, func = %#08x\n",
	     (long)this, name, pid, func);
    write(fd, buf, strlen(buf));
}
