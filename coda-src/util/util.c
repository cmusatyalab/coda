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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "coda_string.h"
#include <time.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <stdarg.h>

#include "util.h"
#ifdef __cplusplus
}
#endif __cplusplus


/* This is probably not the right place for these globals.
   But this works, and I am too tired to figure out a better
   home for them right now (Satya 2/11/92).
   They are all used in conjunction with LogMsg() and extern
   declarations for them are in util.h
*/
int SrvDebugLevel = 0;		/* Server */
int VolDebugLevel = 0;		/* Vol package */
int SalvageDebugLevel = 0;	/* Salvager */
int DirDebugLevel = 0;		/* Dir package */
int AL_DebugLevel = 0;		/* ACL package */
int AuthDebugLevel = 0;		/* Auth package */

extern int CaseFoldedCmp(char *s1, char *s2)
    /* same as strcmp() except that case differences are ignored */
{
	register int i;
	register char c1, c2;
    
	i = -1;
	do  {
		/* Inv: s1[0:i] is identical to s2[0:i] and i is smaller than 
		   the shorter of s1 and s2 */
		i += 1;
		c1 = s1[i];
		if (islower(c1)) c1 -= ('a' - 'A');
		c2 = s2[i];
		if (islower(c2)) c2 -= ('a' - 'A');
		if (c1 == 0 && c2 ==0) return(0);
	} while(c1 == c2);

	if (c1 < c2) 
		return (-1);
	else 
		return(1);
}

int SafeStrCat(char *dest, char *src, int totalspace)
{
    if (strlen(dest)+strlen(src) >= totalspace) 
	    return(-1);
    strcat(dest,src);
    return(0);
}

int SafeStrCpy(char *dest, char *src, int totalspace)
{
    if (strlen(src) >= totalspace) return(-1);
    strcpy(dest,src);
    return(0);
}


int HashString(char *s, unsigned int size)
{
	unsigned int sum;
	int n;

	/* Sum the string in reverse so that consecutive integers, as
	   strings, do not hash to consecutive locations */

	for (sum = 0, n = strlen(s), s += n-1; n--; s--)
		sum = (sum*31) + (*s-31);
	return ((sum % size) + 1);
}

void PrintTimeStamp(FILE *f)
    /* Prints current timestamp on f; 
       Keeps track of when last invocation was;
       If a day boundary is crossed (i.e., midnight),
          prints out date first.
       CAVEAT: uses static locals to remember when last called.
       There should really be one such local per FILE * used, otherwise
       new date will appear on only one of the log files being used.
       I can't figure out how to do this cleanly, short of adding
       yet another required parameter or keeping a list of FILE *'s
       seen so far, and a separate oldyear and oldday for each. This
       seems overkill for now.
    */
{
#define NLOGS 5
    /* these are used for keeping track of when we last logged a message */
    static struct { FILE *file; int year; int yday; } logs[NLOGS];

    struct tm *t;
    time_t clock;
    int i, empty = NLOGS;

    time(&clock);
    t = localtime(&clock);

    /* try to find the last time we wrote to this log */
    for (i = 0; i < NLOGS; i++) {
	if (f == logs[i].file) break;
	/* remember the first empty position we see */
	if (empty == NLOGS && !logs[i].file)
	    empty = i;
    }
    /* log entry not found? use the empty slot */
    if (i == NLOGS) i = empty;

    if (i != NLOGS &&
	(t->tm_year > logs[i].year || t->tm_yday > logs[i].yday))
    {
	char datestr[80];

	strftime(datestr, sizeof(datestr), "\nDate: %a %m/%d/%Y\n\n", t);
	fputs(datestr, f);

	/* remember when we were last called */
	logs[i].file = f;
	logs[i].year = t->tm_year;
	logs[i].yday = t->tm_yday;
    }

    fprintf(f, "%02d:%02d:%02d ", t->tm_hour, t->tm_min, t->tm_sec);    
}

extern void LogMsg(int msglevel, int debuglevel, FILE *fout, char *fmt,  ...)
{
    va_list ap;

    if (debuglevel < msglevel) 
	    return;

    PrintTimeStamp(fout);
    
    va_start(ap, fmt);
    vfprintf(fout, fmt, ap);
    fprintf(fout, "\n");
    fflush(fout);
    va_end(ap);
}

/* Send a message out on a file descriptor. */
#ifdef __CYGWIN32__
/* int vsnprintf(char *buf, size_t len, char *fmt, va_list ap) 
{
    return vsprintf(buf, fmt, ap);
} */

long int gethostid(void)
{
	return 4711;
}

#endif

void fdprint(long afd, char *fmt, ...) 
{
    va_list ap;
    char buf[240];

    va_start(ap, fmt);
    vsnprintf(buf, 239, fmt, ap);
    va_end(ap);
    write((int) afd, buf, (int) strlen(buf));
}

/* message to stderr */
void eprint(char *fmt, ...) 
{
	va_list ap;
	char msg[240];
	char *cp = msg;

	/* Construct message in buffer and add newline */
	va_start(ap, fmt);
	vsnprintf(cp, 239, (const char *)fmt, ap); /* leave 1 char for the "\n" */
	va_end(ap);
	cp += strlen(cp);
	strcat(cp, "\n");

	/* Write to stderr & stdout*/
	PrintTimeStamp(stdout); 
	fprintf(stdout, msg); 
	fflush(stdout);
	PrintTimeStamp(stderr);
	fprintf(stderr, msg);
	fflush(stderr);
}


/* hostname returns name of this host */
char *hostname(char *name)
{
    struct utsname id;
    
    if ( uname(&id) >= 0 ) 
	return strcpy(name, id.nodename);
    else 
	return NULL;
}

/* return 1 if hosts have same first address in h_addr_list */
int UtilHostEq(const char *name1, const char *name2)
{
    char *addr;
    int len;
    struct hostent *host;

    /* Validity check */
    if (!name1 || !name2)
        return 0;
    
    host = gethostbyname(name1);
    if ( ! host ) 
        return 0;
    else
        len = host->h_length;

    addr = (char *) malloc(len);
    if (!addr )
        return 0;
    else 
        memcpy(addr, host->h_addr_list[0], len);

    host = gethostbyname(name2);
    if ( ! host ) 
        return 0;

    if ( host->h_length != len ) 
	return 0;

    if ( strncmp(addr, host->h_addr_list[0], len) == 0 ) 
	return 1;
    else 
	return 0;

}

void UtilDetach()
{
    pid_t child; 
    int rc;

    child = fork();
    
    if ( child < 0 ) { 
	fprintf(stderr, "Cannot fork: exiting.\n");
	exit(1);
    }

    if ( child != 0 ) /* parent */
	exit(0); 

    rc = setsid();

    if ( rc < 0 ) {
	fprintf(stderr, "Error detaching from terminal.\n");
	exit(1);
    }

}
