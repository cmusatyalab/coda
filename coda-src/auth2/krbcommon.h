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

#*/

/* krbcommon.h -- common support support routines for kerberos */

#ifndef _KRBCOMMON_H_
#define _KRBCOMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

char *krb_canonicalize_host(const char *host);
void krb_fork_kinit(const char *kinit);

#ifdef __cplusplus
}
#endif

#endif /* _KRBCOMMON_H_ */
