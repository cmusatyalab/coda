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

/********************************************************************************
 *
 * This module isolates code that determines the name of the command given a pid
 *  
 ********************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "coda_string.h"
#include <unistd.h>
#include <sys/param.h>

#if defined(__BSD44__)
#include <fcntl.h>
#include <kvm.h>
#include <sys/sysctl.h>
#include <limits.h>
#endif /* __BSD44__ */

#ifdef __cplusplus
}
#endif

#include <util.h>
#include "coda_assert.h"

#include "proc.h"

static char CommandString[MAXPATHLEN];
static int lastPid = -837;

void outputcommandname(FILE *file, int pid) {

    if (pid == lastPid)
      return;

    lastPid = pid;
    getcommandname(pid);
    fprintf(file, "%d %s\n", pid, CommandString);
    return;
}

void resetpid() {
  lastPid =  -837;
}

/*****************************************************************************
 *
 *  char *getcommandname(int pid);
 *
 *****************************************************************************/

#ifdef __BSD44__  

// This is the NetBSD way to do this.
char* getcommandname(int pid) {
#ifndef	__FreeBSD__
    kvm_t *KVM;
    struct kinfo_proc *proc_info;
    int num_procs;
    char error_message[_POSIX2_LINE_MAX];

    KVM = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, error_message);
    if (KVM == NULL) {
	strncpy(CommandString, "unknown", strlen("unknown"));
        return(CommandString);
    }

    proc_info = kvm_getprocs(KVM, KERN_PROC_PID, pid, &num_procs);
    kvm_close(KVM);

    if (num_procs == 0)
      strncpy(CommandString, "unknown", strlen("unknown"));
    else
      strncpy(CommandString, proc_info->kp_proc.p_comm, MAXPATHLEN);
    return(CommandString);
#else
strncpy(CommandString, "unknown", strlen("unknown"));
return CommandString;
#endif

}
/* __BSD44__ */


#elif defined(__linux__) || defined(sun)

// This is the Linux way to do this.  It ought to work for any other
// operating system that has a /proc file system, but I haven't tested
// any others.
char *getcommandname(int pid) {
    char path[BUFSIZ];
    char format[16];
    FILE *cmdFile;

    sprintf(path, "/proc/%d/cmdline", pid);
    cmdFile = fopen(path, "r");
    sprintf(format, "%%%ds", BUFSIZ);
    fscanf(cmdFile, format, CommandString);
    fclose(cmdFile);
    return(CommandString);
}
/* __linux__ */


#else 


/*
 *
 * Okay, if your system is not defined above, we haven't ported
 * getcommandname to your operating system.  That means, you get to.
 * First, see if your system has a /proc file system.  If so, you can add
 * your OS identifier to the __linux__ line above and things should just
 * start working.  Otherwise, you have two choices.  You can look for a
 * clean way to do this using system calls.  Or, you can grok the output
 * of the "ps" command, inelegant but easy.  If you choose the former
 * option, you're on your own.  You can check out the example above for
 * BSD44.  The BSD44 system call name was discovered through
 * "man -k process".  For Linux, I resorted to talking to a friend, who
 * showed me the nifty /proc file system.  (This was after becoming madly
 * confused because the ps sources use the openproc and readproc system
 * calls, which apparently don't exist anywhere for others to use.)
 * Anyway, like I said, you're on your own here.  If all else fails, you
 * can use a user-level program (ps) to get the necessary information.
 * I warn you, however, that you'll pay for this approach in performance!
 * The directions for this follow.
 * 
 *
 * This is an almost generic way to do this, using the ps program.  
 * 
 * This function assumes that the following command will identify the
 * command associated with a given pid, in this case 6036, and deposit
 * that command name in the file /tmp/command.  If this isn't true on
 * the system you're working on, then you're going to have to read the
 * ps manual page.  Ideally, you want it to return precisely two lines.
 * The first line should be a bunch of labels.  The second line should
 * be the line describing the process whose pid is specified.  Under
 * NetBSD, the command "ps axc -p <pid>" does precisely this.  The
 * output is:
 *
 *          PID TT  STAT      TIME COMMAND
 *         6036 p2  I      1:54.50 netscape
 *
 *
 * Next, you're going to have to figure out what column number contains
 * the command name, in this case "netscape".  Here it's column number 5.
 * 
 * Finally, you'll need to change the commandString below.  Currently, 
 * it contains
 *
 *      ps axc 6036 | awk '{ if ($5 == "COMMAND") next; print $5 > "/tmp/command"}'
 *
 * This command takes the output of ps described above and pipes it
 * through an awk script that eats the first lien and outputs the 5th
 * column of the second line.  Whammo!  We've got the command name.
 * Granted, it isn't terribly elegant or efficient, but at least it 
 * ought to work on any Unix-based system with minimal effort.
 */

char* getcommandname(int pid) {
    char tmpfile[MAXPATHLEN];
   int rc;
    FILE *f;

    snprintf(tmpfile, MAXPATHLEN, "/tmp/advice_srv.%d", pid);
        
    snprintf(CommandString, MAXPATHLEN, "ps axc -p %d | awk '{ if ($5 == \"COMMAND\") next; print $5 > \"%s\"}'", pid, tmpfile);

    rc = system(CommandString);
    if (rc) {
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }

    f = fopen(tmpfile, "r");
    if (f == NULL) {
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }
    rc = fscanf(f, "%s", CommandString);
    if (rc != 1) {
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }
    fclose(f);
    unlink(tmpfile);
    return(CommandString);
}


#endif /* getcommandname */

