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


extern void toUpper(char *);
extern char *GetCommandName(int);
extern char *GetStringFromTimeDiff(long);
extern char *GetTimeFromLong(long);
extern char *GetDateFromLong(long);
extern char *TimeString(long);


extern void ErrorReport(char *);
extern void PrintCounters();


extern void path(char *, char *, char *);
