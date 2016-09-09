/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODA_DEPRECATIONS_H_
#define _CODA_DEPRECATIONS_H_ 1

/*
 * Use this to label any variable or function that is relies on a single
 * static server IP addresses (ThisHostAddr / HostAddress array).
 *
 * Ideally these can be either 'optimized away' or switched to using the
 * server/port names (Server_FQDN array).
 */
#define WARN_SINGLE_HOMING __attribute__ ((deprecated ("Prevents having multihomed servers")))

#endif  /* _CODA_DEPRECATIONS_H_ */

