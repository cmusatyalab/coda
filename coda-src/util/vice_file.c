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
	if (serverno != 0) {
	    snprintf(serverdir, MAXPATHLEN, "%s/server_%d", vicedir, serverno);
	} else {
	    strncpy(serverdir,vicedir, MAXPATHLEN);
	}
}

char *
vice_file (char *name)
{
	static char *volpath[2] = { NULL, NULL };
	static char  vpidx = 0;

	if (!volpath[0]) {
		volpath[0] = (char *)malloc(MAXPATHLEN);
		volpath[1] = (char *)malloc(MAXPATHLEN);
	}
	CODA_ASSERT(volpath[0]);
	if (!vicedir)
		CODA_ASSERT(0);
	if (!name)
		return serverdir; 

	/* We need at least 2 static volpaths to be able to handle:
	 *   rename(Vol_vicefile(p1), Vol_vicefile(p2);
	 * the following indexing takes care of that. -JH */
	vpidx = (vpidx + 1) & 0x1;
	snprintf(volpath[vpidx], MAXPATHLEN, "%s/%s",  serverdir, name);

	return volpath[vpidx];
}

char *
vice_sharedfile (char *name)
{
	static char *volpath[2] = { NULL, NULL };
	static char  vpidx = 0;

	if (!volpath[0]) {
		volpath[0] = (char *)malloc(MAXPATHLEN);
		volpath[1] = (char *)malloc(MAXPATHLEN);
	}
	CODA_ASSERT(volpath[0]);
	if (!vicedir)
		CODA_ASSERT(0);
	if (!name)
		return vicedir;

	/* We need at least 2 static volpaths to be able to handle:
	 *   rename(Vol_vicefile(p1), Vol_vicefile(p2);
	 * the following indexing takes care of that. -JH */
	vpidx = (vpidx + 1) & 0x1;
	snprintf(volpath[vpidx], MAXPATHLEN, "%s/%s",  vicedir, name);

	return volpath[vpidx];
}
