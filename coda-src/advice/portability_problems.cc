/**************************************************************************************
 *
 *  This file isolates the portability problems contained in the CodaConsole.
 *  
 **************************************************************************************/

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

#include "globals.h"

char CommandString[MAXPATHLEN];

/**************************************************************************************
 *
 *  char *getcommandname(int pid);
 *
 **************************************************************************************/

#ifdef  __MACH__
// This is the Mach-specific way to do this. 
#define ARGSIZE 4096
char* getcommandname(int pid) {
        char arguments[ARGSIZE];
        int *ip; 
        register char   *cp;
        char            c;
        char            *end_argc;
        int rc;

        if ((rc = table(TBL_ARGUMENTS, pid, arguments, 1, ARGSIZE)) != 1) {
            LogMsg(0,LogLevel,LogFile,"getcommandname (MACH version) could not get command name:");
            LogMsg(0,LogLevel,EventFile,"  table: %d\n", rc);
            return((char *)0);
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
//        return(cp);
}
/* __MACH__ */

#elif __BSD44__  
// This is the NetBSD way to do this.
const int BufSize = 1024;
char* getcommandname(int pid) {
    kvm_t *KVM;
    struct kinfo_proc *proc_info;
    int num_procs;
    char error_message[_POSIX2_LINE_MAX];

    KVM = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, error_message);
    if (KVM == NULL) {
	LogMsg(0,LogLevel,LogFile, "getcommandname (BSD44 version) could not get command name:");
	LogMsg(0,LogLevel,LogFile, "  error msg: %s\n", error_message);
	return((char *)0);
    }

    proc_info = kvm_getprocs(KVM, KERN_PROC_PID, pid, &num_procs);
    kvm_close(KVM);
    strncpy(CommandString, proc_info->kp_proc.p_comm, MAXPATHLEN);
    return(CommandString);
}
/* __BSD44__ */

#elif __linux__
// This is the Linux way to do this.  It will also work for any other
// operating system that has a /proc file system.
const int BufSize = 1024;
char *getcommandname(int pid) {
    char path[BufSize];
    char format[16];
    FILE *cmdFile;

    sprintf(path, "/proc/%d/cmdline", pid);
    cmdFile = fopen(path, "r");
    sprintf(format, "%%%ds", BufSize);
    fscanf(cmdFile, format, CommandString);
    return(CommandString);
}

#else 

/*
 * Okay, if your system is not defined above, we haven't ported getcommandname
 * to your operating system.  That means, you get to.  First, see if your system
 * has a /proc file system.  If so, you can add your OS identifier to the __linux__
 * line above and things should just start working.  Otherwise, you have two
 * choices.  You can look for a clean way to do this using system calls.  Or, you
 * can grok the output of the "ps" command, inelegant but easy.  If you choose
 * the former option, you're on your own.  You can check out the examples above 
 * for BSD44 and MACH.  The BSD44 system call name was discovered through "man -k process".
 * For Linux, I resorted to talking to a friend, who showed me the nifty /proc
 * file system.  (This was after becoming madly confused because the ps sources
 * use the openproc and readproc system calls, which apparently don't exist anywhere
 * for others to use.)  Anyway, like I said, you're on your own here.  If all else
 * fails, you can use a user-level program (ps) to get the necessary information.
 * The directions for this follow.
 *
 *
 * This is an almost generic way to do this, using the ps program.  
 *
 * This function assumes that the following command will identify the command
 * associated with a given pid, in this case 6036, and deposit that command name
 * in the file /tmp/command.  If this isn't true on the system you're working on, 
 * then you're going to have to read the ps manual page.  Ideally, you want it
 * to return precisely two lines.  The first line should be a bunch of labels.
 * The second line should be the line describing the process whose pid is specified.
 * Under NetBSD, the command "ps axc -p <pid>" does precisely this.  The output is:
 *
 *          PID TT  STAT      TIME COMMAND
 *         6036 p2  I      1:54.50 netscape
 *
 * Next, you're going to have to figure out what column number contains the command 
 * name, in this case "netscape".  Here it's column number 5.
 * 
 * Finally, you'll need to change the commandString below.  Currently, it contains
 *
 *      ps axc 6036 | awk '{ if ($5 == "COMMAND") next; print $5 > "/tmp/command"}'
 *
 * This command takes the output of ps described above and pipes it through an
 * awk script that eats the first lien and outputs the 5th column of the second
 * line.  Whammo!  We've got the command name.  We're done!  (Granted, it isn't
 * terribly elegant or efficient.  However, I could explain a way to get the information
 * that'll work on any Unix-based system, with minimal effort.)
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
	LogMsg(0,LogLevel,LogFile, "getcommandname (generic version) could not get command name:");
	LogMsg(0,LogLevel,LogFile, "  error message: could not system the command\n");
        return(NULL);
    }

    f = fopen(tmpfile, "r");
    if (f == NULL) {
	LogMsg(0,LogLevel,LogFile, "getcommandname (generic version) could not get command name:");
	LogMsg(0,LogLevel,LogFile, "  error message: could not open the resulting file\n");
	return(NULL);
    }
    rc = fscanf(f, "%s", CommandString);
    if (rc != 1) {
	LogMsg(0,LogLevel,LogFile, "getcommandname (generic version) could not get command name:");
	LogMsg(0,LogLevel,LogFile, "  error message: could not scan the resulting file\n");
	return(NULL);
    }
    fclose(f);
    unlink(tmpfile);
    return(CommandString);
}


#endif /* getcommandname */




/************************************************************************************** 
 *
 *  void path(char *pathname, char *directory, char *file);
 *
 **************************************************************************************/

//#if defined(__linux__) || defined(__BSD44__)

/* An implementation of path(3) which is a standard function in Mach OS
 * the behaviour is according to man page in Mach OS, which says,
 *
 *    The handling of most names is obvious, but several special
 *    cases exist.  The name "f", containing no slashes, is split
 *    into directory "." and filename "f".  The name "/" is direc-
 *    tory "/" and filename ".".  The path "" is directory "." and
 *    filename ".".
 *       -- manpage of path(3)
 */
#include <string.h>

void path(char *pathname, char *direc, char *file)
{
  char *maybebase, *tok;
  int num_char_to_be_rm;

  if (strlen(pathname)==0) {
    strcpy(direc, ".");
    strcpy(file, ".");
    return;
  }
  if (strchr(pathname, '/')==0) {
    strcpy(direc, ".");
    strcpy(file, pathname);
    return;
  } 
  if (strcmp(pathname, "/")==0) {
    strcpy(direc, "/");
    strcpy(file, ".");
    return;
  }
  strcpy(direc, pathname);
  maybebase = strtok(direc,"/");
  while (tok = strtok(0,"/")) 
    maybebase = tok;
  strcpy(file, maybebase);
  strcpy(direc, pathname);
  num_char_to_be_rm = strlen(file) + 
    (direc[strlen(pathname)-1]=='/' ? 1 : 0);/* any trailing slash ? */
  *(direc+strlen(pathname)-num_char_to_be_rm) = '\0';
    /* removing the component for file from direc */
  if (strlen(direc)==0) strcpy(direc,"."); /* this happen when pathname 
                                            * is "name/", for example */
  if (strlen(direc)>=2) /* don't do this if only '/' remains in direc */
    if (*(direc+strlen(direc)-1) == '/' )
      *(direc+strlen(direc)-1) = '\0'; 
       /* remove trailing slash in direc */
  return;
}

//#endif /* __linux__ || __BSD44__ */


