#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <assert.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#include "globals.h"
#include "helpers.h"
#include "portability_problems.h"


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
	assert(strlen(commandname) < MAXPATHLEN);
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
  assert(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(the_string);
}

char *GetDateFromLong(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  assert(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d/%02d/%02d", lt->tm_mon+1, lt->tm_mday, lt->tm_year);
  return(the_string);
}

char *TimeString(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  assert(lt != NULL);
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




