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

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include "coda_assert.h"
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from util */
#include <util.h>
#include <proc.h>

#include "globals.h"
#include "helpers.h"


/*******************************************************************************************
 *
 *  Conversion Routines:
 *	toUpper -- takes a string and converts all lower case letters to upper case
 *		   (overwrites original string).
 *	GetCommandName -- given a process id number, returns the name of the command, or "Unknown"
 *	GetStringFromTimeDiff -- given a time difference in seconds, returns a string
 *				 describing that difference in days, hours, minutes and seconds
 *                               as appropriate
 *	GetTimeFromLong -- given a time, returns a time in hh:mm:ss format
 *      GetDateFromLong -- given a time, retunrs a date in mm/dd/yy format
 *      TimeString -- given a time, returns date and time in mm/dd/yy hh:mm:ss format
 *	
 *******************************************************************************************/

void toUpper(char *s) {
    int i;

    for (i = 0; i < strlen(s); i++) {
        if (islower(s[i]))
            s[i] = toupper(s[i]);
    }
}

char *GetCommandName(int pid) {
    char *commandname;
    static char CommandName[MAXPATHLEN];

    commandname = getcommandname(pid);
    if (commandname == NULL) 
	snprintf(CommandName, MAXPATHLEN, "Unknown");
    else if (strcmp(commandname, "") == 0) 
	snprintf(CommandName, MAXPATHLEN, "Unknown");
    else {
	CODA_ASSERT(strlen(commandname) < MAXPATHLEN);
	snprintf(CommandName, MAXPATHLEN, "%s", commandname);
    }
    return(CommandName);
}

char *GetStringFromTimeDiff(long time_difference) {
    long seconds, minutes, hours, days;
    static char the_string[smallStringLength];

    if (time_difference < 60) {
        snprintf(the_string, smallStringLength, "%d second%s", 
		 time_difference, (time_difference>1)?"s":"");
        return(the_string);
    }

    minutes = time_difference / 60;  // Convert to minutes
    seconds = time_difference % 60;
    if (minutes < 60) {
	if (seconds > 0)
	    snprintf(the_string, smallStringLength, "%d minute%s %d second%s", 
		     minutes, (minutes>1)?"s,":",", seconds, (seconds>1)?"s":"");
        else
            snprintf(the_string, smallStringLength, "%d minute%s", 
		     minutes, (minutes>1)?"s":"");
        return(the_string);
    }
 
    hours = minutes / 60;  // Convert to hours
    minutes = minutes % 60;
    if (hours < 24) {
	if (minutes > 0)
	    snprintf(the_string, smallStringLength, "%d hour%s, %d minute%s", 
		     hours, (hours>1)?"s":"", minutes, (minutes>1)?"s":"");
        else
	    snprintf(the_string, smallStringLength, "%d hour%s", 
		     hours, (hours>1)?"s":"");
        return(the_string);
    }

    days = hours / 24;  // Convert to days
    hours = hours % 24;
    if (hours > 0)
	snprintf(the_string, smallStringLength, "%d day%s, %d hour%s", 
		 days, (days>1)?"s":"", hours, (hours>1)?"s":"");
    else
	snprintf(the_string, smallStringLength, "%d day%s", 
		 days, (days>1)?"s":"");
    return(the_string);
}

char *GetTimeFromLong(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  CODA_ASSERT(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(the_string);
}

char *GetDateFromLong(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  CODA_ASSERT(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d/%02d/%02d", lt->tm_mon+1, lt->tm_mday, lt->tm_year);
  return(the_string);
}

char *TimeString(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  CODA_ASSERT(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d/%02d/%02d %02d:%02d:%02d", lt->tm_mon+1, lt->tm_mday, lt->tm_year, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(the_string);
}


/*******************************************************************************************
 *
 *  Miscellaneous Routines:
 *	ErrorReport -- logs an error message and exists
 *
 *******************************************************************************************/

void ErrorReport(char *message)
{
  LogMsg(0,LogLevel,LogFile, "%s", message);
  exit(1);
}



/************************************************************ 
 *
 *  void path(char *pathname, char *directory, char *file);
 *
 ************************************************************/


/* An implementation of path(3), which is a standard function in 
 * Mach OS.  The behaviour, according to Mach man page, is:
 *
 *    The handling of most names is obvious, but several special
 *    cases exist.  The name "f", containing no slashes, is split
 *    into directory "." and filename "f".  The name "/" is direc-
 *    tory "/" and filename ".".  The path "" is directory "." and
 *    filename ".".
 *       -- manpage of path(3)
 */

void path(char *pathname, char *direc, char *file)
{
  char *maybebase, *tok;
  int num_char_to_be_rm;

  if (strlen(pathname)==0) {
    strcpy(direc, ".");
    strcpy(file, ".");
    return;
  }
  if (strchr(pathname, '/')==0) {
    strcpy(direc, ".");
    strcpy(file, pathname);
    return;
  } 
  if (strcmp(pathname, "/")==0) {
    strcpy(direc, "/");
    strcpy(file, ".");
    return;
  }
  strcpy(direc, pathname);
  maybebase = strtok(direc,"/");
  while (tok = strtok(0,"/")) 
    maybebase = tok;
  strcpy(file, maybebase);
  strcpy(direc, pathname);
  num_char_to_be_rm = strlen(file) + 
    (direc[strlen(pathname)-1]=='/' ? 1 : 0);/* any trailing slash ? */
  *(direc+strlen(pathname)-num_char_to_be_rm) = '\0';
    /* removing the component for file from direc */
  if (strlen(direc)==0) strcpy(direc,"."); /* this happen when pathname 
                                            * is "name/", for example */
  if (strlen(direc)>=2) /* don't do this if only '/' remains in direc */
    if (*(direc+strlen(direc)-1) == '/' )
      *(direc+strlen(direc)-1) = '\0'; 
       /* remove trailing slash in direc */
  return;
}



