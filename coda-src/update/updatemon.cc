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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/update/updatemon.cc,v 1.1 1996/11/22 19:14:51 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdarg.h>
#include <libc.h>
#include <sysent.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#ifdef __cplusplus
}
#endif __cplusplus

static struct timeval  tp;

#define MONITORNAME "UpdateMonitor"

main(int argc, char *argv[], char *envp[])
{
    FILE * file;
    int    child;
    register int    i;
    register int    len, lenmon;

    gettimeofday(&tp, 0);

    if(strcmp(argv[1], "-s") == 0) { /* starting updatesrv */
	chdir("/vice");
	for(i = 2; i < argc ; i++) { /* chdir to prefix operand if it exists */
	    if(strcmp(argv[i],"-p") == 0) {
		chdir(argv[i+1]);
		break;
	    }
	}
    }
    else { /* starting updateclnt */
	chdir("/vice/srv");
    }

    if(!(file = fopen("UpdateMonitor", "w"))) {
	printf("Could not open UpdateMonitor at %s\n", ctime((long *)&tp.tv_sec));
	exit(-1);
    }
    
    fprintf(file, "%d", getpid());
    fclose(file);

    child = fork();
    if (child) {
	len = strlen(argv[0]);
	lenmon = strlen(MONITORNAME);
	for(i = 0; i < len; i++) {
	    if(i < lenmon)
		*(argv[0]+i) = MONITORNAME[i];
	    else
		*(argv[0]+i) = ' ';
	}

	while (child != wait(0));
	gettimeofday(&tp, 0);
	printf("UpdateMonitor is Restarting at %s", ctime((long *)&tp.tv_sec));
	execve("/vice/bin/updatemon", argv, envp);
    }
    gettimeofday( &tp, 0);

    if(argc > 1 && (strcmp(argv[1],"-s") == 0)) {
	execve("/vice/bin/updatesrv", argv, envp);
    }
    else {
	execve("/vice/bin/updateclnt", argv, envp);
    }

}
