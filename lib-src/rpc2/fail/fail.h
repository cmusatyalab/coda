/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
  Network failure simulation package

  Walter Smith

 */

#include <netinet/in.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <netdb.h>
#include "fcon.h"

#ifndef MAXNAMELEN
#define MAXNAMELEN	255	 /* Maximum length of a client name */
#endif
#define MAXCLIENTS      999	 /* Maximum number of clients */
#define MAXPROBABILITY	10000	 /* Probabilities are factor/MAXPROBABILITY */
#define MAXNETSPEED     10000000 /* Speed of Ethernet */

#define FAIL_IMMUNECOLOR 255	 /* Default filters ignore packets of this color */

/* Exported routines */

int Fail_Initialize (char *name, long flags);

int Fcon_Init (void); /* Warning: doesn't return anything */

void Fail_GetInfo (char *info);

int Fail_InsertFilter (FailFilterSide side,
			    int which, FailFilter *filter);

int Fail_RemoveFilter (FailFilterSide side,
			    int id);

int Fail_ReplaceFilter (FailFilterSide side,
			     int id, FailFilter *filter);

int Fail_GetFilters (FailFilterSide side,
			  RPC2_BoundedBS *filters);

