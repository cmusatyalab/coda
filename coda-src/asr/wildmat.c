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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/asr/wildmat.c,v 4.1 1997/01/08 21:49:24 rvb Exp $";
#endif /*_BLURB_*/





/*
**  Do shell-style pattern matching for ?, \, [], and * characters.
**  Might not be robust in face of malformed patterns; e.g., "foo[a-"
**  could cause a segmentation violation.  It is 8bit clean.
**
**  Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.
**  Rich $alz is now <rsalz@bbn.com>.
**  Special thanks to Lars Mathiesen <thorinn@diku.dk> for the ABORT code.
**  This can greatly speed up failing wildcard patterns.  For example:
**	pattern: -*-*-*-*-*-*-12-*-*-*-m-*-*-*
**	text 1:	 -adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1
**	text 2:	 -adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1
**  Text 1 matches with 51 calls, while text 2 fails with 54 calls.  Without
**  the ABORT, then it takes 22310 calls to fail.  Ugh.
*/

#define TRUE		1
#define FALSE		0
#define ABORT		-1

#define NEGATE_CLASS	'^'

/* Forward declaration. */
static int DoMatch(register char *text, register char *p);

/*
**  See if the text matches the p, which has an implied leading asterisk.
*/
static int
Star(register char *text, char * p)
{
    register int	ret;

    do
	ret = DoMatch(text++, p);
    while (ret == FALSE);
    return ret;
}


/*
**  Match text and p, return TRUE, FALSE, or ABORT.
*/
static int
DoMatch(register char *text, register char *p)
{
    register int 	 last;
    register int 	 matched;
    register int 	 reverse;

    for ( ; *p; text++, p++) {
	if (*text == '\0' && *p != '*')
	    return ABORT;
	switch (*p) {
	case '\\':
	    /* Literal match with following character. */
	    p++;
	    /* FALLTHROUGH */
	default:
	    if (*text != *p)
		return FALSE;
	    continue;
	case '?':
	    /* Match anything. */
	    continue;
	case '*':
	    /* Trailing star matches everything. */
	    return *++p ? Star(text, p) : TRUE;
	case '[':
	    if (reverse = p[1] == NEGATE_CLASS)
		/* Inverted character class. */
		p++;
	    for (last = 0400, matched = FALSE; *++p && *p != ']'; last = *p)
		/* This next line requires a good C compiler. */
		if (*p == '-' ? *text <= *++p && *text >= last : *text == *p)
		    matched = TRUE;
	    if (matched == reverse)
		return FALSE;
	    continue;
	}
    }

    return *text == '\0';
}


/*
**  User-level routine.  Returns TRUE or FALSE.
*/
int
wildmat(char *text, char * p)
{
    return DoMatch(text, p) == TRUE;
}



#ifdef	TEST
#include <stdio.h>
/* Yes, we use gets not fgets.  Sue me. */
extern char	*gets();


main()
{
    char	 p[80];
    char	 text[80];

    printf("Wildmat tester.  Enter pattern, then strings to test.\n");
    printf("A blank line gets prompts for a new pattern; a blank pattern\n");
    printf("exits the program.\n\n");

    for ( ; ; ) {
	printf("Enter pattern:  ");
	(void)fflush(stdout);
	if (gets(p) == NULL || p[0] == '\n')
	    break;
	for ( ; ; ) {
	    printf("Enter text:  ");
	    (void)fflush(stdout);
	    if (gets(text) == NULL)
		exit(0);
	    if (text[0] == '\0')
		/* Blank line; go back and get a new pattern. */
		break;
	    printf("      %s\n", wildmat(text, p) ? "YES" : "NO");
	}
    }

    exit(0);
    /* NOTREACHED */
}
#endif	/* TEST */
