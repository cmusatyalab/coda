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




/* this file contains local-repair daemon code */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

#include <vcrcommon.h>
#include "vproc.h"
#include "venusrecov.h"
#include "venus.private.h"
#include "local.h"
#include "adv_daemon.h"

/* file is obsolete -- Adam 7/8/05 */
