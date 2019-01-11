#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif /*_BLURB_*/

//
// mondmon.c
//
// watchdogs the mond data collector
//
//

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include "coda_string.h"
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"

#define STREQ(a, b) (strcmp((a), (b)) == 0)

void CheckSyntax(int, char **);
void ShutDown(void);
void Pause(void);
void Continue(void);

int shutdown = 0;
int dopause  = 0;

main(int argc, char *argv[])
{
    int notfirst = 0;

    LogMsg(0, 0, stdout, "Mondmon: log started, pid = %d", getpid());
    CheckSyntax(argc, argv);
    if (argc < 1)
        exit(EXIT_FAILURE);
    (void)signal(SIGTERM, (void (*)(int))ShutDown);
    (void)signal(SIGPIPE, (void (*)(int))Continue);
    (void)signal(SIGTSTP, (void (*)(int))Pause);

    while (!shutdown) {
        if (dopause) {
            LogMsg(0, 0, stdout, "Mondmon: paused");
            pause();
            notfirst = 0;
            LogMsg(0, 0, stdout, "Mondmon: continued");
            continue;
        }
        argv[0] = "mond";
        LogMsg(0, 0, stdout, "Mondmon: Attempting to start mond");
        int child = fork();
        if (child) {
            notfirst++;
            while (child != wait(0))
                ;
            LogMsg(0, 0, stdout, "Mondmon: mond died");
        } else {
            if (execve("/usr/mond/bin/mond", argv, 0)) {
                LogMsg(0, 0, stdout, "Monmon: execve mond failed");
                exit(EXIT_SUCCESS);
            }
        }
        if (notfirst) {
            sleep(30);
            continue;
        }
    }
    LogMsg(0, 0, stdout, "Mondmon: finished");
}

void CheckSyntax(int argc, char *argv[])
{
    /* check the command line syntax to make sure mond won't choke */
    for (int i = 1; i < argc; i++) {
        if ((STREQ(argv[i], "-r")) || (STREQ(argv[i], "-R"))) {
            continue;
        }
        if (STREQ(argv[i], "-nospool")) {
            continue;
        }
        if (STREQ(argv[i], "-wd")) { /* working directory */
            i++;
            continue;
        } else if (STREQ(argv[i], "-mondp")) { /* vmon/smon port */
            i++;
            continue;
        } else if (STREQ(argv[i], "-d")) { /* debug */
            i++;
            continue;
        } else if (STREQ(argv[i], "-b")) { /* buffer size */
            i++;
            continue;
        } else if (STREQ(argv[i], "-l")) { /* listeners */
            i++;
            continue;
        } else if (STREQ(argv[i], "-w")) { /* low water mark */
            i++;
            continue;
        } else if (STREQ(argv[i], "-ui")) { /* low water mark */
            i++;
            continue;
        } else {
            LogMsg(0, 0, stdout, "Unrecognized command line option, %s\n",
                   argv[i]);
            printf(
                "usage: mondmon [[-wd workingdir] [-mondp port number] [-d debuglevel]\n");
            printf(
                "                [-b buffersize] [-l listeners] [-w lowWaterMark]\n");
            printf("                [-ui utility interval]\n");
            exit(EXIT_FAILURE);
        }
    }
}

void Pause(void)
{
    LogMsg(0, 0, stdout, "Mondmon: pausing");
    dopause = 1;
}

void Continue(void)
{
    LogMsg(0, 0, stdout, "Mondmon: continuing");
    dopause = 0;
}

void ShutDown(void)
{
    LogMsg(0, 0, stdout, "Mondmon: exiting");
    shutdown = 1;
}
