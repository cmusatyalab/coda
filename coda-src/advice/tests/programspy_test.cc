

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vcrcommon.h>
#include <venusioctl.h>

#include "mybstree.h"
#include "programspy.h"

char ProfileDir[MAXPATHLEN];

int IsProgramUnderWatch(char *);


int main(int argc, char *argv[]) {
  dataent *d;
  char testprog[128];

  InitUADB();

  ParseDataDefinitions("/coda/usr/mre/newHoarding/Data");
  ParseDataDefinitions("/coda/usr/mre/newHoarding/Data");

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
    ParseProgramDefinitions("/coda/usr/mre/newHoarding/Programs");
    ParseProgramDefinitions("/coda/usr/mre/newHoarding/Programs");

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


    strcpy(ProfileDir, "/coda/usr/mre/newHoarding/ProgramProfiles");
    ProcessProgramAccessLog("/usr/coda/spool/2660/program.log.old");
}
