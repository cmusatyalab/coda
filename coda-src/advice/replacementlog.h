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

#define TMPREPLACELIST "/tmp/replacelist"
#define MAXSTATUSREPLACEMENTS 10
#define MAXDATAREPLACEMENTS 10

extern void ParseReplacementLog(char *);
extern void PrintGhostDB();
extern void OutputReplacementStatistics();
extern int Find(char *);

extern char GhostDB[];
extern struct Lock GhostLock;
