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

extern const int timeLength;

extern void InitUserData();
extern void InitDataDirectory();
extern void InitializeDirectory(char *, char *);
extern void CreateDataDirectory(char *);
extern void CreateREADMEFile(char *);
extern void SetAccessLists(char *, char *);
extern int MoveFile(char *, char *, char*);
extern char *GetDataFile(char *);
