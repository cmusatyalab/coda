/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#include "lka.h"

/* "Helper" functions for SHA */

void ViceSHAtoHex (unsigned char vsha[SHA_DIGEST_LENGTH],
		   char *printbuf, int printbuflen)
{
/* Convert a Vice SHA structure into a printable hex string in a
   buffer of length printbuflen (at least 60 chars long) */

  int i;

  if (printbuflen <= (3*SHA_DIGEST_LENGTH - 1)) return; /* buffer too short */
  for (i = 0; i < SHA_DIGEST_LENGTH; i++)
    sprintf(&printbuf[3*i], "%02x.", vsha[i]);
  printbuf[3*SHA_DIGEST_LENGTH-1] = '\0';
}


int ComputeViceSHA(int fd, unsigned char vsha[SHA_DIGEST_LENGTH])
{
/* ComputeViceSHA() takes an open file and returns its SHA value
   in a Vice SHA structure.
   Returns 1 on success, and 0 on any kind of failure  */

  int nbytes = 0;
  SHA_CTX cx;
  char *shachunk = 0;
#define SHACHUNKSIZE 4096  /* might be better to set to fs block size? */

  shachunk = (char *) malloc(SHACHUNKSIZE);
  if (!shachunk) return (0);

  SHA1_Init(&cx);
  do {
    nbytes = read (fd, shachunk, SHACHUNKSIZE);
    SHA1_Update(&cx, shachunk, nbytes);
  } while (nbytes > 0);
  SHA1_Final(vsha, &cx);

  free(shachunk);
  return(1);
}

int IsZeroSHA(unsigned char testsha[SHA_DIGEST_LENGTH])
{
/* IsZeroSHA() returns 1 if testsha is all zeros; 0 otherwise */

  for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    if (testsha[i]) return(0); /* non-zero byte in sha */
  return(1); 
}



