/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

extern char *getcommandname(int);

FILE *LogFile = stdout;
int LogLevel = 1000;

int main(int argc, char *argv[]) {
  int pid = getpid();
    printf("CommandName associated with PID=%d is %s\n", 
	   pid, getcommandname(pid));
    fflush(stdout);
}
