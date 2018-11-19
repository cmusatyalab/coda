/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef	_TEST_LWP_H_
#define _TEST_LWP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

/* from util */
#include <util/rvmlib.h>

void LWPInit();
void LWPUnInit();

#endif /* _TEST_LWP_H_ */