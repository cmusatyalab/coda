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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/pdbstuff/pwd2pdb.cc,v 4.3 1997/12/10 16:09:55 braam Exp $";
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
#include <sys/file.h>
#include <sys/time.h>
#include <ctype.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

/* DISCLAIMER:
	1.  I know this program could be made MUCH more efficient using better search algorithms;
	    Be happy this program even exists and don't complain
	2.  Does not handle groups being members of other groups
*/

#define GOBBLE(x)    while( *x && (*x == ' ' || *x == '\t')) x++;


#define ROOTID 778
#define ANONYMOUSID 776
#define SYSTEMID 777



struct	passwd		/* similar to, but not exactly, <pwd.h> */
    {
    char *rawentry;	/* pointer into bigbuf, where this entry begins */
    char *pw_name;
    char *pw_passwd;
    int	 pw_uid;
    int	 pw_gid;
    int	 pw_quota;
    char *pw_comment;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
    };

#define MAXPWENTS 5000
struct passwd pwarray[MAXPWENTS];
int pwcount;	/* no of entries in pwarray */

#define PWSIZE   100000
char pwbuf[PWSIZE];
int pwlen;	/* bytes actually used in pwbuf */



#define ANYUSERID -101

#define MAXMEMBERS	100
struct group
    {
    char *rawentry;	/* pointer into gbuf where this entry begins */
    char *g_name;
    int   g_id;		
    char *g_oname;
    int   g_owner;	
    char *g_memlist;
    int  g_memcount;
    int  g_members[MAXMEMBERS];	/* filled in pass 2 */
    };

#define MAXGROUPS	50
struct group  garray[MAXGROUPS];
int gcount;	/* no of entries used in garray */

#define GROUPSIZE  20000
char gbuf[GROUPSIZE];
int glen;	/* bytes actually used in gbuf */

/*
<owner>:<groupname>	<groupid>	<member1> <member2> ........
*/


int DebugLevel;


/* Function declarations */
int pwcmp(register struct passwd *p1, register struct passwd *p2);
int gcmp(register struct group *g1, register struct group *g2);
char *pwskip(register char *p, char t);
void pwparse(register char *nextline, register struct passwd *pwptr);
void Preamble();
void PrintUserEntry(register struct passwd *pw, register struct passwd *prevpw);
void gparse1 (register char *nextline, register struct group *gptr);
int getuserid(register char *name);
void gparse2(register char *members, register struct group *gptr);
void fillmembership(int userid, char *str1, char *str2, char *str3);
void PrintGroupEntry(register struct group *ge, register struct group *prevge);
void pwscan();





