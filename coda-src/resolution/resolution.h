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
                           none currently
#*/

#ifndef _RESOLUTION_H_
#define _RESOLUTION_H_

#ifdef __cplusplus
extern "C" {
#endif

void ResCheckServerLWP(void *);
void ResCheckServerLWP_worker(void *);

#ifdef __cplusplus
}
#endif

#endif /* _RESOLUTION_H_ */
