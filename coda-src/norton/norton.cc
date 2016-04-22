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



#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "norton.h"
#include "parser.h"
#include "vice_file.h"

void usage(char * name) {
    fprintf(stderr,
	    "Usage: %s [-n servernumber] [-mapprivate] <log_device> <data_device> <length>\n",
	    name);
}


int main(int argc, char * argv[])
{
    rvm_return_t err;
    mapprivate = 0;
    char *exename = argv[0];

    argc--; argv++;
    while (argc > 3) {
	if (strcmp(argv[0],"-mapprivate") == 0) {
	    mapprivate = 1;
	}
	argc--; argv++;
    }
    
    if (argc != 3) {
	usage(exename);
	exit(1);
    }

    vice_dir_init("/vice");
    NortonInit(argv[0], argv[1], atoi(argv[2]));
    
    InitParsing();
    Parser_commands();

    err = rvm_terminate();
    printf("rvm_terminate returns %s\n", rvm_return(err));
    return 0;
}
