/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/



#ifndef _LWP_CAMELOT_
#define _LWP_CAMELOT_

#include    <cthreads.h>

/* Camelot support for LWP package */
typedef void (*CamThreadProc_t) C_ARGS((void *, int));
extern int Camelot_Running;		/* runtime flag for lwp */
extern void Camelot_LWPInit();		/* must be called before LWP_Init */

/* set to CONCURRENT_THREAD before LWP_Init() if Camelot is being used */
extern CamThreadProc_t lwp_camelottp;

#endif _LWP_CAMELOT_
