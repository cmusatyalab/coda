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


