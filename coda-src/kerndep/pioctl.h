/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *  TODO:  combine ViceIoctl and PioctlData structures.
 */

#ifndef _PIOCTL_H_
#define _PIOCTL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <time.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif
#include <coda.h>

/* WARNING: don't send more data than allowed by the CFS_MAXMSG size
in coda.h */

#define PIOCTL_PREFIX "...PIOCTL."

int pioctl(const char *path, unsigned long com, struct ViceIoctl *vidata,
           int follow);

#if defined(__APPLE__) && defined(__MACH__)
/*
 * These are absent from Darwin's <sys/ioccom.h>.
 */
#define _IOC_NRBITS 8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS 2

#define _IOC_NRMASK ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK ((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT 0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT (_IOC_SIZESHIFT + _IOC_SIZEBITS)

/* used to decode ioctl numbers.. */
#define _IOC_DIR(nr) (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr) (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

#endif /* Darwin */

#if defined(__CYGWIN32__)
/* Get the _IO... definitions for CYGWIN. */
#include <asm/socket.h>
#endif

/* people who understand ioctling probably know why this is useful... */
#define _VICEIOCTL(id) (_IOW('V', id, struct ViceIoctl))
#define _VALIDVICEIOCTL(com) (com >= _VICEIOCTL(0) && com <= _VICEIOCTL(255))

/* unpacking macros */
#ifndef _IOC_NR

#if defined(__NetBSD__) || defined(__FreeBSD__)
#define _IOC_TYPEMASK 0xff
#define _IOC_TYPESHIFT 8
#define _IOC_NRMASK 0xff
#define _IOC_NRSHIFT 0
#endif

#define _IOC_TYPE(nr) ((nr >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr) ((nr >> _IOC_NRSHIFT) & _IOC_NRMASK)
#endif

#endif /* _PIOCTL_H_ */
