/* 	$Id$	*/

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
*/

// Utility routines ... i.e. don't want inline code in the gui.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vcodacon.h"
#include "util.h"
#include <FL/fl_ask.H>
#include <sys/stat.h>

char *XferLabel[3];
  
void MainInit (int *argcp, char ***argvp)
{

  // Initialize the visual tool
  // for (int i=0; i<8; i++) Vol[i]->hide();
  for (int i=0; i<3; i++) {
      XferLabel[i] = NULL;
      XferProg[i]->hide();
  }
  // VDisConn->hide();
  // VDisConn->color(FL_YELLOW);
  // VConfl->hide();
  // VConfl->color(FL_RED);
}


//  Do the clog ...
void do_clog(const char *user, const char *pass)
{
  char cmd [255];
  FILE *p; 
  int  stat;

  snprintf (cmd, 255, "clog -pipe %s", user);
  p = popen(cmd, "w");

  if (!p) {
    fl_alert ("could not start clog");
    return;
  }

  fprintf (p, "%s\n", pass);
  stat = pclose(p);

  if (stat)
    fl_alert ("clog failed");
}

void do_ctokens(void)
{
  FILE *p;
  char line[100];
  char cmd[100];

  snprintf (cmd, 100, "ctokens");  
  p = popen(cmd,"r");
  if (!p) {
    fl_alert ("could not start ctokens");
    return ;
  }
  TokenList->clear();
  CTokens->show();
  while (fgets(line, 100, p))
    TokenList->add(line,NULL);
  pclose(p);
  TokenList->show();
  TokenList->redraw();
}

// Stat /coda/name to find a realm

int do_findRealm (const char *realm)
{
  char fullname[258];
  int realmlen = strlen(realm);

  struct stat sb;
  int rv;

  if (realmlen > 250) {
    fl_alert ("%s: name too long", realm);
    return 0;
  }

  snprintf (fullname, 258, "/coda/%s", realm);
  
  rv = stat ( fullname, &sb );

  if (rv < 0 || ((sb.st_mode & S_IFMT) != S_IFDIR))
    return 0;

  return 1;
}