int main(int argc, char *argv[])
    {
    int ufd, gfd;	/* file desc's on password and group files */
    register int i;
    struct timeval t;

    /* get command line args */
    ufd = gfd = -1;
    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0)
	    {DebugLevel = 1; continue;}
	if (strcmp(argv[i], "-u") == 0 && i < argc -1)
	    {
	    ufd = open(argv[++i], O_RDONLY, 0);
	    if (ufd < 0) {perror(argv[i]); exit(-1);}
	    else continue;
	    }
	if (strcmp(argv[i], "-g") == 0 && i < argc -1)
	    {
	    gfd = open(argv[++i], O_RDONLY, 0);
	    if (gfd < 0) {perror(argv[i]); exit(-1);}
	    else continue;
	    }

	printf("Usage: pwd2pdb -u usersfile  [-g groupfile] [-x]\n");
	exit(-1);
	}    

    /*  read input file in one fell swoop */
    if (ufd < 0) {printf("No users file specified\n"); exit(-1);}
    pwlen = read(ufd, pwbuf, PWSIZE);
    if (pwlen >= PWSIZE)
    	{
	printf("Users file too large; recompile with larger PWSIZE\n");
	exit(-1);	
	}
    else close(ufd);

    /* scan big buffer and chop into individual lines */
    pwscan();

    /* parse each entry */
    for (i = 0; i < pwcount; i++)
	pwparse(pwarray[i].rawentry, &pwarray[i]);

    /* sort in ascending uid order */
    qsort((char *)pwarray, pwcount, sizeof(struct passwd), 
	  (int (*)(const void *, const void *))pwcmp);

    fprintf(stderr, "%d user entries found\n", pwcount);
    close(ufd);

    /* Now process Group file */

    /* read input file in one fell swoop */
    glen = read(gfd, gbuf, GROUPSIZE);
    if (glen >= GROUPSIZE)
    	{
	printf("Group file too large; recompile with larger GROUPSIZE\n");
	exit(-1);	
	}
    else close(gfd);

    /* get pointers to beginning of each group entry */ 
    i = 0;
    gbuf[glen] = 0;
    for (gcount = 0; i < glen && gcount < MAXGROUPS; gcount++)
	{/* suck in next line */
	garray[gcount].rawentry = &gbuf[i];
	while (gbuf[i] != '\n' && gbuf[i] != 0) i++;
	gbuf[i++] = 0;
	if (garray[gcount].rawentry[0] == '#'){gcount--; continue;} /* skip comments */
	}

    if (gcount >= MAXGROUPS-1)
	{
	fprintf(stderr, "Too many group entries; recompile with bigger MAXGROUPS\n");
	exit(-1);
	}

    /* Pass1: obtain group names, ids and owners */
    for (i = 0; i < gcount; i++)
	gparse1(garray[i].rawentry, &garray[i]);

    /* Pass2: obtain group membership info */
    for (i = 0; i < gcount; i++)
	gparse2(garray[i].g_memlist, &garray[i]);


    /* sort in descending gid order */
    qsort((char *)garray, gcount, sizeof(struct group), 
 	  (int (*)(const void *, const void *))gcmp);


    Preamble();

    gettimeofday(&t, 0);
    printf("# Created from users file on %s\n", ctime((const long *)&t.tv_sec));
    printf("# Highest UID in use = %d\n", pwarray[pwcount-1].pw_uid);
    printf("# Lowest GID  in use = %d\n\n", garray[gcount-1].g_id);
    
    printf("####  User entries from users file begin here ####\n\n");
    for (i = 0; i < pwcount; i++)
	PrintUserEntry(&pwarray[i], (i == 0) ? 0 : &pwarray[i-1]);


    printf("####  Group entries from group file begin here ####\n\n");
    for (i = 0; i < gcount; i++)
	PrintGroupEntry(&garray[i], (i == 0) ? 0 : &garray[i-1]);


    printf("########### EMPTY ENTRY:  leave this here!!! ##############\n");
    printf("\t\t;\n");

    }
	
char *pwskip(register char *p, char t)
    /* t = tab character; 0 ==> white space */
    {
    register char t1, t2;
    if (t == 0) {t1 = ' '; t2 = '\t';}
    else {t1 = t2 = t;}
    while( *p && *p != t1 && *p != t2)
	++p;
    if( *p ) *p++ = 0;
    return(p);
    }

void pwparse(register char *nextline, register struct passwd *pwptr)
    /*
    nextline: line from password file to be parsed
    pwptr: pointer to an allocated area which gets filled
    */
    {
    register char *p = nextline;

    pwptr->pw_name = p;
    p = pwskip(p, ':');
    pwptr->pw_passwd = p;
    p = pwskip(p, ':');
    pwptr->pw_uid = atoi(p);
    assert(pwptr->pw_uid < 750 || pwptr->pw_uid > 800);	/* magic range used by AL package */
    p = pwskip(p, ':');
    pwptr->pw_gid = atoi(p);
    pwptr->pw_quota = 0;
    pwptr->pw_comment = 0;
    p = pwskip(p, ':');
    pwptr->pw_gecos = p;
    p = pwskip(p, ':');
    pwptr->pw_dir = p;
    p = pwskip(p, ':');
    pwptr->pw_shell = p;
    while(*p && *p != '\n') p++;
    *p = '\0';
    }




