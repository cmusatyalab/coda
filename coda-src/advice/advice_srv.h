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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/advice/Attic/advice_srv.h,v 4.6 98/01/29 20:00:27 braam Exp $";
#endif /*_BLURB_*/



// Advice monitor version information
#define ADVICE_MONITOR_VERSION 1

/*****  Definitions  *****/
#define FALSE 0
#define TRUE 1
#define DFTSTACKSIZE  16            /* 8 k is the default stacksize for lwp's */
#define STARTASR_INTERVAL  300   /* Number seconds before asr can be retired */

/* Definitions:  Pathnames to other executables... */
#define CODACONSOLEOUT "tixwish"
#define JUMPSTARTASR "parser"

#define CFS "cfs"

#define UserVolume "/coda/usr"
#define UsernameEnvironmentVariable "USER"
#define HoardDir "newHoarding"
#define DataFile "Data"
#define ProgramFile "Programs"
#define ProfileDirectoryName "ProgramProfiles"
#define GhostDBName "ghostdb"
#define CodaSpoolingArea "/usr/coda/spool"

extern int LogLevel;
extern FILE *LogFile;

extern struct Lock VenusLock;

extern int execute_tcl(char *script, char *args[]);

/* Monitor Definitions */
enum StoplightStates { SLunknown, SLdisconnect, SLweak, SLstrong, SLoff, SLquit };
#define VDBFileName "/tmp/advice_vdb.out"

#define TMPDIR "/tmp"

extern void Yield();


