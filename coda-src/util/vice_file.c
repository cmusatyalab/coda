/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 2000 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

*/

/* vice_file.c: Routines to find the correct path to files stored in
 *		the "/vice" tree supporting coda servers. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <coda_string.h>
#include <stdio.h>
#include <sys/param.h>
#include <coda_assert.h>
#include "vice_file.h"

static char vicedir[MAXPATHLEN];
static char serverdir[MAXPATHLEN];

void
vice_dir_init (char *dirname, int serverno)
{
	strncpy(vicedir, dirname, MAXPATHLEN);
	strncpy(serverdir, dirname, MAXPATHLEN);
	if (serverno) {
	    int len = strlen(dirname);
	    snprintf(&serverdir[len], MAXPATHLEN - len, "/server_%d", serverno);
	}
}

static char *
vice_filepath (char *dir, char *name)
{
	static char volpath[2][MAXPATHLEN];
	static int vpidx = 0;

	if (!name)
		return dir; 

	/* We need at least 2 static volpaths to be able to handle:
	 *   rename(Vol_vicefile(p1), Vol_vicefile(p2);
	 * the following indexing takes care of that. -JH */
	vpidx = 1 - vpidx;
	snprintf(volpath[vpidx], MAXPATHLEN, "%s/%s",  dir, name);

	return volpath[vpidx];
}

char *
vice_file (char *name)
{
	return vice_filepath(serverdir, name);
}

char *
vice_sharedfile (char *name)
{
	return vice_filepath(vicedir, name);
}

