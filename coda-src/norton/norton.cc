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
    fprintf(stderr, "Usage: %s <log_device> <data_device> <length>\n",
	    name);
}


int main(int argc, char * argv[]) {
    rvm_return_t 	err;
    
    if (argc != 4) {
	usage(argv[0]);
	exit(1);
    }

    
    NortonInit(argv[1], argv[2], atoi(argv[3]));
    
    InitParsing();
    Parser_commands();

    err = rvm_terminate();
    printf("rvm_terminate returns %s\n", rvm_return(err));
    return 0;
}
