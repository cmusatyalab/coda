/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

extern "C" {

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

}
int MountedAtRoot(char *path)
    /* Returns 1 if path is mounted at "/", 0 otherwise */
    {
    struct stat rootbuf, pathbuf;

    /* Check exactly one slash, and in first position */
    if (rindex(path, '/') != path) return(0);

    /* Then compare root and path device id's */
    if (stat("/", &rootbuf))
	{
	perror("/");
	return(0);
	}
    if (stat(path, &pathbuf))
	{
	perror(path);
	return(0);
	}

    if (rootbuf.st_dev == pathbuf.st_dev) return(0);
    else return(1);
    }

main(int argc, char *argv[])
    {
    if (argc != 2) 
	{
	printf("Usage: testmount <path>\n");
	exit (-1);
	}
    if (MountedAtRoot(argv[1])) printf("Mounted at root\n");
    else printf("Not mounted at root\n");
    }


