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



#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "norton.h"
#include "parser.h"

void usage(char * name) {
    fprintf(stderr,
	    "Usage: %s [-mapprivate] <log_device> <data_device> <length>\n",
	    name);
}


int main(int argc, char * argv[]) {
    rvm_return_t 	err;
    int argstart;

    if (argc == 5 && strcmp(argv[1],"-mapprivate") == 0) {
      mapprivate = 1;
      argstart = 2;
      argc--;
    } else {
      mapprivate = 0;
      argstart = 1;
    }
    
    if (argc != 4) {
	usage(argv[0]);
	exit(1);
    }

    
    NortonInit(argv[argstart], argv[argstart+1], atoi(argv[argstart+2]));
    
    InitParsing();
    Parser_commands();

    err = rvm_terminate();
    printf("rvm_terminate returns %s\n", rvm_return(err));
    return 0;
}
