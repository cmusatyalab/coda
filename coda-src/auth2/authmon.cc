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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/auth2/Attic/authmon.cc,v 4.2 1997/02/26 16:02:32 rvb Exp $";
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
authmon.c -- watchdog process to restart authentication server

*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

int main(int argc, char **argv, char **envp);
PRIVATE void InitGlobals(int argc, char **argv);
PRIVATE void ShutDown();
PRIVATE void Continue();
PRIVATE void Pause();

PRIVATE int ShutDownFlag = 0;	/* if 1 do not refork child; exit when child dies */
PRIVATE int PauseFlag = 0;	/* if 1 do not fork child, but do not exit */
PRIVATE char *AuthDir = "/vice/auth2";
PRIVATE int DoFork = 1;


int main(int argc, char **argv, char **envp)
    {
    FILE *file;
    int child;

    if (DoFork && fork()) exit(0);	/* disassociate from controlling tty */
    InitGlobals(argc, argv);

    if (chdir(AuthDir))
	{
	if(mkdir(AuthDir, 0777))
	    {
	    printf("Could not cd or mkdir %s ....... exiting\n", AuthDir);
	    exit(-1);
	    }
	else
	    {
	    printf("Created %s\n",AuthDir);
	    if(chdir(AuthDir))
		{
		printf("Could not cd to %s even after a mkdir\n", AuthDir);
		}
	    }
	}

    freopen("AuthLog", "a+", stdout);
    freopen("AuthLog", "a+", stderr);

    if((file = fopen("monpid", "w")) == NULL)
	{
	perror("monpid");
	exit(-1);
	}
    else
	{
	fprintf(file, "%d", getpid());
	fclose(file);
	}

    (void) signal(SIGTERM, (void (*)(int))ShutDown);	/* shutdown after child terminates */
    (void) signal(SIGPIPE, (void (*)(int))Continue);	/* start forking childen again */
    (void) signal(SIGTSTP, (void (*)(int))Pause);	/* stop forking children */

    LogMsg(-1, 0, stdout, "Auth Monitor started\n");
    while (!ShutDownFlag)
	{
	static int notfirst = 0;
	if (PauseFlag) 
	    {
	    LogMsg(-1, 0, stdout, "Auth Monitor pausing\n");
	    pause();
	    notfirst = 0;
	    continue;
	    }

	if (notfirst++) sleep(30);	/* to allow system to quiesce  */
	if (ShutDownFlag) break;	/* could have arrived during sleep */
	LogMsg(-1, 0, stdout, "Auth Monitor forking Auth Server .....\n");
	child = fork();
	if(child)
	    while(child != wait(0));
	else
	    execve("/vice/bin/auth2", argv, envp);
	}
    LogMsg(-1, 0, stdout, "Auth Monitor shutdown complete\n");
    return(0);
    }


PRIVATE void InitGlobals(int argc, char **argv)
    /* Set globals from command line */
    {
    register int i;
    int len;

    len = strlen(argv[0]);
    for(i=0;i<len;i++)
	*(argv[0]+i) = ' ';
    strcpy(argv[0],"authmon");
    for (i = 1; i < argc; i++)
	{
	if (strcmp(argv[i], "-d") == 0 && i < argc - 1)
	    {
	    AuthDir = argv[++i];
	    continue;
	    }
	/* skip the following arguments that are used by auth2 - this allows	*/
	/* parameters on the command line to be passed through to auth2		*/
	if (strcmp(argv[i], "-x") == 0)
	    {
	    i++;
	    continue;
	    }
	if (strcmp(argv[i], "-r") == 0)
	    {
	    continue;
	    }
	if(strcmp(argv[i], "-chk") == 0)
	    {
	    continue;
	    }
	if(strcmp(argv[i], "-p") == 0)
	    {
	    i++;
	    continue;
	    }
	if(strcmp(argv[i], "-tk") == 0)
	    {
	    i++;
	    continue;
	    }
	if (strcmp(argv[i], "-fk") == 0 && i < argc - 1)
	    {
	    i++;
	    continue;
	    }

	fprintf(stderr, "Usage: authmon [-d chdir] [-r] [-chk] [-x debuglevel] [-p pwfile] [-tk tokenkey] [-fk filekey]\n");
	exit(-1);
	}
    }


PRIVATE void ShutDown()
    {
    LogMsg(-1, 0, stdout, "Auth Monitor received shutdown\n");
    ShutDownFlag = 1;
    }


PRIVATE void Continue()
    {
    LogMsg(-1, 0, stdout, "Auth Monitor received continue\n");
    PauseFlag = 0;
    }


PRIVATE void Pause()
    {
    LogMsg(-1, 0, stdout, "Auth Monitor received pause\n");
    PauseFlag = 1;
    }