void Preamble()
    {
    char str1[6*MAXGROUPS], str2[6*MAXGROUPS], str3[6*MAXGROUPS]; /* assuming 6 chars per id in list*/
    printf("############################\n");
    printf("# VICE protection database #\n");
    printf("############################\n");
    printf("\n");
    printf("# Lines such as these are comments. Comments and whitespace are ignored.\n");
    printf("\n");
    printf("# This file consists of user entries and group entries in no particular order.\n");
    printf("# An empty entry indicates the end.\n");
    printf("\n");
    printf("# A user entry has the form:\n");
    printf("# UserName\tUserId\n");
    printf("#\t\t\"Is a group I directly belong to\"_List\n");
    printf("#\t\t\"Is a group in my CPS\"_List\n");
    printf("#\t\t\"Is a group owned by me\"_List\n");
    printf("#\t\tAccess List\n");
    printf("#\t\t;\n");
    printf("\n");
    printf("# A group entry has the form:\n");
    printf("# GroupName\tGroupId OwnerId\n");
    printf("#\t\t\"Is a group I directly belong to\"_List\n");
    printf("#\t\t\"Is a group in my CPS\"_List\n");
    printf("#\t\t\"Is a user or group who is a direct member of me\"_List\n");
    printf("#\t\tAccess List\n");
    printf("#\t\t;\n");
    printf("\n");
    printf("\n");
    printf("# A simple list has the form ( i1 i2 i3 ..... )\n");
    printf("\n");
    printf("# An access list has two tuple lists:\n");
    printf("#\t\tone for positive and the other for negative rights:\n");
    printf("#\t\t(+ (i1 r1) (i2 r2) ...)\n");
    printf("#\t\t(- (i1 r1) (i2 r2) ...)\n");
    printf("\n\n\n");

    printf("System\t\t%d\n", SYSTEMID);
    str1[0] = str2[0] = str3[0] = 0;
    fillmembership(SYSTEMID, str1, str2, str3);
    printf("\t\t( %s)\n", str1); printf("\t\t( %s)\n", str2); printf("\t\t( %s)\n", str3);
    printf("\t\t(+ (%d -1))\n", SYSTEMID);printf("\t\t(- )\n");
    printf("\t\t;\n\n\n");

    printf("System:AnyUser\t\t%d\t%d\n", ANYUSERID, SYSTEMID);
    printf("\t\t( )\n");printf("\t\t( )\n");printf("\t\t( )\n");
    printf("\t\t(+ (%d -1))\n", SYSTEMID);printf("\t\t(- )\n");
    printf("\t\t;\n\n\n");

    printf("Anonymous\t\t%d\n", ANONYMOUSID);
    str1[0] = str2[0] = str3[0] = 0;
    fillmembership(ANONYMOUSID, str1, str2, str3);
    printf("\t\t( %s)\n", str1); printf("\t\t( %s)\n", str2); printf("\t\t( %s)\n", str3);
    printf("\t\t(+ (%d -1))\n", SYSTEMID);printf("\t\t(- )\n");
    printf("\t\t;\n\n\n");

    }

void PrintUserEntry(register struct passwd *pw, register struct passwd *prevpw)
    {
    char str1[6*MAXGROUPS], str2[6*MAXGROUPS], str3[6*MAXGROUPS]; /* assuming 6 chars per id in list*/

    if (prevpw != NULL && pw->pw_uid == prevpw->pw_uid)
	{
	printf("#  ENTRY WITH UID IDENTICAL TO PRECEDING ENTRY OMITTED: %s \n\n\n",
		pw->pw_name);
	fprintf(stderr, "Arrrrgggggggh..... user entries with same uid fields:\n");
	fprintf(stderr, "\t\"%s\"\n", pw->rawentry);
	fprintf(stderr,"\t\"%s\"\n", prevpw->rawentry);
	return;	
	}
    printf("#\t\t%s\n", pw->pw_gecos);
    printf("%s\t\t%d\n", pw->pw_name, (pw->pw_uid == 0) ?  ROOTID : pw->pw_uid);

    str1[0] = str2[0] = str3[0] = 0;
    fillmembership(pw->pw_uid, str1, str2, str3);
    printf("\t\t( %s)\n", str1);
    printf("\t\t( %s)\n", str2);
    printf("\t\t( %s)\n", str3);
    printf("\t\t(+ (%d -1) (%d 1))\n", (pw->pw_uid == 0) ? ROOTID : pw->pw_uid,
			ANYUSERID);
    printf("\t\t(- )\n");
    printf("\t\t;\n\n\n");
    }



void gparse1 (register char *nextline, register struct group *gptr)
    /*
    nextline: line from group file to be parsed
    gptr:  pointer to an  area which gets filled
    */
    {
    register char *p;

    p = nextline;
    gptr->g_oname = p;
    p = pwskip(p, ':');
    gptr->g_name = p;
    gptr->g_owner = getuserid(gptr->g_oname);
    if (gptr->g_owner < 0)
	{fprintf(stderr, "Unknown owner: \"%s\"\n", gptr->rawentry); exit(-1);}

    p = pwskip(p, 0); GOBBLE(p);
    gptr->g_id = atoi(p);
    if (gptr->g_id >= ANYUSERID)
	{fprintf(stderr, "Bad group id (should be < %d) in: \"%s\"\n", ANYUSERID, gptr->rawentry); exit(-1);}
    p = pwskip(p, 0); GOBBLE(p);
    gptr->g_memlist = p;
    while(*p && *p != '\n') p++;
    *p = '\0';
    }

