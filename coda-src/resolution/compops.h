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

#ifndef _COMPOPS_H_
#define _COMPOPS_H_ 1
// compops.h

#include <olist.h>
#include <arrlist.h>
#include <vcrcommon.h>

extern void PrintCompOps(arrlist *);
extern arrlist *ComputeCompOps(olist *, ViceFid *);

#endif /* _COMPOPS_H_ */
