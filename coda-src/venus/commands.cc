/*****************************************************************************
 *
 * This file isolates code that determines the name of the command given a pid
 *  
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>

#if defined(__BSD44__)
#include <fcntl.h>
#include <kvm.h>
#include <sys/sysctl.h>
#include <limits.h>
#endif /* __BSD44__ */

#ifdef __MACH__
#include <sys/table.h>
extern int table(int id, int index, char *addr, int nel, int lel); 
#endif /* __MACH__ */

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>

#include "venus.private.h"
#include "commands.h"

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

#ifdef  __MACH__
#define ARGSIZE 4096
char* getcommandname(int pid) {
    char arguments[ARGSIZE];
    int *ip; 
    register char   *cp;
    char            c;
    char            *end_argc;
    int rc;

    if ((rc = table(TBL_ARGUMENTS, pid, arguments, 1, ARGSIZE)) != 1) {
      LOG(0, ("getcommandname (MACH version) could not get command name: %d\n", rc));
      strncpy(CommandString, "unknown", strlen("unknown"));
      return(CommandString);
    }

    end_argc = &arguments[ARGSIZE];

    ip = (int *)end_argc;
    /* words must be word aligned! */
    if ((unsigned)ip & 0x3)
      ip = (int*)((unsigned)ip & ~0x3);
#ifdef  mips
    /* one exception frame worth of zeroes too */
    ip -= 10; /* EA_SIZE bytes */
#endif  mips
    ip -= 2;                /* last arg word and .long 0 */
    while (*--ip)
      if (ip == (int *)arguments)
	return((char *)0);
    
    *(char *)ip = ' ';
    ip++;
    for (cp = (char *)ip; cp < end_argc; cp++) {
      c = *cp & 0177;
      if (c == 0)
	break;  
      else if (c < ' ' || c > 0176) 
	*cp = '?';
    }
    *cp = 0;
    cp = (char *)ip;
    strncpy(CommandString, cp, MAXPATHLEN);
    return(CommandString);
}
/* __MACH__ */

#elif __BSD44__  

// This is the NetBSD way to do this.
char* getcommandname(int pid) {
    kvm_t *KVM;
    struct kinfo_proc *proc_info;
    int num_procs;
    char error_message[_POSIX2_LINE_MAX];

    KVM = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, error_message);
    if (KVM == NULL) {
        LOG(0, ("getcommandname (BSD44 version) could not get command name: %s\n", error_message));
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
}
/* __BSD44__ */

#elif __linux__
// This is the Linux way to do this.  It will also work for any other
// operating system that has a /proc file system.
char *getcommandname(int pid) {
    char path[BUFSIZ];
    char format[16];
    FILE *cmdFile;

    sprintf(path, "/proc/%d/cmdline", pid);
    cmdFile = fopen(path, "r");
    sprintf(format, "%%%ds", BUFSIZ);
    fscanf(cmdFile, format, CommandString);
    return(CommandString);
}

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
 * option, you're on your own.  You can check out the examples above for
 * BSD44 and MACH.  The BSD44 system call name was discovered through
 * "man -k process".  For Linux, I resorted to talking to a friend, who
 * showed me the nifty /proc file system.  (This was after becoming madly
 * confused because the ps sources use the openproc and readproc system
 * calls, which apparently don't exist anywhere for others to use.)
 * Anyway, like I said, you're on your own here.  If all else fails, you
 * can use a user-level program (ps) to get the necessary information.
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
    int thisPID;
   int rc;
    FILE *f;

    thisPID = getpid();
    snprintf(tmpfile, MAXPATHLEN, "/tmp/advice_srv.%d", thisPID);
        
    snprintf(CommandString, MAXPATHLEN, "ps axc -p %d | awk '{ if ($5 == \"COMMAND\") next; print $5 > \"%s\"}'", pid, tmpfile);

    rc = system(CommandString);
    if (rc) {
	LOG(0, ("getcommandname (generic version) could not get command name: could not system the command\n"));
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }

    f = fopen(tmpfile, "r");
    if (f == NULL) {
	LOG(0, ("getcommandname (generic version) could not get command name: could not open the resulting file\n"));
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }
    rc = fscanf(f, "%s", CommandString);
    if (rc != 1) {
	LOG(0, ("getcommandname (generic version) could not get command name: could not scan the resulting file\n"));
	strncpy(CommandString, "unknown", strlen("unknown"));
	return(CommandString);
    }
    fclose(f);
    unlink(tmpfile);
    return(CommandString);
}


#endif /* getcommandname */

