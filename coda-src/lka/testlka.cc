/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


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

#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

#include <codaconf.h>
#include "lka.h"

char em[4096];
int emlen = sizeof(em);

main(int argc, char **argv) {

  int rc;
  char testfile[MAXPATHLEN];
  unsigned char testsha[SHA_DIGEST_LENGTH];
  char container[30], shaprintbuf[60]; 
  
  strcpy(container, "/tmp/testlka.XXXXXX");
  mktemp(container);

  switch (argc) {
  case 2:
    rc = LKParseAndExecute(argv[1], em, emlen);
    if (em[0]) printf(em); /* hopefully useful msg */
    if (!rc) exit(-1); 
    else break;

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
    int fd = open (testfile,O_RDONLY, 0);
    if (fd < 0) {
      printf("%s: %s\n", testfile, strerror(errno));
      continue;
    }
    if (!ComputeViceSHA(fd, testsha)) {
      printf("%s: can't compute SHA", testfile);
      close(fd);
      continue;    
    }

    close(fd);

    ViceSHAtoHex(testsha, shaprintbuf, sizeof(shaprintbuf));
    printf("SHA = %s\n", shaprintbuf);

    /* see if we can find this SHA in lookaside databases */

    int cfd = open(container, O_CREAT|O_TRUNC|O_WRONLY, 0644); /* create file */
    if (cfd < 0) {
	printf("Can't create %s: %s\n", container, strerror(errno));
	exit(-1);
    }

    memset(em, 0, emlen); /* null message is default */
    if (LookAsideAndFillContainer(testsha, cfd, -1, 0, em, emlen)){
      printf("Found match: %s\n", em);
      close(cfd);
      continue;
    }
    else {
      if (em[0]) printf("LookAsideAndFillContainer: %s\n", em);
      else printf("sigh....no luck\n");
      close(cfd);
      continue;
    }
  }

Quit:
  unlink(container);
  exit(0);
}


