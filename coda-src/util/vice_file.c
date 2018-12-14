/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2003-2016 Carnegie Mellon University
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

void
vice_dir_init (const char *dirname)
{
    strncpy(vicedir, dirname, MAXPATHLEN-1);
    vicedir[MAXPATHLEN-1] = '\0';
}

static const char *
vice_filepath (const char *dir, const char *name)
{
	static char volpath[2][MAXPATHLEN];
	static int vpidx = 0;
        int n;

	if (!name)
		return dir; 

	/* We need at least 2 static volpaths to be able to handle:
	 *   rename(Vol_vicefile(p1), Vol_vicefile(p2);
	 * the following indexing takes care of that. -JH */
	vpidx = 1 - vpidx;
	n = snprintf(volpath[vpidx], MAXPATHLEN, "%s/%s",  dir, name);
        CODA_ASSERT(n < MAXPATHLEN);

	return volpath[vpidx];
}

const char *
vice_config_path(const char *name)
{
	return vice_filepath(vicedir, name);
}
