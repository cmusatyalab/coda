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

/*
 *
 *    Specification of the Venus Signal Handler facility.
 *
 */

#ifndef _VENUS_SIGHAND_H_
#define _VENUS_SIGHAND_H_ 1

void SigInit(void);
extern int TerminateVenus;
extern int mount_done;

#endif /* _VENUS_SIGHAND_H_ */
