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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/advice_srv.h,v 4.2 1997/07/15 14:20:11 lily Exp $";
#endif /*_BLURB_*/




// Advice monitor version information
#define ADVICE_MONITOR_VERSION 1

/*****  Definitions  *****/
#define FALSE 0
#define TRUE 1
#define DFTSTACKSIZE  16            /* 8 k is the default stacksize for lwp's */
#define STARTASR_INTERVAL  300   /* Number seconds before asr can be retired */

/* Definitions:  Pathnames to other executables... */
#define JUMPSTARTASR "/usr/coda/etc/parser"        
#define RECONNECTION_SURVEY "/usr/coda/etc/reconnection"
#define DISCOMISS_SURVEY "/usr/coda/etc/discomiss"
#define HOARDLIST "/usr/coda/etc/hoardlist"
#define USERINITIATED "/usr/coda/etc/user_initiated"
#define REINT_PENDING "/usr/coda/etc/reintegration_pending"
#define WEAKMISS "/usr/coda/etc/weakmiss"
#define READMISS "/usr/coda/etc/readmiss"
#define STOPLIGHT "/usr/coda/etc/stoplight"
#define STOPLIGHT_STATECHANGE "/usr/coda/etc/stoplight_statechange"

#define CFS "/usr/coda/etc/cfs"

/* Environment variables */
/*
 * #define TCL "/usr/misc/.tcl/lib/tcl"
 * #define TK "/usr/misc/.tcl/lib/tk"
 */
#define TCL "/usr/coda/tcl/lib/tcl"
#define TK "/usr/coda/tcl/lib/tk"


extern int LogLevel;
extern FILE *LogFile;

extern int execute_tcl(char *script, char *args[]);

/* Stuff related to user-initiated requests. */
#define LINELENGTH 256
#define CommandFileName "/tmp/advice_srv_command_file"
#define SolicitHoardAdvice 'a'
#define UnsolicitHoardAdvice 'b'
#define RequestMissList 'c'
#define RequestLongFetchQuery 'd'
#define BeginStoplight 'e'
#define EndStoplight 'f'
#define RequestDiscoMissQs 'g'
#define UnrequestDiscoMissQs 'h'
#define UnrequestLongFetchQuery 'i'
#define QuitAdvice 'q'


/* Monitor Definitions */
enum StoplightStates { SLunknown, SLdisconnect, SLweak, SLstrong, SLoff, SLquit };
#define VDBFileName "/tmp/advice_vdb.out"




