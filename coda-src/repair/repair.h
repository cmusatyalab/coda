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
    Repair data structures and routines used only on client.
    The server component of repair does not know of these.
    Data structures shared by client and server are defined in repio.{h,c}

    Created:
	M. Satyanarayanan
	October 1989	
*/

#ifndef _REPAIR_H_
#define _REPAIR_H_

#include "repcmds.h"

#define NOT_IN_SESSION 0
#define	LOCAL_GLOBAL   1
#define SERVER_SERVER  2

extern struct repvol *RepairVol; /* volume under repair */
extern int allowclear, interactive, repair_DebugFlag, session;

int checklocal(char *arg);
int checkserver(char *arg);
void GetArgs(int argc, char *argv[]);
int  getcompareargs(int, char **, char **, char **, char **, char **, char **);
int  getrepairargs(int, char **, char *);
int  GetTokens(void);
void INT(int, int, struct sigcontext *);

/* User-visible parser commands (possibly interactive) */
void rep_BeginRepair     (int largc, char **largv);
void rep_CheckLocal      (int largc, char **largv);
void rep_ClearInc        (int largc, char **largv);
void rep_CompareDirs     (int largc, char **largv);
void rep_DiscardLocal    (int largc, char **largv);
void rep_DiscardAllLocal (int largc, char **largv);
void rep_DoRepair        (int largc, char **largv);
void rep_EndRepair       (int largc, char **largv);
void rep_Exit            (int largc, char **largv);
void rep_Help            (int largc, char **largv);
void rep_ListLocal       (int largc, char **largv);
void rep_PreserveLocal   (int largc, char **largv);
void rep_PreserveAllLocal(int largc, char **largv);
void rep_RemoveInc       (int largc, char **largv);
void rep_ReplaceInc      (int largc, char **largv);
void rep_SetGlobalView   (int largc, char **largv);
void rep_SetLocalView    (int largc, char **largv);
void rep_SetMixedView    (int largc, char **largv);

#define INITHELPMSG 	\
"This repair tool can be used to manually repair server/server \n\
or local/global conflicts on files and directories. \n\
You will first need to do a \"beginrepair\" to start a repair\n\
session where messages about the nature of the conflict and\n\
the commands that should be used to repair the conflict will\n\
be displayed. Help message on individual commands can also be\n\
obtained by using the \"help\" facility. Finally, you can use the\n\
\"endrepair\" or \"quit\" to terminate the current repair session.\n"

#endif
