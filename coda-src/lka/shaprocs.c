/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <lwp/lwp.h>

#include "lka.h"

#define SHA_YIELD_INTERVAL 200

/* "Helper" functions for SHA */

void ViceSHAtoHex(unsigned char sha[SHA_DIGEST_LENGTH],
		  char *printbuf, int printbuflen)
{
    /* Convert a Vice SHA structure into a printable hex string in a
       buffer of length printbuflen (at least 40 chars long) */

    int i;

    if (printbuflen <= 2*SHA_DIGEST_LENGTH)
	return; /* buffer too short */

    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
	sprintf(&printbuf[2*i], "%02x", sha[i]);

    printbuf[2*SHA_DIGEST_LENGTH] = '\0';
}


int CopyAndComputeViceSHA(int infd, int outfd,
			  unsigned char sha[SHA_DIGEST_LENGTH])
{
    /* ComputeViceSHA() takes an open file and returns its SHA value
       in a Vice SHA structure. If outfd is not -1, we copy the while
       computing the SHA.
       Returns 0 on success, and -1 on any kind of failure  */

    int bytes_out, bytes_in = 0;
    int i = 0;
    SHA_CTX cx;

#define SHACHUNKSIZE 4096  /* might be better to set to fs block size? */
    unsigned char shachunk[SHACHUNKSIZE];

    SHA1_Init(&cx);
    while (1) {
	/* make sure we yield to other threads once in a while */
	if ((++i % SHA_YIELD_INTERVAL) == 0)
	    LWP_DispatchProcess();

	bytes_in = read (infd, shachunk, SHACHUNKSIZE);
	if (bytes_in <= 0)
	    break;

	SHA1_Update(&cx, shachunk, bytes_in);

	if (outfd != -1) {
	    bytes_out = write(outfd, shachunk, bytes_in);
	    if (bytes_out < bytes_in) return -1;
	}
    }
    SHA1_Final(sha, &cx);
    return (bytes_in < 0 ? -1 : 0);
}

int IsZeroSHA(unsigned char sha[SHA_DIGEST_LENGTH])
{
    int i;
/* IsZeroSHA() returns 1 if testsha is all zeros; 0 otherwise */

    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
	if (sha[i])
	    return(0); /* non-zero byte in sha */
    return(1); 
}



