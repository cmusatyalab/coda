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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/auth2/RCS/initpw.cc,v 4.1 1997/01/08 21:49:27 rvb Exp $";
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




/*
initpw.c -- hack routine to initially generate the pw file used by auth2

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/file.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <rpc2.h>
#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

int main(int argc, char **argv);
PRIVATE void parse(char *line, RPC2_EncryptionKey outpw, char **last);

PRIVATE int DebugLevel = 0;
PRIVATE int KeyIsValid = FALSE;
PRIVATE RPC2_EncryptionKey EKey;


int main(int argc, char **argv)
    {
    register int i;
    char thisline[1000], *lastpart;
    RPC2_EncryptionKey thispw;

    /* Obtain invocation options */
    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-x") == 0 && i < argc -1)
	    {
	    DebugLevel = atoi(argv[++i]);
	    continue;
	    }
	if (strcmp(argv[i], "-k") == 0 && i < argc -1)
	    {
	    KeyIsValid = TRUE;
	    strncpy((char *)EKey, argv[++i], sizeof(RPC2_EncryptionKey));
	    continue;
	    }
	printf("Usage: initpw [-x debuglevel] [-k key]\n");
	exit(-1);
	}    

/* Reads lines from stdin of the form:
	<ViceId> <Clear Password> <other junk>\n

   Produces lines on stdout of the form:
	<ViceId> <Hex representation of encrypted password> <other junk>\n

*/
    if (!KeyIsValid) 
	fprintf(stderr, "WARNING: no key specified\n");

    PROCESS mypid;
    assert(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mypid) == LWP_SUCCESS);
    while(TRUE)
	{
	if (gets(thisline) == NULL) break;
	parse(thisline, thispw, &lastpart);
	if (KeyIsValid)
	    rpc2_Encrypt((char *)thispw, (char *)thispw, sizeof(RPC2_EncryptionKey), (char *)EKey, RPC2_XOR);
	printf("%s\t", thisline);	/* only viceid part */
	for (i = 0; i < sizeof(RPC2_EncryptionKey); i++)
	    printf("%02x", thispw[i]);
	printf("\t%s\n", lastpart);
	}
    return(0);
    }


PRIVATE void parse(char *line, RPC2_EncryptionKey outpw, char **last)
/* line:    input: first tab is replaced by null */
/* outpw:   output: filled with password */
/* last:    output: points to first character of uninterpreted part */
    {
    char *pp;
    int i;
    pp = (char *)index(line, '\t');
    if (pp == NULL)
	{
	fprintf(stderr, "Bogus line in input file: \"%s\"\n", line);
	abort();
	}
    *pp++ = 0;
    bzero(outpw, sizeof(RPC2_EncryptionKey));
    i = 0;
    while(pp && *pp != 0 && *pp != '\t' && i < sizeof(RPC2_EncryptionKey))
	outpw[i++] = *pp++;
    while(pp && *pp != 0 && *pp != '\t') pp++;
    if (*pp == 0) *last = pp;
    else *last = pp + 1;	/* skip over tab */

    }


