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



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <struct.h>

#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vcrcommon.h>
#include <venusioctl.h>

#include "../mybstree.h"
#include "../programspy.h"

//char ProfileDir[MAXPATHLEN];
extern char ProfileDir[];

int IsProgramUnderWatch(char *);

void Yield() {
    printf("Pretending to Yield\n");
}


int main(int argc, char *argv[]) {
  dataent *d;
  char testprog[128];

  InitUADB();

  ParseDataDefinitions("./Data");
  ParseDataDefinitions("./Data");

  PrintUADB("/tmp/dataareas.foo");

  
  if (IsProgramAccessingUserArea(0x7f0003ed))
    printf("Is dataarea\n");
  else 
    printf("Is not dataarea\n");

  if (IsProgramAccessingUserArea(0x7f0003ef))
    printf("Is dataarea\n");
  else 
    printf("Is not dataarea\n");

  if (IsProgramAccessingUserArea(233))
    printf("Is dataarea\n");
  else 
    printf("Is not dataarea\n");
  
    InitPWDB();
    ParseProgramDefinitions("./Programs");
    ParseProgramDefinitions("./Programs");

    PrintPWDB("/tmp/programs.foo");

    strcpy(testprog, "gnu-emacs");
    if (IsProgramUnderWatch(testprog))
        printf("watching %s\n", testprog);
    else
      printf("not watching %s\n", testprog);

    strcpy(testprog, "gnufoo");
    if (IsProgramUnderWatch(testprog))
        printf("watching %s\n", testprog);
    else
      printf("not watching %s\n", testprog);

    strcpy(testprog, "virtex");
    if (IsProgramUnderWatch(testprog))
        printf("watching %s\n", testprog);
    else
      printf("not watching %s\n", testprog);


    strcpy(ProfileDir, "./ProgramProfiles");
    ProcessProgramAccessLog("/usr/coda/spool/2660/program.log.old", "/tmp");
}
