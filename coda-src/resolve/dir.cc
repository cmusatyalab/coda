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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







/* 
 * Created 09/16/89 - Puneet Kumar
 * 
 * Test Program to see how directories work on vice directories on client side
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/dir.h>

#ifdef __cplusplus
}
#endif __cplusplus


main(argc, argv)
     int argc;
     char *argv[];
{
  DIR *dirp;
  struct direct *dp;

  printf("Looking at directory %s \n", argv[1]);
  dirp = opendir(argv[1]);
  if (dirp == NULL) {
    perror("opendir");
    exit(-1);
  }
  for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
    printf("inode_number = %d; rec_len = %d; namelen = %d; name = %s \n\n", dp->d_ino, dp->d_reclen, dp->d_namlen, dp->d_name);

  printf("\n\n END OF DIRECTORY \n");
}
