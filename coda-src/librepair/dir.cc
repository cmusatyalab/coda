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
                           none currently

#*/







/* 
 * Created 09/16/89 - Puneet Kumar
 * 
 * Test Program to see how directories work on vice directories on client side
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

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
    exit(EXIT_FAILURE);
  }
  for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
    printf("inode_number = %d; rec_len = %d; namelen = %d; name = %s \n\n", dp->d_ino, dp->d_reclen, dp->d_namlen, dp->d_name);

  printf("\n\n END OF DIRECTORY \n");
}