int getuserid(register char *name)
    {
    register int i;
    if (CaseFoldedCmp(name, "system") == 0) return(SYSTEMID);
    if (CaseFoldedCmp(name, "Anonymous") == 0) return(ANONYMOUSID);

    for (i = 0; i < pwcount; i++)
	if (CaseFoldedCmp(pwarray[i].pw_name, name) == 0) return(pwarray[i].pw_uid);
    return(-1);
    }


void gparse2(register char *members, register struct group *gptr)
    {
    register char *p, *q;
    register int i;

    
    p = members;
    for (i = 0; i < MAXMEMBERS && *p != 0; i++)
	{
	q = p;
	p = pwskip(p, 0);
	GOBBLE(p);
	gptr->g_members[i] = getuserid(q);
	if (gptr->g_members[i] < 0)
	    {fprintf(stderr, "Unknown member %s specified in group file\n", q); exit(-1);}
	}
    if (i  >= MAXMEMBERS)
	{
	fprintf(stderr, "Too many group members specified: recompile with larger MAXMEMBERS\n");
	exit(-1);
	}
    else gptr->g_memcount = i;
    }


void fillmembership(int userid, char *str1, char *str2, char *str3)
    {
    register int i, j;
    for (i = 0; i < gcount; i++)
	{
	for (j = 0; j < garray[i].g_memcount; j++)
	    if (userid == garray[i].g_members[j])
		{
		sprintf(&str1[strlen(str1)], "%d ", garray[i].g_id);
		sprintf(&str2[strlen(str2)], "%d ", garray[i].g_id);
		break;
		}
	if (userid == garray[i].g_owner) sprintf(&str3[strlen(str3)], "%d ", garray[i].g_id);
	}
    }



void PrintGroupEntry(register struct group *ge, register struct group *prevge)
    {
    register int i;
    char str1[6*MAXGROUPS], str2[6*MAXGROUPS], str3[6*MAXGROUPS]; /* assuming 6 chars per id in list*/

    if (prevge != NULL && ge->g_id == prevge->g_id)
	{
	fprintf(stderr, "Arrrrgggggggh..... group entries with same id fields:\n");
	fprintf(stderr, "\t\"%s\"\n", ge->rawentry);
	fprintf(stderr,"\t\"%s\"\n", prevge->rawentry);
	fprintf(stderr, "Aborting ....\n");
	exit(-1);
	}
    printf("%s:%s\t%d %d\n", ge->g_oname, ge->g_name, ge->g_id, ge->g_owner);

    str1[0] = str2[0] = str3[0] = 0;
    printf("\t\t( )\n", str1);	/* fix later */
    printf("\t\t( )\n", str2);	/* ditto */
    printf("\t\t( ");
    for (i = 0; i < ge->g_memcount; i++)
	printf("%d ", ge->g_members[i]);
    printf(")\n");
    printf("\t\t(+ (%d -1) (%d 1))\n", ge->g_owner, ANYUSERID);
    printf("\t\t(- )\n");
    printf("\t\t;\n\n\n");
    }


void pwscan()
    /* get pointers to beginning of each users entry */ 
    {
    register int i;
    enum {EatNull, EatText, EndText} mystate;

    mystate = EatNull;
    pwbuf[pwlen] = 0;
    for (pwcount = 0, i = 0; i < pwlen; i++)
	{
	if (pwbuf[i] == '\n')
	    {
	    if (mystate == EatNull) continue;
	    assert(mystate == EatText);
	    mystate = EndText;
	    }
	switch(mystate)
	    {
	    case EatNull:
		pwarray[pwcount++].rawentry = &pwbuf[i];
		if (pwcount >= MAXPWENTS-1)
		    {
		    printf("Too many users entries; recompile with bigger MAXPWENTS\n");
		    exit(-1);
		    }
		mystate = EatText;
		break;
		
	    case EatText:
		break;

	    case EndText:
		pwbuf[i] = 0;
		mystate = EatNull;
		break;
		
	    default: abort();
	    }

	}
    }



int pwcmp(register struct passwd *p1, register struct passwd *p2)
    {/* compare routine for qsort() */
    if (p1->pw_uid < p2->pw_uid) return(-1);
    if (p1->pw_uid > p2->pw_uid) return(1);
    return(0);
    }

int gcmp(register struct group *g1, register struct group *g2)
    {/* compare routine for qsort() */
    if (g1->g_id > g2->g_id) return(-1);
    if (g1->g_id < g2->g_id) return(1);
    return(0);
    }
