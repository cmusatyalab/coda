#ifndef _BLURB_
#define _BLURB_
/*
 * This code was originally part of the CMU SCS library "libcs".
 * A branch of that code was taken in May 1996, to produce this
 * standalone version of the library for use in Coda and Odyssey
 * distribution.  The copyright and terms of distribution are
 * unchanged, as reproduced below.
 *
 * Copyright (c) 1990-96 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND CARNEGIE MELLON UNIVERSITY
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Users of this software agree to return to Carnegie Mellon any
 * improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Export of this software is permitted only after complying with the
 * regulations of the U.S. Deptartment of Commerce relating to the
 * Export of Technical Data.
 */

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/libcs/stabsearch.c,v 1.1 1996/11/22 19:19:20 braam Exp $";
#endif /*_BLURB_*/

/*  stabsearch --  search for best match within string table
 * 
 *  int stabsearch (arg,table,quiet);
 *  char *arg;
 *  char **table;
 *  int quiet;
 * 
 *  Just like stablk(3), but a match score is determined for each
 *  string in the table, and the best match is used.  If quiet=0,
 *  the user will be asked if he really meant the best matching
 *  string; if he says "no", a list of several other good matches
 *  will be printed.
 *  If there is exactly one perfect match, then its index will be
 *  returned, and the user will not be asked anything.  If there are
 *  several perfect matches, then up to 50 will be listed for the
 *  user to review and among which he can select one.
 * 
 */

#include <stdio.h>

#include "libcs.h"

#define KEEPBEST 6		/* # of matches to keep around */
#define KEEPPERFECT 50		/* # of perfect matches to keep */
#define KEEPEXACT 50		/* # of exact matches to keep */
#define PERFECT 100		/* perfect match score */
#define EXACT 101		/* exact match pseudo-score */
#define THRESHOLD 35		/* minimum acceptable score */
#define NOMATCH -1		/* return value if no match */
#define MAXLENGTH 400		/* max length of arg and table entries */
#define SCROLLSIZE 20		/* size of each screenful for scrolling */

int stabsearch (arg,table,quiet)
char *arg, **table;
int quiet;
{
	int bestentry[KEEPPERFECT], bestscore[KEEPPERFECT];
	int arglen;
	int maxscore;		/* best possible score */
	register int i,j,k;	/* temps */
	int nperfect;		/* # of perfect matches */
	int nexact; 		/* # of exact matches */
	int nentries;		/* # of entries in table */
	char line[MAXLENGTH];
	char a[MAXLENGTH],e[MAXLENGTH];

	if (arg == 0)
	    return (NOMATCH);
	if (strcmp (arg,"?") != 0) {

		for (i=0; i<KEEPPERFECT; i++) {
			bestentry[i] = -1;
			bestscore[i] = -1;
		}
		arglen = strlen (arg);
		maxscore = arglen * arglen;
		if( !maxscore ) maxscore=1;  /* if the default is a NULL string */
		folddown (a,arg);

		nperfect = 0;
		nexact = 0;
		for (i=0; table[i]; i++) {
			folddown (e,table[i]);
			j = (srchscore (e,a) * PERFECT) / maxscore;
			if (nperfect == 0 && nexact == 0 && j != PERFECT) {
				if (j >= bestscore[KEEPBEST-1]) {
					k = KEEPBEST - 1;
					while ((k > 0) && (j > bestscore[k-1])) {
						bestscore[k] = bestscore[k-1];
						bestentry[k] = bestentry[k-1];
						--k;
					}
					bestscore[k] = j;
					bestentry[k] = i;
				}
			}
			else if (j == PERFECT && arglen == strlen (e))
			{	bestscore[0] = EXACT;
				if (nexact < KEEPEXACT)
					bestentry[nexact++] = i;
			}
			else if (j == PERFECT && nperfect < KEEPPERFECT && !nexact) {
				bestscore[0] = PERFECT;
				bestentry[nperfect] = i;
				nperfect++;
			}
		}
		nentries = i;

		if (bestscore[0] <= THRESHOLD) {
			if (!quiet) {
				fprintf (stderr,"Sorry, nothing matches \"%s\" reasonably.\n",arg);
				if (getbool("Do you want a list?",(nentries<=SCROLLSIZE))) goto makelist;
			}
			return (NOMATCH);
		}

		if (quiet)  return (bestentry[0]);
		if (nexact > 1)
		{	fprintf (stderr,"There are %d exact matches for \"%s\":\n",nexact,arg);
			j = getint ("Which one should be used?  (0 if none ok)",0,nexact,1);
			return ((j > 0) ? bestentry[j-1] : NOMATCH);
		}
		else if (nexact == 1) return (bestentry[0]);

		if (nperfect > 1) {	/* multiple max matches */
			fprintf (stderr,"There are %d perfect matches for \"%s\":\n",nperfect,arg);
			for (j=0; j<nperfect && ((j%SCROLLSIZE!=SCROLLSIZE-1) || getbool("Continue?",1)); j++) {
				fprintf (stderr,"(%d)\t%s\n",j+1,table[bestentry[j]]);
			}
			j = getint ("Which one should be used?  (0 if none ok)",0,nperfect,1);
			return ((j > 0) ? bestentry[j-1] : NOMATCH);
		}
		else if (nperfect == 1) {	/* unique perfect match */
			return (bestentry[0]);
		}

		sprintf (line,"Did you mean \"%s\" [%d] ?",
			table[bestentry[0]],bestscore[0]);
		if (getbool(line,1))  return (bestentry[0]);

		fprintf (stderr,"May I suggest the following?\n");
		for (i=0; i<KEEPBEST && bestscore[i] >= THRESHOLD; i++) {
			fprintf (stderr,"(%d)\t%-15s \t[%d]\n",i+1,table[bestentry[i]],bestscore[i]);
		}
		j = getint ("Which one should be used?  (0 if none ok)",0,i,1);
		if (j>0)  return (bestentry[j-1]);
		if (getbool("Do you want a list of all possibilities?",(nentries<=SCROLLSIZE)))  goto makelist;

	}
	else {
makelist:

		fprintf (stderr,"The choices are as follows:\n");
		fprstab (stderr,table);
	}

	return (NOMATCH);
}
