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

extern "C" {
#include <stdio.h>
#include <lock.h>
#include <util.h>
}


#include "../replacementlog.h"

FILE *LogFile;
int LogLevel = 0;

#define GHOSTDB "/usr/coda/spool/2660/ghostdb"

Yield() {

}

main() {

  LogFile = stdout;
  LogLevel = 100;

  strcpy(GhostDB, GHOSTDB);

  Lock_Init(&GhostLock);


  //  ParseReplacementLog("/usr/coda/spool/2660/replacement.log.old");

  PrintGhostDB();

  Find("/coda/platforms/win16/CD/WIN/l500_493.wp_");
  Find("/coda/usr/mre");
  Find("/coda/usr/mre/personal/BF/bf-Dec92.tex");
  Find("/coda/info/laptop_setup/hosts/rinald");
  Find("/coda/platforms/win16/windows/winini.wri");
  Find("/coda/platforms/win16/wabi/lib/locale/es/wabi/wabitrb.hlp");
  Find("/coda/platforms/win16/CD/WABI/Doc/html/start.htm");

  printf("\n");
  OutputReplacementStatistics();
}
