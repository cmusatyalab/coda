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
	    reverse = (p[1] == NEGATE_CLASS);
	    if (reverse)
		/* Inverted character class. */
		p++;
	    for (last = 0400, matched = FALSE; *p && *p != ']'; last = *p, p++) {
		/* This next line requires a good C compiler. */
	      
		if (*p == '-' ? (p++, *text <= *p && *text >= last) : *text == *p)
		    matched = TRUE;
	    }
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
