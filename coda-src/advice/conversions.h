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

#define MAXEVENTLEN 64
#define MAXCOMMANDLEN 8
#define MAXMETALEN 4

extern InterestID GetInterestID(char *);
extern HoardCommandID GetHoardCommandID(char *);
extern MetaInfoID GetMetaInfoID(char *);

extern char *InterestToString(InterestID);
extern char *HoardCommandToString(HoardCommandID);
extern char *MetaInfoIDToString(MetaInfoID);
