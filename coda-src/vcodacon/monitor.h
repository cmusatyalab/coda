/* 	$Id: monitor.h,v 1.3 2005/09/09 22:02:24 phil Exp $	*/

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

// Monitor of the codacon port, definition

#ifndef MONITOR_CXX
#define MONITOR_CXX

#include "config.h"

#include "Inet.h"

#define CODACONPORT	2430
#define LINESIZE	1024

class monitor {

  public:

    monitor() {
      actcolor = 255;
      browsersize = 100;
      StoreCount = 0;
      ReintCount = 0;
      DisFsCount = 0;
    }

    void Start();

    void NextLine();

    void ForceClose();

    void AgeActColor();

    void SetBrowserSize (int x) {
      if (x > 10)
	browsersize = x;
    }

  private:
    Inet conn;
    int actcolor;
    int browsersize;

    int StoreCount;
    int ReintCount;
    int DisFsCount;
};

void NextLine(void);

#endif
