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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/vice/codamon.cc,v 4.2 98/08/31 12:23:33 braam Exp $";
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


/* 
 * codamon.c	- Coda Server Monitor
 * 
 * Function	- Restart the file server after a shutdown 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>


int ShutDown = 0;
static void	Terminate();

main(int argc, char **argv, char **envp) {

    if (chdir("/vice/srv")) {
	printf("Couldn't cd to /vice/srv - exiting\n");
	exit(-1);
    }
    signal(SIGQUIT, (void (*)(int))Terminate);

    freopen("codasrvmon", "a+", stdout);
    freopen("codasrvmon", "a+", stderr);

    LogMsg(-1, SrvDebugLevel, stdout, "Server Monitor Started");
    while (!ShutDown) {
	LogMsg(-1, SrvDebugLevel, stdout, "Coda Server Monitor: forking server /vice/bin/srv...");
	int child = fork();
	if (child) {
	    union wait pstatus;
	    while(child != wait(&pstatus));
	    if (pstatus.w_retcode == 51) {
		LogMsg(-1, SrvDebugLevel, stdout, "Server with pid %d killed voluntarily by RVM; code = 51", child);
	    }
	    else {
		LogMsg(-1, SrvDebugLevel, stdout, "Server with pid %d finished; status = %d", child, pstatus.w_retcode);
	    }
	    sleep(60);
	}
	else
	    execve("/vice/bin/srv", argv, envp);
    }
}

static void	Terminate()

{
    struct timeval tp;
    struct timezone tsp;
    gettimeofday( &tp, &tsp);
    
    LogMsg(-1, SrvDebugLevel, stdout, "Shutdown File Server Monitor");
    ShutDown = 1;
}
