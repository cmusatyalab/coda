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

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

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
#ifdef sun
#include "/usr/ucbinclude/sys/wait.h"
#else
#include <sys/wait.h>
#endif
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
