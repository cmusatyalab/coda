#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/






/*
  Network failure simulation package

  Walter Smith

 */

#include <lwp.h>
#include <rpc2.h>
#include "fcon.h"

#include "cargs.h"

#define MAXNAMELEN	255	 /* Maximum length of a client name */
#define MAXCLIENTS      999	 /* Maximum number of clients */
#define MAXPROBABILITY	10000	 /* Probabilities are factor/MAXPROBABILITY */
#define MAXNETSPEED     10000000 /* Speed of Ethernet */

#define FAIL_IMMUNECOLOR 255	 /* Default filters ignore packets of this color */

/* Exported routines */

int Fail_Initialize C_ARGS((char *name, long flags));

int Fcon_Init C_ARGS((void)); /* Warning: doesn't return anything */

void Fail_GetInfo C_ARGS((char *info));

int Fail_InsertFilter C_ARGS((FailFilterSide side,
			    int which, FailFilter *filter));

int Fail_RemoveFilter C_ARGS((FailFilterSide side,
			    int id));

int Fail_ReplaceFilter C_ARGS((FailFilterSide side,
			     int id, FailFilter *filter));

int Fail_GetFilters C_ARGS((FailFilterSide side,
			  RPC2_BoundedBS *filters));

