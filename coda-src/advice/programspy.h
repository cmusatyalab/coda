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

class progent {
    friend int ProgramPriorityFN(bsnode *, bsnode *);
    friend void PrintPWDB(char *);
    friend int IsProgramUnderWatch(char *);

  
    char *program;
    bsnode queue_handle;  /* link for the program queue */

  public:
    progent(char *Program);
    progent(progent&);
    int operator=(progent&);
    ~progent();

    void print(FILE *f);
};

class dataent {
    friend int DataAreaPriorityFN(bsnode *, bsnode *);
    friend void PrintUADB(char *);
    friend int IsProgramAccessingUserArea(VolumeId volume);

    VolumeId volume;
    bsnode queue_handle;

  public:
    dataent(VolumeId vol);
    dataent(dataent&);
    int operator=(dataent&);
    ~dataent();

    void print(FILE *f);
};

extern void InitPWDB();
extern void InitUADB();

extern void ParseProgramDefinitions(char *);
extern void ParseDataDefinitions(char *);
extern void ProcessProgramAccessLog(char *, char *);

