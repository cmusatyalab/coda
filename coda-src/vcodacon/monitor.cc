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

// Monitor of the codacon port ....

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <codaconf.h>

#include "monitor.h"
#include "util.h"
#include "vcodacon.h"

#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <FL/fl_ask.H>

// "Static" acces
static monitor *TheMon = NULL;

// Call back routines for add_fd and add_timeout
static void GetNextLine(int fd, void *isnull) 
{
  TheMon->NextLine();
}

static void TryAgain(void *)
{
  TheMon->Start();
}

static void AgeColor(void *)
{
  TheMon->AgeActColor();
}

static void ClearXfer(void *data)
{
  int ix = (int)data;
  if (!XferProg[ix]->active() && XferProg[ix]->visible())
    XferProg[ix]->hide();
}

// Run to start at the beginning of the application
void monitor::Start(void)
{
  char *marinerport;

  // printf ("monitor::Start\n");
  if (TheMon == NULL) {
    codaconf_init("venus.conf");
    TheMon = this;
  }

  marinerport = codaconf_lookup("marinersocket", "/usr/coda/spool/mariner");

  if (!conn.isOpen()) {
#if  defined(HAVE_SYS_UN_H) && !defined(WIN32)
    if (!conn.TcpOpen (marinerport))
#endif
      if (!conn.TcpOpen ("localhost", CODACONPORT)) {
	// Not open ..  register for time out.
	VConn->color(FL_RED);
	VConn->redraw();
	// printf ("codacon not opened ...\n");
	Fl::add_timeout (5.0, TryAgain, NULL);
	return ;
      }
  
    // Set up things properly
    
    Fl::add_fd (conn.FileNo(), FL_READ, GetNextLine);
    VConn->color(FL_GREEN);
    VConn->redraw();
    VAct->color( (Fl_Color)actcolor );
    VAct->redraw();
    conn.SetUnix();
    conn.Write("set:fetch\n");
  }
}
  
// Anytime we can read something, go here to get a line;
void monitor::NextLine() {
  char inputline [LINESIZE];
  int  rdsize;
  static int index = 0;

  if (conn.isOpen()) {

    inputline[0] = 0;
    rdsize = conn.Readline (inputline, LINESIZE);
    if (rdsize > 0) {
    
      /* here is where to process the line */
      // printf ("vis: %s\n", inputline);

      // Update the browser
      if (!strstr(inputline,"fetch::fetch done")) {
	codacontext->add(inputline,NULL);
	int s = codacontext->size();
	if (s > browsersize) {
	  codacontext->remove(1);
	  s--;
	}
	codacontext->bottomline(s);
      }

      // Update the activity color
      if (actcolor == 255)
	Fl::add_timeout (10, AgeColor, NULL);
      actcolor=248;
      VAct->color((Fl_Color) actcolor); 
      VAct->redraw();

      // Now figure out what is the line
      if (strstr(inputline, "progress::fetching")) {
	char *name = strchr(inputline, '(');
	name++;
        char *num = strrchr(name, ')');
        char *pc = strrchr(num, '%');
	*num = 0;
	num+= 2;
	*pc = 0;
	int percent = atoi(num);

	// printf ("Fetch: file='%s' percent=%d\n", name, percent);

	// Update proper progress bar
	int ix; 
        for (ix = 0; ix < 3; ix++)
	    if (XferProg[ix]->active() && strcmp(name, XferLabel[ix]) == 0)
		break;
	if (ix == 3)
	  for (ix = 0; ix < 3; ix++)
	      if (!XferProg[ix]->visible())
		  break;
	if (ix == 3) {
	    ix = index++;
	    if (index == 3)
		index = 0;
	}
	// Found the right one
	if (!XferProg[ix]->active()) {
	  if (XferLabel[ix])
	    free(XferLabel[ix]);
	  XferLabel[ix] = strdup(name);
	  XferProg[ix]->label(XferLabel[ix]);
	  XferProg[ix]->activate();
	  if (!XferProg[ix]->visible())
	    XferProg[ix]->show();
	}
	XferProg[ix]->value(percent);

	if (percent == 100) {
	  // schedule clearing of it!
	  XferProg[ix]->deactivate();
	  Fl::add_timeout(10, ClearXfer, (void *)ix);
	}
      }

      else if (strstr(inputline, "cache::Begin")) {
	VWalk->color(FL_YELLOW);
	VWalk->redraw();
      }

      else if (strstr(inputline, "cache::End")) {
	VWalk->color(FL_WHITE);
	VWalk->redraw();
      }

      else if (strstr(inputline, "shutdown in progress")
	       || strstr(inputline, "zombie state:")) {
	ForceClose();
      }

      else if (strstr(inputline, "store::Store")) {
	if (StoreCount++ == 0) {
	  VStore->color(FL_CYAN);
	  VStore->redraw();
	}
      }

      else if (strstr(inputline, "store::store done")) {
	if (--StoreCount == 0) {
	  VStore->color(FL_WHITE);
	  VStore->redraw();
	}
	// Just make sure it never goes negative
	if (StoreCount < 0)
	  StoreCount = 0;
      }

      else if (strstr(inputline, "store::Reintegrate")) {
        if (ReintCount++ == 0) {
          VReint->color(FL_CYAN);
          VReint->redraw();
        }
      }

      else if (strstr(inputline, "store::reintegrate done")) {
        if (--ReintCount == 0) {
          VReint->color(FL_WHITE);
          VReint->redraw();
        }
	// Just make sure it never goes negative
	if (ReintCount < 0)
	  ReintCount = 0;
      }

      else if (strstr(inputline, "pattern to look for")) {
      }

      else if (strstr(inputline, "pattern to look for")) {
      }


    }
  } else {
    /* need to try to restart it ... */
    printf ("vis: conn got closed!\n");
    VConn->color(FL_RED);
    VConn->redraw();
    Fl::add_timeout (1.0, TryAgain, NULL);
  }
}


void monitor::ForceClose()
{
  /* need to try to restart it ... */

  printf ("ForceClose closed!\n");
  conn.Close();
  VConn->color(FL_RED);
  VConn->redraw();

  /* anything else? */

}

void monitor::AgeActColor()
{
  if (actcolor == 255)
    return;

  actcolor++;
  VAct->color((Fl_Color) actcolor); 
  VAct->redraw();
  Fl::add_timeout (10, AgeColor, NULL);
}
