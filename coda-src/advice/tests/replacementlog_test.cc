extern "C" {
#include <stdio.h>
#include <lock.h>
}

#include "replacementlog.h"

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
