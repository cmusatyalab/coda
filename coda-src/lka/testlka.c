/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include "lka_private.h"

char em[4096];
int emlen = sizeof(em);

/* dummy function for ComputeViceSHA */
int LWP_DispatchProcess(void) { return 0; }


int main(int argc, char **argv)
{
  int rc, fd, cfd;
  char testfile[MAXPATHLEN];
  unsigned char testsha[SHA_DIGEST_LENGTH];
  char container[MAXPATHLEN], shaprintbuf[2*SHA_DIGEST_LENGTH+1]; 
  
  switch (argc) {
  case 2:
    rc = LKParseAndExecute(argv[1], em, emlen);
    if (em[0]) printf(em); /* hopefully useful msg */
    if (!rc) exit(-1); 
    break;

  default:
    printf ("Usage: testlka <quoted command string for cfs lka>\n");
    printf ("       testlka then prompts interactively\n");
    exit(0);
  }

  while (1) {
    printf("File name (""."" to quit): ");
    scanf("%s", testfile);
    if (!strcmp(testfile, ".")) goto Quit;

    /* find SHA of testfile */
    fd = open (testfile, O_RDONLY, 0);
    if (fd < 0) {
      printf("%s: %s\n", testfile, strerror(errno));
      continue;
    }

    ComputeViceSHA(fd, testsha);
    close(fd);

    ViceSHAtoHex(testsha, shaprintbuf, sizeof(shaprintbuf));
    printf("SHA = %s\n", shaprintbuf);

    /* see if we can find this SHA in lookaside databases */

    strcpy(container, "/tmp/testlka.XXXXXX");
    cfd = mkstemp(container);
    if (cfd < 0) {
	printf("Can't create %s: %s\n", container, strerror(errno));
	exit(-1);
    }

    memset(em, 0, emlen); /* null message is default */
    if (LookAsideAndFillContainer(testsha, cfd, -1, NULL, em, emlen)){
      printf("Found match: %s\n", em);
    }
    else {
      if (em[0]) printf("LookAsideAndFillContainer: %s\n", em);
      else printf("sigh....no luck\n");
    }
    close(cfd);
    unlink(container);
  }

Quit:
  exit(0);
}


