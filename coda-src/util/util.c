#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/util.cc,v 4.1 1997/01/08 21:51:15 rvb Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <ctype.h>
#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */
#include <math.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <stdarg.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"

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
    do
	{/* Inv: s1[0:i] is identical to s2[0:i] and i is smaller than 
		the shorter of s1 and s2 */
	i += 1;
	c1 = s1[i];
	if (islower(c1)) c1 -= ('a' - 'A');
	c2 = s2[i];
	if (islower(c2)) c2 -= ('a' - 'A');
	if (c1 == 0 && c2 ==0) return(0);
	}
    while(c1 == c2);

    if (c1 < c2) return (-1);
    else return(1);
    }

int SafeStrCat(char *dest, char *src, int totalspace)
    {
    if (strlen(dest)+strlen(src) >= totalspace) return(-1);
    strcat(dest,src);
    return(0);
    }

int SafeStrCpy(char *dest, char *src, int totalspace)
    {
    if (strlen(src) >= totalspace) return(-1);
    strcpy(dest,src);
    return(0);
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
    struct tm *t;
    time_t clock;
    static int oldyear = -1, oldyday = -1; 
    static char *day[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    time(&clock);
    t = localtime(&clock);
    if ((t->tm_year > oldyear) || (t->tm_yday > oldyday))
	fprintf(f, "\nDate: %3s %02d/%02d/%02d\n\n", day[t->tm_wday], t->tm_mon+1, t->tm_mday, t->tm_year);
    fprintf(f, "%02d:%02d:%02d ", t->tm_hour, t->tm_min, t->tm_sec);    
    oldyear = t->tm_year; /* remember when we were last called */
    oldyday = t->tm_yday;
}

extern void LogMsg(int msglevel, int debuglevel, FILE *fout, char *fmt,  ...)
{
    va_list ap;

    if (debuglevel < msglevel) return;

    PrintTimeStamp(fout);
    
    va_start(ap, fmt);
    vfprintf(fout, fmt, ap);
    fprintf(fout, "\n");
    fflush(fout);
    va_end(ap);
}

/* Send a message out on a file descriptor. */
void fdprint(long afd, char *fmt, ...) 
{
    va_list ap;
    char buf[240];

    va_start(ap, fmt);
    vsnprintf(buf, 240, fmt, ap);
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
    vsnprintf(cp, 239, fmt, ap); /* leave 1 char for the "\n" */
    va_end(ap);
    cp += strlen(cp);
    strcat(cp, "\n");

    /* Write to stderr */
    PrintTimeStamp(stderr);  /* first put out a timestamp */
    fprintf(stderr, msg); /* then the message */
    fflush(stderr);
}


/* hostname returns name of this host */
char *hostname(char *name)
{
    struct utsname id;
    
    if ( uname(&id) == 0 ) 
	return strcpy(name, id.nodename);
    else 
	return NULL;
}

/* return 1 if hosts have same first address in h_addr_list */
int UtilHostEq(char *name1, char *name2)
{
    char *addr;
    int len;
    struct hostent *host;
    
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
