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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/advice/RCS/advice_srv.cc,v 4.1 1997/01/08 21:49:15 rvb Exp $";
#endif /*_BLURB_*/






/*  
 *  Clean up activities:
 *      1) Create a single file system error routine (see CreateDataDirectory).
 *      2) Create an advice.err file to log known errors for entry into the database
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef	__MACH__
#include <sys/table.h>
extern void path(char *, char *, char *); /* defined in a Mach library */
#endif /* MACH */
#if defined(__linux__) || defined(__BSD44__)
void path(char *, char *, char *); /* defined locally */
#endif /* __linux__ || __BSD44__ */
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <assert.h> 
#include <struct.h>
#include <sys/wait.h> 
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/dir.h>
#include <pwd.h>
#include <errno.h>
#include <strings.h>
#include <lwp.h>
#include <rpc2.h>

extern int table(int id, int index, char *addr, int nel, int lel);
extern int ffilecopy(FILE*, FILE*);
extern char **environ;

#ifdef __cplusplus
}
#endif __cplusplus

extern int myfilecopy(int,int);
extern void DispatchDaemons();
extern void DaemonInit();
extern void RegisterDaemon(unsigned long, char *);

#include <util.h>
#include <bstree.h>
#include "admon.h"
#include "adsrv.h"
#include <advice.h>
#include "advice_srv.h"
#include "miss.h"
#include "volent.h"

int ASRinProgress = 0;
int ReconnectionQuestionnaireInProgress = 0;

int CHILDpid = 0;
int CHILDresult = -1;

int NumASRStarted = 0;
int NumRecoSurveys = 0;
int NumDiscoMissSurveys = 0;

/***** Function Definitions *****/
void InitPGID();
void InitEnvironment();
void InitHostName();
void InitUserData();
void InitCounters();
void IncrementCounter();
void InitLogFile();
void InitEventFile();
void InitDataDirectory();
void PrintMissList(char *filename);
void InitMissQueue();
void InitVDB();
void ErrorReport(char *);
void PrintUsage();
void CreateREADMEFile(char *);
void InitializeDirectory(char *, char *);
void SetAccessLists(char *, char *);
void CommandLineArgs(int, char **);
void InitiateNewUser();
RPC2_Handle connect_to_machine(char *_name);
void Init_RPC();
void InformVenusOfOurExistance(char *);
int SetupDefaultAdviceRequests();
int GetAdvice(char *);
void Shutdown(int);
void Child(int);
void UserDrivenEvent(int);
void WorkerHandler();
void DataHandler();
void UserEventHandler(char *);
void ShutdownHandler(char *);
void EndASREventHandler(char *);
void KeepAliveHandler(char *);
int execute_tcl(char *script, char *args[]);
int fork_tcl(char *script, char *args[]);
//int TestDiscoMiss();
StoplightStates StoplightStateChange(StoplightStates newState);

/* Command Line Variables */
int debug = 0;
int verbose = 0;
int AutoReply = 1;               /* -a will set (or unset) this value */ 
int LogLevel = 0;                /* -d <int> */
int StackChecking = 0;		 /* -s */

/* RPC Variables */
extern RPC2_PortalIdent rpc2_LocalPortal;
char ShortHostName[MAXHOSTNAMELEN];
char HostName[MAXHOSTNAMELEN];
RPC2_Handle VenusCID = -1;

RPC2_Handle cid;
RPC2_RequestFilter reqfilter;
RPC2_PacketBuffer *reqbuffer;

/* Error checking variables */
int ReadError = 0;
int WriteError = 0;

/* Stack Checking Variables */
int max, used;
int oldworkerused;

const int timeLength = 26;
const int smallStringLength = 64;

typedef struct {
int presentedCount;
int requestedCount;
} InternalCounter;

/* Internal Counters */
InternalCounter PCMcount;
InternalCounter HWAcount;
InternalCounter DMcount;
InternalCounter Rcount;
InternalCounter RPcount;
InternalCounter IASRcount;
InternalCounter WCMcount;
InternalCounter LCcount;
InternalCounter VSCcount;

/* Internal Variables */
extern int errno;
char error_msg[BUFSIZ];

char UserName[16];
int uid = -1;
int thisPID;
int thisPGID;

char BaseDir[MAXPATHLEN];
char WorkingDir[MAXPATHLEN];
char tmpDir[MAXPATHLEN];

char tmpReconnectionFileName[MAXPATHLEN];
char ReconnectionFileName[MAXPATHLEN];

int WeLostTheConnection = FALSE;
int AwaitingUserResponse = FALSE;

int userdrivenpid, asrendpid, shutdownpid, keepalivepid, datapid, mainpid, workerpid;
;
char shutdownSync;
char dataSync;
char keepaliveSync;
char initialUserSync;
char userSync;
char workerSync;

/* Interest and Monitor Variables */
int StoplightInterest = FALSE;
StoplightStates CurrentStoplightState = SLquit;


/* Venus Version Information */
int VenusMajorVersionNumber;
int VenusMinorVersionNumber;

FILE *LogFile;
char LogFileName[MAXPATHLEN];       /* DEFAULT: /coda/usr/<username>/.questionnaires/advice.log */

FILE *EventFile;
char EventFileName[MAXPATHLEN];     /* DEFAULT: /coda/usr/<username>/.questionnaires/advice.event */

/******************************************************************
 *************************  MAIN Routine  *************************
 ******************************************************************/

main(int argc, char *argv[])
{
  long rc;
  char c, s;

  // Initialization
  thisPID = getpid();               // Get the pid 
  InitPGID();			    // Give us our very own pgid
  InitEnvironment();		    // Make sure the environment variables are set...
  InitHostName();                   // Get our HostName for later use...
  InitUserData();                   // Get uid and UserName (must preceed CommandLineArgs)
  CommandLineArgs(argc, argv);      // Parse command line arguments
  InitDataDirectory();              // Create a unique directory in which to store data
  InitLogFile();                    // Open the LogFile
  InitEventFile();		    // Open the EventFile
  InitMissQueue();
  InitVDB();

  Init_RPC();                                 // Initialize RPC...
  assert(IOMGR_Initialize() == LWP_SUCCESS);  // and IOMGR
  DaemonInit();		                      // and the daemon package


  /****************************************************
   ******   Create LWPs to handle signal events  ******
   ****************************************************/

  // SIGCHLD signals the completion of an ASR.
  assert((LWP_CreateProcess((PFIC)EndASREventHandler,
			   DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "ASR End Signal Handler",
			   (PROCESS *)&asrendpid)) == (LWP_SUCCESS));

  // SIGTERM signals the AdviceMonitor to shut down
  assert(LWP_CreateProcess((PFIC)ShutdownHandler,
			   DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY+1,
			   (char *)&s, "Shutdown Handler",
			   (PROCESS *)&shutdownpid) == LWP_SUCCESS);

  // SIGUSR1 signals user-driven interaction.
  assert((LWP_CreateProcess((PFIC)UserEventHandler,
			    DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "User-driven Event Handler",
			   (PROCESS *)&userdrivenpid)) == (LWP_SUCCESS));

  // Initialize signal handlers
  signal(SIGTERM, Shutdown);
  assert(!signal(SIGCHLD, Child));
  signal(SIGUSR1, UserDrivenEvent);

  /*******************************************************
   ******  Create LWPs to handle daemon activities  ******
   *******************************************************/
  assert((LWP_CreateProcess((PFIC)KeepAliveHandler,
			    DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY-1,
			   (char *)&c, "Keep Alive Handler",
			   (PROCESS *)&keepalivepid)) == (LWP_SUCCESS));

  assert((LWP_CreateProcess((PFIC)DataHandler,
			    DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY-1,
			   (char *)&c, "Data Handler",
			   (PROCESS *)&datapid)) == (LWP_SUCCESS));
   
  /****************************************************
   ******    Create LWP to handle venus events   ******
   ****************************************************/

  assert((LWP_CreateProcess((PFIC)WorkerHandler,
			    DFTSTACKSIZE*2*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "Worker Handler",
			   (PROCESS *)&workerpid)) == (LWP_SUCCESS));


  // Inform Venus of the availability of an AdviceMonitor for this user
  InformVenusOfOurExistance(HostName);

  // Setup the default user interests
  assert(LWP_NoYieldSignal(&initialUserSync) == LWP_SUCCESS);

  // Set up the request filter:  we only service ADMON subsystem requests.
  reqfilter.FromWhom = ANY;
  reqfilter.OldOrNew = OLDORNEW;
  reqfilter.ConnOrSubsys.SubsysId = ADMONSUBSYSID;

  // Loop forever, waiting for Venus to ask us for advice
  LogMsg(100,LogLevel,LogFile, "main:  Entering Request Loop");
  struct timeval DaemonExpiry;
  DaemonExpiry.tv_sec = 5;
  DaemonExpiry.tv_usec = 0;
  if (LogLevel==1000) RPC2_DebugLevel = 1000;
  for ( ; ; ) {
    if (WeLostTheConnection)
      exit(0);

    // Wait for a request
    rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, &DaemonExpiry, NULL, NULL, NULL) ;

    if (rc == RPC2_TIMEOUT) {
      // Fire daemons that are ready to run. 
      DispatchDaemons();
      continue;
    }
    else if (rc != RPC2_SUCCESS) {
      LogMsg(0,LogLevel,LogFile, "main: ERROR ==> GetRequest (%s)", RPC2_ErrorMsg((int)rc));
      LogMsg(0,LogLevel,EventFile, "GetRequest: %s", RPC2_ErrorMsg((int)rc));
    }
    else {
	assert(LWP_SignalProcess(&workerSync) == LWP_SUCCESS);
    }
  }
}

void WorkerHandler() {
    int max, before, after;
    long rc;

    LogMsg(100,LogLevel,LogFile, "WorkerHandler: Initializing...");

    // Wait for and handle user events 
    while (1) {
        assert(LWP_WaitProcess(&workerSync) == LWP_SUCCESS);

	if (StackChecking) {
	    LWP_StackUsed((PROCESS)workerpid, &max, &before);
	    LogMsg(100,LogLevel,LogFile, "Worker:  Before call StackUsed returned max=%d used=%d\n",
		   max,before);
	    assert(used < 12*1024);
        }

	// Venus has asked us for advice -- do something about it!
	rc = AdMon_ExecuteRequest(cid, reqbuffer, NULL);
        if (rc != RPC2_SUCCESS) {
            LogMsg(0,LogLevel,LogFile, "WorkerHandler: ERROR ==> ExecuteRequest (%s)", RPC2_ErrorMsg((int)rc));
            LogMsg(0,LogLevel,EventFile, "ExecuteRequest: %s", RPC2_ErrorMsg((int)rc));
        }

	if (StackChecking) {
	    LWP_StackUsed((PROCESS)workerpid, &max, &after);
	    LogMsg(100,LogLevel,LogFile, "Worker:  After call StackUsed returned max=%d used=%d\n",
		   max,after);
	    if (after > before)
		LogMsg(0,LogLevel,LogFile, 
		   "Worker:  OP call (=%d -- see admon.h) increased stack usage\n",
		   reqbuffer->Header.Opcode);
	    assert(used < 12*1024);
        }
    }
}

/**********************************************
 *************  Utility Routines  *************
 **********************************************/

void InitPGID() {
#ifdef	__linux__
        (void) setpgrp();
#else
        (void) setpgrp(0, thisPID);
#endif
  thisPGID = thisPID;
}

void InitEnvironment() {
    int error = 0;

    error += setenv("TCL_LIBRARY", TCL, 1);
    error += setenv("TK_LIBRARY", TK, 1);
    assert(error == 0);
}

void InitHostName() {
  assert(gethostname(HostName, MAXHOSTNAMELEN) == 0);
  strcpy(ShortHostName, HostName);
  for (int i = 0; i < strlen(HostName); i++)
	  if (ShortHostName[i] == '.')
		  ShortHostName[i] = 0;
  snprintf(HostName, MAXHOSTNAMELEN, "localhost");
}

void InitUserData() {
  struct passwd *pwent;

  uid = getuid();

  /* Get the user's name */
  pwent = getpwuid(uid);
  assert(pwent != NULL);
  assert(strlen(pwent->pw_name) < 16);
  strcpy(UserName, pwent->pw_name);

  snprintf(BaseDir, MAXPATHLEN, "/coda/usr/%s/.questionnaires", UserName);
}

void InitCounters() {
    PCMcount.presentedCount = 0;
    PCMcount.requestedCount = 0;
    HWAcount.presentedCount = 0;
    HWAcount.requestedCount = 0;
    DMcount.presentedCount = 0;
    DMcount.requestedCount = 0;
    Rcount.presentedCount = 0;
    Rcount.requestedCount = 0;
    RPcount.presentedCount = 0;
    RPcount.requestedCount = 0;
    IASRcount.presentedCount = 0;
    IASRcount.requestedCount = 0;
    WCMcount.presentedCount = 0;
    WCMcount.requestedCount = 0;
    LCcount.presentedCount = 0;
    LCcount.requestedCount = 0;
    VSCcount.presentedCount = 0;
    VSCcount.requestedCount = 0;
}

#define REQUESTED 0
#define PRESENTED 1
void IncrementCounter(InternalCounter *counter, int presented) {
    assert(counter != NULL);
    counter->requestedCount++;
    if (presented)
	    counter->presentedCount++;
}

void InitDataDirectory() {
  struct timeval curTime;
  char timeString[timeLength];
  char dateNtime[15];
  int i;

  /* Generate dateNtime based on current date and time (mmmdd.hh:mm:ss) */
  (void) gettimeofday(&curTime, NULL);
  (void) strncpy(timeString, ctime((const long int *)&curTime.tv_sec), timeLength);
  for (i = 0; i < 3; i++) dateNtime[i] = timeString[i+4];
  for (i = 3; i < 14; i++) dateNtime[i] = timeString[i+5];
  dateNtime[14] = '\0';
  if (dateNtime[3] == ' ') dateNtime[3] = '0'; /* don't want blank */
  dateNtime[5] = '.';

  /* Create a unique Coda directory in which to dump our data */
  /* WorkingDir: /coda/usr/<username>/.questionnaires/<HOSTNAME>_<PID>_<DATE:TIME> */
  snprintf(WorkingDir, MAXPATHLEN, "%s/%s_%d_%s", BaseDir, ShortHostName, thisPID, dateNtime);
  InitializeDirectory(WorkingDir, UserName);

  // Create a temporary directory in which to deposit data before moving it into /coda
  snprintf(tmpDir, MAXPATHLEN, "/tmp/%s_%d_%s", ShortHostName, thisPID, dateNtime);
  InitializeDirectory(tmpDir, UserName);
}

void InitLogFile() {
    /* LogFileName: 
         /coda/usr/<username>/.questionnaires/<HOSTNAME>_<PID>_<DATE:TIME>/advice.log  */
    snprintf(LogFileName, MAXPATHLEN, "%s/advice.log", WorkingDir);

    LogFile = fopen(LogFileName, "a");
    if (LogFile == NULL) {
        fprintf(stderr, "LOGFILE (%s) initialization failed (errno=%d)\n\n", LogFileName, errno);
	fflush(stderr);
    }
    //  Use the following line to send the LogFile output to stdout...
    //    LogFile = stdout;

    struct timeval now;
    gettimeofday(&now, 0);
    char *s = ctime((const long int *)&now.tv_sec);
    LogMsg(0,LogLevel,LogFile,"LOGFILE initialized with LogLevel = %d at %s",
           LogLevel,ctime((const long int *)&now.tv_sec));
    LogMsg(0,LogLevel,LogFile,"My pid is %d",getpid());
}

void InitEventFile() {
    /* EventFileName:
       /coda/usr/<username>/.questionnaires/<HOSTNAME>_<PID>_<DATE:TIME>/advice.event  */
    snprintf(EventFileName, MAXPATHLEN, "%s/advice.event", WorkingDir);

    EventFile = fopen(EventFileName, "a");
    if (LogFile == NULL) 
        LogMsg(0,LogLevel,LogFile, "EVENTFILE (%s) initialization failed (errno=%d)", EventFileName, errno);
}

void ErrorReport(char *message)
{
  LogMsg(0,LogLevel,LogFile, "%s", message);
  exit(1);
}

void PrintUsage() {
  fprintf(stderr, "advice_srv [-d <LogLevel>] [i] [-v]\n");
  fflush(stderr);
  exit(-1);
}

/*PRIVATE*/ 
void CommandLineArgs(int argc, char **argv) {
  extern int optind;
  int arg_counter = 1;

  while ((arg_counter = getopt(argc, argv, "ad:im:svw:")) != EOF)
    switch(arg_counter) {
      case 'a':
	      // Undocumented feature 
	      // DEFAULT is ON (until PCM is functional)
              fprintf(stderr, "AutoReply turned on.\n");
              AutoReply = 1;
              break;
      case 'd':
              debug = 1;
	      LogLevel = atoi(argv[optind-1]);
              break;
      case 'i':
	      InitiateNewUser();
	      break;
      case 'm':
	      extern struct timeval cont_sw_threshold;
	      cont_sw_threshold.tv_sec = atoi(argv[optind-1]);
	      cont_sw_threshold.tv_usec = 0;
	      break;
      case 's':
	      StackChecking = 1;
	      break;
      case 'v':
              verbose = 1;
              break;
      case 'w':
	      extern struct timeval run_wait_threshold;
	      run_wait_threshold.tv_sec = (atoi(argv[optind-1]));
	      run_wait_threshold.tv_usec = 0;
	      break;
      default:
              PrintUsage();
    }
}

void InitiateNewUser()
{
  /* Give them some sort of tutorial?? */
  fprintf(stderr, "Tutorial for new users doesn't exist!  Bug Maria...\n");

  /* Create a unique Coda directory in which to dump our data */
  /* dflt: /coda/usr/<username>/.questionnaires */
  InitializeDirectory(BaseDir, UserName);

  if (strncmp(BaseDir, "/coda", 5) == 0)
      SetAccessLists(BaseDir, UserName);

  fprintf(stderr, "New user (%s) initiated.\n", UserName);
}


void CreateDataDirectory(char *dirname) {
  /* Create the directory */
  if (mkdir(dirname, 0755) != 0) {
      switch (errno) {
	  case EACCES:
	      fprintf(stderr, "advice_srv ERROR:  You do not have the necessary access rights to:\n");
	      fprintf(stderr, "\t\tmkdir %s'.\n\n", dirname);
	      fprintf(stderr, "\t==>  Please obtain tokens and retry.  <==\n");
	      fflush(stderr);
	      exit(-1);
	      break;
	  case ENOENT:
	      fprintf(stderr, "\n");
	      fprintf(stderr, "If this is the first time you have run the advice monitor,\n");
              fprintf(stderr, "then please do the following:\n");
              fprintf(stderr, "\t1) Make sure you are connected to the servers.\n");
              fprintf(stderr, "\t2) Rerun the advice monitor with the -i switch.\n");
              fprintf(stderr, "The -i switch gives you some instructions about how to use\n");
              fprintf(stderr, "the advice monitor and initializes a directory in which to\n");
              fprintf(stderr, "store the data collected by the advice monitor.\n\n");
              fprintf(stderr, "WARNING:  If you have previously run the advice monitor with\n");
              fprintf(stderr, "\tthe -i switch, do NOT follow these instructions.  If you \n");
              fprintf(stderr, "\tdo, you risk creating a conflict which will need to be\n");
              fprintf(stderr, "\trepaired manually.\n\n");
              fprintf(stderr, "Contact Maria Ebling (mre@cs) if you have further problems.\n\n");
              exit(-1);
	      break;
          case EEXIST:
	      fprintf(stderr, "\n");
	      fprintf(stderr, "The file or directory (%s) already exists.\n", dirname);
	      fprintf(stderr, "If you were running with the -i switch, you've probably \n");
	      fprintf(stderr, "already initializedthe necessary directory.  Try again \n");
	      fprintf(stderr, "without the -i switch.\n");
	      fprintf(stderr, "\n");
	      fprintf(stderr, "If you weren't running with the -i swtich, please contact\n");
	      fprintf(stderr, "Maria Ebling (mre@cs).\n");
	      exit(-1);
	      break;
      default:
	  fprintf(stderr, "advice_srv ERROR:  Unknown error (errno=%d) with 'mkdir %s'.\n", errno, dirname);
	  fflush(stdout);
	  exit(-1);
      }
  }
}


void SetAccessLists(char *dirname, char *username) {
  char commandName[MAXPATHLEN];

  /* Set the access lists correctly to give the user limited access */
  if (strcmp(username,"mre") != 0) {
      snprintf(commandName, MAXPATHLEN, "%s sa %s %s rliw", CFS, dirname, username);
      if (system(commandName) != 0) {
	  fprintf(stderr, "advice_srv WARNING:  Could not set access list appropriately.\n");
	  fprintf(stderr, "                     Command was %s\n\n", commandName);
	  fflush(stderr);
	  exit(-1);
      }
  }

  /* Set the access lists correctly to give me full access */
  bzero(commandName, MAXPATHLEN);
  snprintf(commandName, MAXPATHLEN, "%s sa %s mre all", CFS, dirname);
  if (system(commandName) != 0) {
    fprintf(stderr, "advice_srv WARNING:  Could not set access list appropriately.\n");
    fprintf(stderr, "                     Command was %s\n\n", commandName);
    fflush(stderr);
    exit(-1);
  }

}


void CreateREADMEFile(char *dirname) {
  char readmePathname[MAXPATHLEN];
  FILE *README;

  /* Generate a README file in the directory */
  if (strncmp(dirname, "/coda", 5) == 0)
      snprintf(readmePathname, MAXPATHLEN, "%s/README", dirname);
  else
      snprintf(readmePathname, MAXPATHLEN, "%s/#README", dirname);

  README = fopen(readmePathname, "w+");
  if (README == NULL) {
    fprintf(stderr, "advice_srv ERROR:  Cannot create README file: %s\n", readmePathname);
    fprintf(stderr, "                   Errno = %d\n", errno);
    fflush(stderr);
    exit(-1);
  }

  fprintf(README, "This directory contains data for the advice monitor.\n");
  fprintf(README, "Do NOT modify the contents of any file in this directory!\n");
  fprintf(README, "This directory and the data it contains will automatically\n");
  fprintf(README, "be processed by a nightly daemon and then removed.\n\n");
  fprintf(README, "If you have any questions, please contact Maria Ebling <mre@cs>.\n");
  fflush(README);
  fclose(README);
  chmod(readmePathname, 00444 /* world readable */);
}

void InitializeDirectory(char *dirname, char *username) {
  CreateDataDirectory(dirname);
  CreateREADMEFile(dirname);
}


#ifdef	__MACH__
// This is a Mach-specific way to do this.  For non-Mach architectures, read on...
#define ARGSIZE 4096
char* getcommandname(int pid) {
	char arguments[ARGSIZE];
	int *ip; 
	register char	*cp;
	char		c;
	char		*end_argc;
	int rc;

	if ((rc = table(TBL_ARGUMENTS, pid, arguments, 1, ARGSIZE)) != 1) {
	    LogMsg(0,LogLevel,LogFile,"getcommandname: ERROR ==> table() call failed");
	    LogMsg(0,LogLevel,EventFile,"table: %d\n", rc);
	    return((char *)0);
	}

	end_argc = &arguments[ARGSIZE];

	ip = (int *)end_argc;
	/* words must be word aligned! */
	if ((unsigned)ip & 0x3)
		ip = (int*)((unsigned)ip & ~0x3);
#ifdef	mips
	/* one exception frame worth of zeroes too */
	ip -= 10; /* EA_SIZE bytes */
#endif	mips
	ip -= 2;		/* last arg word and .long 0 */
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
	return(cp);
}

#else

/*
 * NON-Mach-specific way to get command names (sort of)
 *
 * This function assumes that the following command will identify the command
 * associated with pid 12845 and deposit that command in the file /tmp/command.
 * If this isn't true on the system you're working on, then the command names
 * which appear in various tcl scripts will be incorrect or non-existant.
 * ps ac 12845 | awk '{ if ($5 == "COMMAND") next; print $5 > "/tmp/command"}'
 */
char* getcommandname(int pid) {
    char tmpfile[MAXPATHLEN];
    char commandString[MAXPATHLEN];
    char *commandname;
    int rc;
    FILE *f;

    snprintf(tmpfile, MAXPATHLEN, "/tmp/advice_srv.%d", thisPID);
	
    snprintf(commandString, MAXPATHLEN, "ps axc %d | awk '{ if ($5 == \"COMMAND\") next; print $5 > \"%s\"}'", pid, tmpfile);

    rc = system(commandString);
    f = fopen(tmpfile, "r");
    fscanf(f, "%s", commandname);
    fclose(f);
    unlink(tmpfile);
    return(commandname);
}
#endif

char *GetCommandName(int pid) {
    char *commandname;
    static char CommandName[MAXPATHLEN];

    commandname = getcommandname(pid);
    assert(strlen(commandname) < MAXPATHLEN);
    if (commandname == NULL) 
	snprintf(CommandName, MAXPATHLEN, "Unknown");
    else if (strcmp(commandname, "") == 0) 
	snprintf(CommandName, MAXPATHLEN, "Unknown");
    else
	snprintf(CommandName, MAXPATHLEN, "%s", commandname);
    return(CommandName);
}

char *GetStringFromTimeDiff(long time_difference) {
    long seconds, minutes, hours, days;
    static char the_string[smallStringLength];

    if (time_difference < 60) {
        snprintf(the_string, smallStringLength, "%d second%s", time_difference, (time_difference>1)?"s":"");
        return(the_string);
    }

    minutes = time_difference / 60;  // Convert to minutes
    seconds = time_difference % 60;
    if (minutes < 60) {
	if (seconds > 0)
	    snprintf(the_string, smallStringLength, "%d minute%s %d second%s", minutes, (minutes>1)?"s,":",", seconds, (seconds>1)?"s":"");
        else
            snprintf(the_string, smallStringLength, "%d minute%s", minutes, (minutes>1)?"s":"");
        return(the_string);
    }
 
    hours = minutes / 60;  // Convert to hours
    minutes = minutes % 60;
    if (hours < 24) {
	if (minutes > 0)
	    snprintf(the_string, smallStringLength, "%d hour%s, %d minute%s", hours, (hours>1)?"s":"", minutes, (minutes>1)?"s":"");
        else
	    snprintf(the_string, smallStringLength, "%d hour%s", hours, (hours>1)?"s":"");
        return(the_string);
    }

    days = hours / 24;  // Convert to days
    hours = hours % 24;
    if (hours > 0)
	snprintf(the_string, smallStringLength, "%d day%s, %d hour%s", days, (days>1)?"s":"", hours, (hours>1)?"s":"");
    else
	snprintf(the_string, smallStringLength, "%d day%s", days, (days>1)?"s":"");
    return(the_string);
}

char *GetTimeFromLong(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  assert(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(the_string);
}

char *GetDateFromLong(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  assert(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d/%02d/%02d", lt->tm_mon+1, lt->tm_mday, lt->tm_year);
  return(the_string);
}

char *TimeString(long the_time) {
  static char the_string[smallStringLength];
  struct tm *lt = localtime((long *)&the_time);
  assert(lt != NULL);
  snprintf(the_string, smallStringLength, "%02d/%02d/%02d %02d:%02d:%02d", lt->tm_mon+1, lt->tm_mday, lt->tm_year, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(the_string);
}

void PrintCounters() {
    LogMsg(1000,LogLevel,LogFile, "PCM=(%d/%d); DM=(%d/%d); R=(%d/%d); IASR=(%d/%d); HWA=(%d/%d); RP=(%d/%d); WCM=(%d/%d); LC=(%d/%d); VSC=(%d/%d)\n", PCMcount.presentedCount, PCMcount.requestedCount, DMcount.presentedCount, DMcount.requestedCount, Rcount.presentedCount, Rcount.requestedCount, IASRcount.presentedCount, IASRcount.requestedCount, HWAcount.presentedCount, HWAcount.requestedCount, RPcount.presentedCount, RPcount.requestedCount, WCMcount.presentedCount, WCMcount.requestedCount, LCcount.presentedCount, LCcount.requestedCount, VSCcount.presentedCount, VSCcount.requestedCount);
}


/******************************************************
 ***********  RPC2 Initialization Routines  ***********
 ******************************************************/

/* Establish a connection to the server running on machine machine_name */
RPC2_Handle connect_to_machine(char *machine_name)
{
  RPC2_Handle cid;
  RPC2_HostIdent hid;
  RPC2_PortalIdent pid;
  RPC2_SubsysIdent sid;
  long rc;
  RPC2_BindParms bp;

  hid.Tag = RPC2_HOSTBYNAME;
  if (strlen(machine_name) >= 64) { /* Not MAXHOSTNAMELEN because rpc2.h uses "64"! */
      snprintf(error_msg, BUFSIZ, "Machine name %s too long!", machine_name);
      ErrorReport(error_msg);
    } ;
  strcpy(hid.Value.Name, machine_name);
  pid.Tag = RPC2_PORTALBYINETNUMBER;
  pid.Value.InetPortNumber = htons(ADSRVPORTAL);
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADSRVSUBSYSID; 

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = NULL;
  bp.SideEffectType = NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
  if (rc != RPC2_SUCCESS) {
      snprintf(error_msg, BUFSIZ, "%s\nCan't connect to machine %s", RPC2_ErrorMsg((int)rc),
              machine_name);
      ErrorReport(error_msg);
    };
  return(cid);
} ;

void Init_RPC()
{
  RPC2_PortalIdent *portallist[1], portal1;
  RPC2_SubsysIdent sid;
  long rc ;

  if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)&mainpid) != LWP_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "Can't Initialize LWP");   /* Initialize LWP package */
    ErrorReport(error_msg);
  }

  /* Initialize RPC2 Package */
  portallist[0] = &portal1; 

  portal1.Tag = RPC2_PORTALBYINETNUMBER;
  portal1.Value.InetPortNumber = NULL;

  rc = RPC2_Init(RPC2_VERSION, NULL, portallist, 1, -1, NULL) ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s:  Can't Initialize RPC2", RPC2_ErrorMsg((int)rc));
    ErrorReport(error_msg);
  }

  /* Export Venus subsystem */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADMONSUBSYSID;
  rc = RPC2_Export(&sid) != RPC2_SUCCESS ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s\nCan't export the advice subsystem", RPC2_ErrorMsg((int)rc));
    ErrorReport(error_msg);
  }
  //  VenusService_EnqueueRequest = 1; 
}


void InformVenusOfOurExistance(char *hostname) {
  long rc;
  RPC2_Integer Major;
  RPC2_Integer Minor;

  LogMsg(1000,LogLevel,LogFile,"InformVenusOfOurExistance: Binding to venus");
  VenusCID = connect_to_machine(hostname);
  LogMsg(1000,LogLevel,LogFile,
	"InformVenusOfOurExistance: NewAdviceService(%s, %d, %d, %d, %d, %d)...", 
	hostname, uid, rpc2_LocalPortal.Value.InetPortNumber, thisPGID, 
	ADSRV_VERSION, ADMON_VERSION);

  rc = NewAdviceService(VenusCID, (RPC2_String)hostname, (RPC2_Integer)uid, (RPC2_Integer)rpc2_LocalPortal.Value.InetPortNumber, (RPC2_Integer)thisPGID, (RPC2_Integer)ADSRV_VERSION, (RPC2_Integer)ADMON_VERSION, &Major, &Minor);

  VenusMajorVersionNumber = (int)Major;
  VenusMinorVersionNumber = (int)Minor;

  if (rc != RPC2_SUCCESS) {
      fprintf(stderr,"InformVenusOfOurExistance:  NewAdviceService call failed\n");
      fprintf(stderr,"\tCheck the venus.log file for the error condition.\n");
      fprintf(stderr,"\tIt is probably version skew between venus and advice_srv.\n");
      fflush(stderr);
      LogMsg(0,LogLevel,LogFile, "InformVenusOfOurExistance: ERROR ==> NewAdviceService call failed (%s)", RPC2_ErrorMsg((int)rc));
      LogMsg(0,LogLevel,LogFile, "\t Check the venus.log file for the error condition.");
      LogMsg(0,LogLevel,LogFile, "\t It is probably version skew between venus and advice_srv.");
      LogMsg(0,LogLevel,EventFile, "NewAdviceService: VersionSkew or UserNotExist");
      LogMsg(0,LogLevel,LogFile, "Information:  uid=%d, ADSRV_VERSION=%d, ADMON_VERSION=%d",
	     uid, ADSRV_VERSION, ADMON_VERSION);
      fflush(LogFile);
      fflush(EventFile);
      fclose(LogFile);
      fclose(EventFile);
      exit(-1);
  }
  else {
    LogMsg(0,LogLevel,LogFile,"Version information:");
    LogMsg(0,LogLevel,LogFile,"\tAdvice Monitor Version = %d",ADVICE_MONITOR_VERSION);
    LogMsg(0,LogLevel,LogFile,"\tVenus Version = %d.%d", VenusMajorVersionNumber, VenusMinorVersionNumber);
    LogMsg(0,LogLevel,LogFile,"\tADSRV Version = %d",ADSRV_VERSION);
    LogMsg(0,LogLevel,LogFile,"\tADMON Version = %d\n",ADMON_VERSION);
    fflush(LogFile);
  }
}

SetupDefaultAdviceRequests() {
    InterestValuePair interests[2];
    int numInterests = 2;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E SetupDefaultAdviceRequests");

    interests[0].interest = ReconnectionEvent;
    interests[1].interest = ReintegrationPending;
    for (int i=0; i<numInterests; i++) {
        interests[i].argument = 0;
        interests[i].value = 1;
    }
    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "SetupDefaultAdviceRequests call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L SetupDefaultAdviceRequests");
}

SolicitAdviceOnNextHoardWalk() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E SolicitAdviceOnNextHoardWalk");

    interests[0].interest = HoardWalk;
    interests[0].argument = 0;
    interests[0].value = 1;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "SolicitHoardWalkAdvice call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L SolicitAdviceOnNextHoardWalk");
}


UnsolicitAdviceOnNextHoardWalk() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E UnsolicitAdviceOnNextHoardWalk");

    interests[0].interest = HoardWalk;
    interests[0].argument = 0;
    interests[0].value = 0;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile, "UnsolicitHoardWalkAdvice call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L UnsolicitAdviceOnNextHoardWalk");
}

SolicitWeakMissAdvice() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E SolicitWeakMissAdvice");

    interests[0].interest = WeaklyConnectedCacheMiss;
    interests[0].argument = 0;
    interests[0].value = 1;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "SolicitWeakMissAdvice call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L SolicitWeakMissAdvice");
}

UnsolicitWeakMissAdvice() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E UnsolicitWeakMissAdvice");

    interests[0].interest = WeaklyConnectedCacheMiss;
    interests[0].argument = 0;
    interests[0].value = 0;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "UnsolicitWeakMissAdvice call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L UnsolicitWeakMissAdvice");
}

SolicitDiscoMissQs() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E SolicitDiscoMissQs");

    interests[0].interest = DisconnectedCacheMiss;
    interests[0].argument = 0;
    interests[0].value = 1;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "SolicitDiscoMissQs call failed (rc=%d)",rc);
    }
    LogMsg(100,LogLevel,LogFile, "L SolicitDiscoMissQs");
}

UnsolicitDiscoMissQs() {
    InterestValuePair interests[1];
    int numInterests = 1;
    long rc;

    LogMsg(100,LogLevel,LogFile, "E UnsolicitDiscoMissQs");

    interests[0].interest = DisconnectedCacheMiss;
    interests[0].argument = 0;
    interests[0].value = 0;

    rc = RegisterInterest(VenusCID, (RPC2_Integer)uid, numInterests, (InterestValuePair *)interests);
    if (rc != RPC2_SUCCESS) {
	    LogMsg(0,LogLevel,LogFile, "UnsolicitDiscoMissQs call failed\n");
    }
    LogMsg(100,LogLevel,LogFile, "L UnsolicitDiscoMissQs");
}

BeginStopLightMonitor() {
    long rc;

    LogMsg(100,LogLevel,LogFile, "E BeginStopLightMonitor()");
    (void) unlink(VDBFileName);

    /* Startup the monitor */
    {
      char *args[2];

      args[0] = STOPLIGHT;
      args[1] = NULL;

      int rc = fork_tcl(STOPLIGHT, args);
      if (rc == -1) {
        LogMsg(0,LogLevel,LogFile, "BeginStopLightMonitor: fork_tcl ERROR");
        LogMsg(0,LogLevel,EventFile, "BeginStopLightMonitor: fork_tcl ERROR");
        LogMsg(0,LogLevel,LogFile, "L BeginStopLightMonitor()");
	return(-1);
      } else {
	CurrentStoplightState = SLoff;	
      }
    } 

    /* Inform Venus of our Interest */
/*
    rc = BeginStoplightMonitor(VenusCID, (RPC2_Integer)uid);
    if (rc != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile, "BeginStoplightMonitor call failed\n");
    } else {
        StoplightInterest = TRUE;
    }
*/

    LogMsg(100,LogLevel,LogFile, "L BeginStopLightMonitor()");
}


EndStopLightMonitor() {
    long rc;

    LogMsg(100,LogLevel,LogFile, "E EndStopLightMonitor()");

    /* Inform Venus of our Disinterest */
/*
    rc = EndStoplightMonitor(VenusCID, (RPC2_Integer)uid);
    if (rc != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile, "EndStoplightMonitor call failed\n");
    } else {
        StoplightInterest = FALSE;
    }
*/

    /* Stop the monitor */
    CurrentStoplightState = StoplightStateChange(SLquit);

    (void) unlink(VDBFileName);
    LogMsg(100,LogLevel,LogFile, "L EndStopLightMonitor()");
}


StoplightStates StoplightStateChange(StoplightStates NewState)
{
  StoplightStates newState;
  char statestring[smallStringLength];

  LogMsg(100,LogLevel,LogFile, "E StoplightStateChange()");

  switch (NewState) {
      case SLdisconnect:
	  snprintf(statestring, smallStringLength, "disconnect");
	  break;
      case SLweak:
	  snprintf(statestring, smallStringLength, "weak");
	  break;
      case SLstrong:
	  snprintf(statestring, smallStringLength, "strong");
	  break;
      case SLoff:
	  snprintf(statestring, smallStringLength, "off");
	  break;
      case SLquit:
	  snprintf(statestring, smallStringLength, "quit");
	  break;
      case SLunknown:
	  snprintf(statestring, smallStringLength, "off");
	  break;
  }

  {
       char *args[3];
       args[0] = STOPLIGHT_STATECHANGE;
       args[1] = statestring;
       args[2] = NULL;

       int rc = fork_tcl(STOPLIGHT_STATECHANGE, args);
       if (rc == -1) {
           LogMsg(0,LogLevel,LogFile, "L StoplightStateChange(): fork_tcl ERROR");
           LogMsg(0,LogLevel,EventFile, "StoplightStateChange: fork_tcl ERROR");
	   return(SLunknown);
       }
  }

    LogMsg(100,LogLevel,LogFile, "L StoplightStateChange()");
    return(NewState);
}


/*****************************************************
 *******  Helper Routine for User Interaction  *******
 *****************************************************/

int GetAdvice(char *request) 
{
  long rc;
  int rdfds = 0;
  int NFDS = 32;
  struct timeval Expiry;
  char buf[128];
  int op;

  Expiry.tv_sec = 60;
  Expiry.tv_usec = 0;

  rdfds = (1 << 0);

  printf("Please input advice regarding %s:  ", request); 
  fflush(stdout);
  rc = IOMGR_Select(NFDS, &rdfds, 0, 0, 0);
  switch (rc) {
    case 0:
	  LogMsg(100,LogLevel,LogFile,"GetAdvice: Timeout");
          break;
    case -1:
	  snprintf(error_msg, BUFSIZ, "IOMGR_Select errored.\n%d\n", RPC2_ErrorMsg((int)rc));
	  ErrorReport(error_msg);
    case 1:
	  rc = read(0, buf, 128);
	  LogMsg(100,LogLevel,LogFile, "GetAdvice: buf = *%s*", buf);
	  if (strncmp(buf, "fetch", 3) == 0)
	    op = ReadDiscFetch;
	  else if (strncmp(buf, "timeout", 7) == 0)
	    op = ReadDiscTimeout;
	  else if (strncmp(buf, "hoard", 5) == 0)
	    op = ReadDiscHOARDimmedFETCH;
	  else
	    op = ReadDiscUnknown;
	  return(op);
    default:
	  snprintf(error_msg, BUFSIZ, "IOMGR_Select returned too many fds, %d\n",rc);
	  ErrorReport(error_msg);
	  return(ReadDiscUnknown);
  }

  return(ReadDiscUnknown);
}


/***********************************************************************
 ***************** Routines to help Spawn Subprocesses *****************
 ***********************************************************************/

int execute_tcl(char *script, char *args[]) {
  LogMsg(100,LogLevel,LogFile, "E execute_tcl(%s)", script);

  int rc = fork();
  if (rc == -1) {
    LogMsg(0,LogLevel,LogFile, "execute_tcl: ERROR ==> during fork (rc = %d)", rc);
    LogMsg(0,LogLevel,EventFile, "execute_tcl: ERROR ==> during fork (rc = %d)", rc);
    return(-1);
  }
  else if (rc) {                          
    // parent process 
      CHILDpid = rc;
      LogMsg(1000,LogLevel,LogFile, "execute_tcl (Parent): CHILDpid = %d", CHILDpid);
      assert(LWP_WaitProcess((char *)&CHILDpid) == LWP_SUCCESS);
    
  }
  else {                                  
    // child process 
      if (execve(script, (const char **) args, (const char **) environ)) {
	LogMsg(0,LogLevel,LogFile, "execute_tcl (Child): ERROR ==> during execl");
	LogMsg(0,LogLevel,EventFile, "execute_tcl (Child): ERROR ==> during execl");
	return(-1);
      }
  }

  LogMsg(100,LogLevel,LogFile,"L execute_tcl()");
  return(0);
}

int fork_tcl(char *script, char *args[]) {
  LogMsg(100,LogLevel,LogFile, "E fork_tcl(%s)", script);

  int rc = fork();
  if (rc == -1) {
    LogMsg(0,LogLevel,LogFile, "fork_tcl: ERROR ==> during fork (rc = %d)", rc);
    LogMsg(0,LogLevel,EventFile, "fork_tcl: ERROR ==> during fork (rc = %d)", rc);
    return(-1);
  }
  else if (rc) {                          
    // parent process 
//      CHILDpid = rc;
      LogMsg(1000,LogLevel,LogFile, "fork_tcl (Parent): CHILDpid = %d", CHILDpid);
//      LWP_WaitProcess((char *)&CHILDpid);
  }
  else {                                  
    // child process 
      if (execve(script, (const char **) args, (const char **) environ)) {
	LogMsg(0,LogLevel,LogFile, "fork_tcl (Child): ERROR ==> during execl");
	LogMsg(0,LogLevel,EventFile, "fork_tcl (Child): ERROR ==> during execl");
	return(-1);
      }
  }

  LogMsg(100,LogLevel,LogFile,"L fork_tcl()");
  return(0);
}

/*************************************************************
 ******************  Incoming RPC Handlers  ******************
 *************************************************************/

long ReadDisconnectedMiss(RPC2_Handle _cid, RPC2_String pathname, RPC2_Integer pid, RPC2_Integer *advice) 
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    static int lastAdvice;
    miss *m;

    if (StackChecking) {
	LWP_StackUsed((PROCESS)workerpid, &max, &used);
	LogMsg(100,LogLevel,LogFile, "WorkerHandler:  ReadDisconnectedMiss returned max=%d used=%d\n",max,used);
	oldworkerused = used;
    }

    *advice = ReadDiscUnknown;

    /* Filter out duplicates -- if it's the same as the last one... */
    strncpy(thisPathname, (char*)pathname, MAXPATHLEN);
    strncpy(thisProgName, GetCommandName((int)pid), MAXPATHLEN);
    if ((strcmp(thisPathname,lastPathname) == 0) && 
	(strcmp(thisProgName,lastProgName) == 0)) {
	*advice = lastAdvice;
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, thisPathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    LogMsg(100,LogLevel,LogFile,"E ReadDisconnectedMiss: %s %d", (char*)pathname, (int)pid);

    IncrementCounter(&PCMcount, REQUESTED);

    if (AutoReply) {
       *advice = ReadDiscFetch;
       lastAdvice = *advice;
       return(RPC2_SUCCESS);
    }

    {
	char *args[4];

	args[0] = READMISS;
	args[1] = (char*)pathname;
	args[2] = GetCommandName((int)pid);
	args[3] = NULL;

	int rc = execute_tcl(READMISS, args);
	if (rc == -1) {
	    LogMsg(0,LogLevel,LogFile, "ReadDisconnectedMiss: execute_tcl ERROR");
	    LogMsg(0,LogLevel,EventFile, "ReadDisconnectedMiss: execute_tcl ERROR");
	    lastAdvice = *advice;
	    return(RPC2_SUCCESS);
	}
    }

    switch (CHILDresult) {
        case 2:
	    LogMsg(100,LogLevel,LogFile, "ReadDisconnectedMiss Advice: Hoarding...\n");
        case 0:
	    LogMsg(100,LogLevel,LogFile, "ReadDisconnectedMiss Advice: Fetch\n");
  	   *advice = ReadDiscFetch;
            break;
        case 1:
	    LogMsg(100,LogLevel,LogFile, "ReadDisconnectedMiss Advice: Timeout\n");
	    m = new miss((char*)pathname,GetCommandName((int)pid));
	   *advice = ReadDiscTimeout;
            break;
        default:
	    LogMsg(100,LogLevel,LogFile, "ReadDisconnectedMiss: ERROR ==> Invalid return code from tcl script (%d)\n",CHILDresult);
	    LogMsg(100,LogLevel,EventFile, "tcl_childreturn: PCM  %d", CHILDresult);
  	   *advice = ReadDiscUnknown;
	    break;
    }

    CHILDpid = 0;
    PrintCounters();
    LogMsg(100,LogLevel,LogFile, "L ReadDisconnectedMiss(advice=%d)", *advice);
    lastAdvice = *advice;
    if (StackChecking) {
	LWP_StackUsed((PROCESS)workerpid, &max, &used);
	LogMsg(100,LogLevel,LogFile, "WorkerHandler:  ReadDisconnectedMiss returned max=%d used=%d\n",max,used);
	oldworkerused = used;
    }
    return(RPC2_SUCCESS);
}


void InitDiscoFile(char *FileName, int venusmajor, int venusminor, int advice, int adsrv, int admon, DisconnectedMissQuestionnaire *questionnaire)
{
    FILE *DiscoFile;
    struct stat buf;

    // Ensure it does not exist
    stat(FileName, &buf);
    assert(errno == ENOENT);

    // Create and initialize it
    DiscoFile = fopen(FileName, "w+");
    assert(DiscoFile != NULL);

    assert(questionnaire != NULL);
    fprintf(DiscoFile, "Disconnected Cache Miss Questionnaire\n");
    fprintf(DiscoFile, "hostid: 0x%o\n", gethostid());
    fprintf(DiscoFile, "user: %d\n", uid);
    fprintf(DiscoFile, "VenusVersion: %d.%d\n", venusmajor, venusminor);
    fprintf(DiscoFile, "AdviceMonitorVersion: %d\n", advice);
    fprintf(DiscoFile, "ADSRVversion: %d\n", adsrv);
    fprintf(DiscoFile, "ADMONversion: %d\n", admon);
    fprintf(DiscoFile, "Qversion: %d\n", questionnaire->DMQVersionNumber);
    fprintf(DiscoFile, "TimeOfDisconnection: %d\n", questionnaire->TimeOfDisconnection);
    fprintf(DiscoFile, "TimeOfCacheMiss: %d\n", questionnaire->TimeOfCacheMiss);
    fprintf(DiscoFile, "Fid: <%x.%x.%x>\n", questionnaire->Fid.Volume, questionnaire->Fid.Vnode, questionnaire->Fid.Unique);
    fprintf(DiscoFile, "Path: %s\n", questionnaire->Pathname);
    fprintf(DiscoFile, "RequestingProgram: %s\n", GetCommandName((int)questionnaire->pid));
    fflush(DiscoFile);
    fclose(DiscoFile);
}

long DisconnectedMiss(RPC2_Handle _cid, DisconnectedMissQuestionnaire *questionnaire, RPC2_Integer *Qrc)
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    char tmpFileName[MAXPATHLEN];
    char FileName[MAXPATHLEN];
    miss *m;

    /* Filter out duplicates -- if it's the same as the last one... */
    assert(questionnaire != NULL);
    strncpy(thisPathname, (char*)questionnaire->Pathname, MAXPATHLEN);
    strncpy(thisProgName, GetCommandName((int)questionnaire->pid), MAXPATHLEN);
    if ((strcmp(thisPathname,lastPathname) == 0) && 
	(strcmp(thisProgName,lastProgName) == 0)) {
	*Qrc = ADMON_DUPLICATE;
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, thisPathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    LogMsg(100,LogLevel,LogFile,"E DisconnectedMiss: %s", TimeString((int)questionnaire->TimeOfDisconnection));

    m = new miss(thisPathname, thisProgName);

    IncrementCounter(&DMcount, PRESENTED);

    // Determine the filename
    snprintf(tmpFileName, MAXPATHLEN, "%s/#DisconnectedMiss.%d", tmpDir, DMcount.requestedCount);
    snprintf(FileName, MAXPATHLEN, "%s/DisconnectedMiss.%d", tmpDir, DMcount.requestedCount);

    InitDiscoFile(tmpFileName, VenusMajorVersionNumber, VenusMinorVersionNumber, ADVICE_MONITOR_VERSION, ADSRV_VERSION, ADMON_VERSION, questionnaire);

    fflush(stdout);
    *Qrc = ADMON_SUCCESS;

    {
       char *args[5];

       args[0] = DISCOMISS_SURVEY;
       args[1] = tmpFileName;
       args[2] = (char *)questionnaire->Pathname;
       args[3] = GetCommandName((int)questionnaire->pid);
       args[4] = NULL;

       int rc = execute_tcl(DISCOMISS_SURVEY, args);
       if (rc == -1) {
         LogMsg(0,LogLevel,LogFile, "DisconnectedMiss: execute_tcl ERROR");
         LogMsg(0,LogLevel,EventFile, "DisconnectedMiss: execute_tcl ERROR");
         *Qrc = ADMON_FAIL;
         return(RPC2_SUCCESS);
       }
    }

    if (CHILDresult != 0) {
	LogMsg(0,LogLevel,LogFile, "DisconnectedMiss: ERROR ==> tcl script return %d", CHILDresult);
        LogMsg(0,LogLevel,EventFile, "tcl_childreturn: DCM %d", CHILDresult);
    }
    else 
        // Move tmpFileName over to FileName (still in /tmp)
        rename(tmpFileName, FileName);    

    CHILDpid = 0;

    LogMsg(1000,LogLevel,LogFile, "L DisconnectedMiss()");
    PrintCounters();
    *Qrc = CHILDresult;
    return(RPC2_SUCCESS);
}

long WeaklyConnectedMiss(RPC2_Handle _cid, WeaklyConnectedInformation *information, RPC2_Integer *advice)
{
    static char lastPathname[MAXPATHLEN]; char thisPathname[MAXPATHLEN];
    static char lastProgName[MAXPATHLEN]; char thisProgName[MAXPATHLEN];
    static int lastAdvice;
    miss *m;

    *advice = WeaklyUnknown;

    /* Filter out duplicates -- if it's the same as the last one... */
    assert(information != NULL);
    strncpy(thisPathname, (char*)information->Pathname, MAXPATHLEN);
    strncpy(thisProgName, GetCommandName((int)information->pid), MAXPATHLEN);
    if ((strcmp(thisPathname,lastPathname) == 0) && 
	(strcmp(thisProgName,lastProgName) == 0)) {
	*advice = lastAdvice;
	return(RPC2_SUCCESS);
    }
    strncpy(lastPathname, thisPathname, MAXPATHLEN);
    strncpy(lastProgName, thisProgName, MAXPATHLEN);

    LogMsg(100,LogLevel,LogFile,"E WeaklyConnectedMiss: %s %d", (char*)information->Pathname, (int)information->pid);

    m = new miss((char*)information->Pathname,GetCommandName((int)information->pid));

    IncrementCounter(&WCMcount, PRESENTED);

    {
	char arg[smallStringLength];
	char *args[5];

	args[0] = WEAKMISS;
	args[1] = (char*)information->Pathname;
	args[2] = GetCommandName((int)information->pid); 
	snprintf(arg, smallStringLength, "%d", information->ExpectedFetchTime);
	args[3] = arg;
	args[4] = NULL;

	int rc = execute_tcl(WEAKMISS, args);
	if (rc == -1) {
	    LogMsg(0,LogLevel,LogFile, "WeaklyConnectedMiss: execute_tcl ERROR");
	    LogMsg(0,LogLevel,EventFile, "WeaklyConnectedMiss: execute_tcl ERROR");
	    *advice = WeaklyUnknown;
	    return(RPC2_SUCCESS);
	}
    }

    /* Check that the result is valid; if invalid, return WeaklyUnknown in advice. */
    if ((CHILDresult != -1) && (CHILDresult != 0) && (CHILDresult != 1)) {
	LogMsg(0,LogLevel,LogFile, "WeaklyConnectedMiss: ERROR ==> Invalid return code from tcl script (%d)", CHILDresult);
        LogMsg(0,LogLevel,EventFile, "tcl_childreturn: WCM  %d", CHILDresult);
	*advice = WeaklyUnknown;
    } else {
        // CHILDresult contains the user's answer
        *advice = CHILDresult;
    }

    CHILDpid = 0;

    PrintCounters();

    LogMsg(100,LogLevel,LogFile, "L WeaklyConnectedMiss(advice = %d)", *advice);
    lastAdvice = *advice;
    return(RPC2_SUCCESS);
}

long DataFetchEvent(RPC2_Handle _cide, RPC2_String Pathname, RPC2_Integer Size, RPC2_String Vfile)
{
    printf("DataFetchEvent:  Path=%s; Size=%d; Vfile=%s\n", Pathname, Size, Vfile);
    return(RPC2_SUCCESS);
}

long HoardWalkAdvice(RPC2_Handle _cid, RPC2_String InputPathname, RPC2_String OutputPathname, RPC2_Integer *ReturnCode)
{
    LogMsg(100,LogLevel,LogFile,"E HoardWalkAdvice(%s,%s)", (char *)InputPathname, (char *)OutputPathname);

    IncrementCounter(&HWAcount, PRESENTED);

    *ReturnCode = ADMON_SUCCESS;

    {
       char *args[4];

       args[0] = HOARDLIST;
       args[1] = (char *)InputPathname;
       args[2] = (char *)OutputPathname;
       args[3] = NULL;

       int rc = execute_tcl(HOARDLIST, args);
       if (rc == -1) {
         LogMsg(0,LogLevel,LogFile, "HoardWalkAdvice: execute_tcl ERROR");
         LogMsg(0,LogLevel,EventFile, "HoardWalkAdvice: execute_tcl ERROR");
         *ReturnCode = ADMON_FAIL;
         return(RPC2_SUCCESS);
       }
    }

    if (CHILDresult != 0) {
	LogMsg(0,LogLevel,LogFile, "HoardWalkAdvice: ERROR ==> tcl script return %d", CHILDresult);
        LogMsg(0,LogLevel,EventFile, "tcl_childreturn: HWA %d", CHILDresult);
    }

    CHILDpid = 0;

    LogMsg(100,LogLevel,LogFile, "L HoardWalkAdvice()");
    PrintCounters();
    *ReturnCode = CHILDresult;

    return(RPC2_SUCCESS);
}

void InitReconFile(char *FileName, int venusmajor, int venusminor, int advice, int adsrv, int admon, ReconnectionQuestionnaire *questionnaire)
{
    FILE *ReconFile;
    struct stat buf;

    // Ensure it does not exist
    stat(FileName, &buf);
    assert(errno == ENOENT);

    // Create and initialize it
    ReconFile = fopen(FileName, "w+");
    assert(ReconFile != NULL);

    assert(questionnaire != NULL);
    fprintf(ReconFile, "Reconnection Questionnaire\n");
    fprintf(ReconFile, "hostid: %d\n", gethostid());
    fprintf(ReconFile, "user: %d\n", uid);
    fprintf(ReconFile, "VenusVersion: %d.%d\n", venusmajor, venusminor);
    fprintf(ReconFile, "AdviceMonitorVersion: %d\n", advice);
    fprintf(ReconFile, "ADSRVversion: %d\n", adsrv);
    fprintf(ReconFile, "ADMONversion: %d\n", admon);
    fprintf(ReconFile, "Qversion: %d\n", questionnaire->RQVersionNumber);
    fprintf(ReconFile, "VolumeName: %s\n", questionnaire->VolumeName);
    fprintf(ReconFile, "VID: 0x%x\n", questionnaire->VID);
    fprintf(ReconFile, "CMLCount: %d\n", questionnaire->CMLcount);
    fprintf(ReconFile, "TimeOfDisconnection: %d\n", questionnaire->TimeOfDisconnection);
    fprintf(ReconFile, "TimeOfReconnection: %d\n", questionnaire->TimeOfReconnection);
    fprintf(ReconFile, "TimeOfLastDemandHoardWalk: %d\n", questionnaire->TimeOfLastDemandHoardWalk);
    fprintf(ReconFile, "NumberOfReboots: %d\n", questionnaire->NumberOfReboots);
    fprintf(ReconFile, "NumberOfCacheHits: %d\n", questionnaire->NumberOfCacheHits);
    fprintf(ReconFile, "NumberOfCacheMisses: %d\n", questionnaire->NumberOfCacheMisses);
    fprintf(ReconFile, "NumberOfUniqueCacheHits: %d\n", questionnaire->NumberOfUniqueCacheHits);
//    fprintf(ReconFile, "NumberOfUniqueCacheMisses: %d\n", questionnaire->NumberOfUniqueCacheMisses);
    fprintf(ReconFile, "NumberOfObjectsNotReferenced: %d\n", questionnaire->NumberOfObjectsNotReferenced);

    fflush(ReconFile);
    fclose(ReconFile);
}

int PresentRQ(char *volumeName) {
    char userVolume[smallStringLength];

    // Guess at the name of the user volume --> WEAK!
    snprintf(userVolume, smallStringLength, "u.%s", UserName);

    if (strcmp(volumeName, userVolume) == 0)
	return(1);
    else
	return(0);
}

long Reconnection(RPC2_Handle _cid, ReconnectionQuestionnaire *questionnaire, RPC2_Integer *Qrc)
{
    char tmpFileName[MAXPATHLEN];
    char FileName[MAXPATHLEN];

    assert(questionnaire != NULL);
    LogMsg(100,LogLevel,LogFile,"E Reconnection: %s", questionnaire->VolumeName);
    // Log params to this reconnection event
    LogMsg(1000,LogLevel,LogFile,"VenusVersionNumber = %d.%d", VenusMajorVersionNumber, VenusMinorVersionNumber);
    LogMsg(1000,LogLevel,LogFile,"RQVersionNumber = %d", (int)questionnaire->RQVersionNumber);

    LogMsg(1000,LogLevel,LogFile,"Volume Name = %s <%x>", questionnaire->VolumeName, questionnaire->VID);
    LogMsg(1000,LogLevel,LogFile,"CMLcount = %d", questionnaire->CMLcount);
    LogMsg(1000,LogLevel,LogFile,"Time of Disconnection = %s",TimeString((long)questionnaire->TimeOfDisconnection)); 
    LogMsg(1000,LogLevel,LogFile,"Time of Reconnection = %s",TimeString((long)questionnaire->TimeOfReconnection)); 
    LogMsg(1000,LogLevel,LogFile,"Time of last demand hoard walk = ");
    LogMsg(1000,LogLevel,LogFile,"Number of Reboots = %d", questionnaire->NumberOfReboots);
    LogMsg(1000,LogLevel,LogFile,"Number of Cache HITS = %d", questionnaire->NumberOfCacheHits);
    LogMsg(1000,LogLevel,LogFile,"Number of Cache MISSES = %d", questionnaire->NumberOfCacheMisses);
    LogMsg(1000,LogLevel,LogFile,"Number of Unique Cache HITS = %d", questionnaire->NumberOfUniqueCacheHits);
    LogMsg(1000,LogLevel,LogFile,"Number of Objects NOT Referenced = %d", questionnaire->NumberOfObjectsNotReferenced);

    *Qrc = ADMON_SUCCESS;

    // Decide whether or not to present the reconnection questionnaire
    if (!PresentRQ((char *)questionnaire->VolumeName)) {
    	LogMsg(100,LogLevel,LogFile, "L Reconnection()");
	IncrementCounter(&Rcount, REQUESTED);
	PrintCounters();
	return(RPC2_SUCCESS);
    }

    IncrementCounter(&Rcount, PRESENTED);

    // Determine the filename
    snprintf(tmpReconnectionFileName, MAXPATHLEN, "%s/#Reconnection.%d", tmpDir, Rcount.requestedCount);
    snprintf(ReconnectionFileName, MAXPATHLEN, "%s/Reconnection.%d", tmpDir, Rcount.requestedCount);

    InitReconFile(tmpReconnectionFileName, VenusMajorVersionNumber, VenusMinorVersionNumber, ADVICE_MONITOR_VERSION, ADSRV_VERSION, ADMON_VERSION, questionnaire);

    int rc = fork();
    if (rc == -1) {
      LogMsg(0,LogLevel,LogFile, "Reconnection: ERROR ==> during fork");
      LogMsg(0,LogLevel,EventFile, "tcl_fork: reconn");
      *Qrc = ADMON_FAIL;
      return(RPC2_SUCCESS);
    }
    else if (!rc) {      
      // child process

      char *args[6];
      args[0] = RECONNECTION_SURVEY;
      args[1] = tmpReconnectionFileName;
      args[2] = GetDateFromLong((long)questionnaire->TimeOfDisconnection);
      args[3] = GetTimeFromLong((long)questionnaire->TimeOfDisconnection);
      args[4] = GetStringFromTimeDiff((long)questionnaire->TimeOfReconnection-(long)questionnaire->TimeOfDisconnection);
      args[5] = NULL;

      if (execve(RECONNECTION_SURVEY, (const char **) args, (const char **) environ)) {
	LogMsg(0,LogLevel,LogFile, "Reconnection (Child): ERROR ==> during execl");
        LogMsg(0,LogLevel,EventFile, "tcl_execl: reconn");
        *Qrc = ADMON_FAIL;
        return(RPC2_SUCCESS);
      }
    }
    else {
        // parent process
        CHILDpid = rc;
	LogMsg(1000,LogLevel,LogFile, "Reconnection (Parent): CHILDpid = %d", CHILDpid);
	ReconnectionQuestionnaireInProgress = 1;

	// Can't wait for the questionnaire due to some sort of race condition in Venus!
	// LWP_WaitProcess((char *)&CHILDpid);
        // Sleep for a couple of seconds so that if the exec fails it finishes first
        sleep(2);
    }

    LogMsg(100,LogLevel,LogFile, "L Reconnection()");
    PrintCounters();
    *Qrc = ADMON_SUCCESS;
    return(RPC2_SUCCESS);
}


long VSEmulating(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    LogMsg(100,LogLevel,LogFile, "E VSEmulating(%s,%x)", (char *)VolumeName, vid);

    v = new volent((char *)VolumeName, vid, VSemulating);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(100,LogLevel,LogFile, "L VSEmulating()");
    return(RPC2_SUCCESS);
}

long VSHoarding(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    LogMsg(100,LogLevel,LogFile, "E VSHoarding(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VShoarding);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(100,LogLevel,LogFile, "L VSHoarding()");
    return(RPC2_SUCCESS);
}

long VSLogging(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    LogMsg(100,LogLevel,LogFile, "E VSLogging(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VSlogging);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(100,LogLevel,LogFile, "L VSLogging()");
    return(RPC2_SUCCESS);
}

long VSResolving(RPC2_Handle _cid, RPC2_String VolumeName, VolumeId vid)
{
    StoplightStates newState;
    volent *v;

    LogMsg(100,LogLevel,LogFile, "E VSResolving(%s,%x)", (char *)VolumeName, vid);
    v = new volent((char *)VolumeName, vid, VSresolving);
    newState = StoplightState();
    if ((newState != CurrentStoplightState) && (CurrentStoplightState != SLquit))
        CurrentStoplightState = StoplightStateChange(newState);
    LogMsg(100,LogLevel,LogFile, "L VSResolving()");
    return(RPC2_SUCCESS);
}




long ReintegratePending(RPC2_Handle _cid, RPC2_String VolumeName, Boolean PendingFlag)
{
  LogMsg(100,LogLevel,LogFile,"E ReintegratePending(%s,%s)", (char *)VolumeName, (PendingFlag?"pending":"not pending"));

  if (PendingFlag) {
    LogMsg(100,LogLevel,LogFile, "Alert:  Reintegration pending for Volume %s\n",
	   VolumeName);
    IncrementCounter(&RPcount, PRESENTED);
    printf("Alert:  ReintegratePending for Volume %s\n", VolumeName);
    fflush(stdout);
    {
       char *args[3];

       args[0] = REINT_PENDING;
       args[1] = (char *)VolumeName;
       args[2] = NULL;

       int rc = fork_tcl(REINT_PENDING, args);
       if (rc == -1) {
         LogMsg(0,LogLevel,LogFile, "ReintegratePending: fork_tcl ERROR");
         LogMsg(0,LogLevel,EventFile, "ReintegratePending: fork_tcl ERROR");
       }
    }
  } else {
    LogMsg(100,LogLevel,LogFile, "Cancel: Reintegration pending for Volume %s\n",
	   VolumeName);
    IncrementCounter(&RPcount, REQUESTED);
    printf("Cancel:  ReintegratePending for Volume %s\n", VolumeName);
    fflush(stdout);
  }


  LogMsg(100,LogLevel,LogFile, "L ReintegratePending()");
  PrintCounters();
  return(RPC2_SUCCESS);
}

long InvokeASR(RPC2_Handle _cid, RPC2_String pathname, RPC2_Integer uid, RPC2_Integer *ASRid, RPC2_Integer *ASRrc)
{
    IncrementCounter(&IASRcount, PRESENTED);

    *ASRrc = ADMON_SUCCESS;

    NumASRStarted ++;
    *ASRid = NumASRStarted;

    struct stat statbuf;
    if (!::stat(JUMPSTARTASR, &statbuf)) {
    int rc = fork();
    if (rc == -1) {
      LogMsg(0,LogLevel,LogFile,"InvokeASR: ERROR ==> during fork");
      LogMsg(0,LogLevel,EventFile, "tcl_fork: asr");
      *ASRrc = ADMON_FAIL;
      return(RPC2_SUCCESS);
    }
    else if (!rc) {
      // child process
      setuid((unsigned short)uid);
      char dnamebuf[MAXPATHLEN];
      char fnamebuf[MAXNAMLEN];
      path((char *)pathname, dnamebuf, fnamebuf);
      if (chdir(dnamebuf)) {
	LogMsg(0,LogLevel,LogFile,"InvokeASR: ERROR ==> ASR jump-starter couldn't change to directory %s", dnamebuf);
        LogMsg(0,LogLevel,EventFile, "jump-starter: cd");
	*ASRrc = ADMON_FAIL;
	return(ENOENT);
      }
      execl(JUMPSTARTASR, JUMPSTARTASR, pathname, 0);
    }
    else 
	// parent process
        ASRinProgress = 1;
	CHILDpid = rc;
  }
  else {
    LogMsg(0,LogLevel,LogFile, "InvokeASR: ERROR ==> JUMPSTARTASR (%s) not found", JUMPSTARTASR);
    LogMsg(0,LogLevel,EventFile, "jump-starter: not found");
    *ASRrc = ADMON_FAIL;
    return(RPC2_SUCCESS);
  }

  PrintCounters();
  return(RPC2_SUCCESS);
}

long TestConnection(RPC2_Handle _cid)
{
  LogMsg(100,LogLevel,LogFile, "TestConnection from venus");
  return(RPC2_SUCCESS);
}

long LostConnection(RPC2_Handle _cid)
{
  LogMsg(100,LogLevel,LogFile,"LostConnection to \"venus\"");
  IncrementCounter(&LCcount, REQUESTED);
  WeLostTheConnection = TRUE;
  if (AwaitingUserResponse == TRUE)
    (void) RPC2_Unbind(rpc2_LocalPortal.Value.InetPortNumber);
  return(RPC2_SUCCESS);
}


/****************************************************
 *******************  Data Mover  *******************
 ****************************************************/

/*
 *  We cannot write the reconnection questionnaire results directly to Coda files
 *  or else Venus blocks on the advice monitor and the advice monitor blocks on
 *  Venus.  The result is that the connection to the advice monitor eventually
 *  times out, thus preventing the deadlock...  Venus blocks on the advice monitor 
 *  to return from the reconnection questionnaire on Volume #1.  In presenting the
 *  reconnection questionnaire for Volume #1 to the user, the advice monitor must
 *  open a file in /coda in Volume #2.  Venus will not allow this open to go through 
 *  because the volume has a transition pending.  
 *
 *  A nominal solution to this problem is to write the files to /tmp first and then
 *  move them over into /coda at another time.  I improve upon this solution by writing
 *  the data to a file named #Name.n first (the data gets written in two stages -- initial
 *  information is written by the advice monitor and the data provided by the user is
 *  written by the tcl script.  Once the tickle script has returned successfully, I
 *  move the file into Name.n.  When the RPC2 GetRequest routine times out, I trigger 
 *  the Data Mover routine.  This routine moves completed data files from /tmp into /coda.
 */

int MoveFile(char *filename, char *hereDir, char* thereDir) {
    int hereFile, thereFile;
    char hereFileName[MAXPATHLEN];
    char thereFileName[MAXPATHLEN];
    int code = 0;

    LogMsg(100,LogLevel,LogFile, "E MoveFile(%s, %s, %s)\n", filename, hereDir, thereDir);

    // Set up the file names
    snprintf(hereFileName, MAXPATHLEN, "%s/%s", hereDir, filename);
    snprintf(thereFileName, MAXPATHLEN, "%s/%s", thereDir, filename);

    LogMsg(1000,LogLevel,LogFile, "MoveFile: %s",hereFileName);
    LogMsg(1000,LogLevel,LogFile, "MoveFile: %s",thereFileName);

    // Open the file we will copy FROM
    hereFile = open(hereFileName, O_RDONLY, 0644);
    if (hereFile < 0) {
	if (ReadError == 0) {
	    LogMsg(0,LogLevel,LogFile, "ERROR: MoveFile(%s,%s,%s) ==> Cannot open file %s", filename, hereDir, thereDir, hereFileName);
            LogMsg(0,LogLevel,LogFile, "       This error will not be reported again until the situation is resolved.  At that time, we will report the total number of failures.");
            LogMsg(0,LogLevel,EventFile, "movefile: open %s",hereFileName);
        }
	ReadError++;
	return(1);
    } else if ( /* OPEN SUCCEEDED and */ ReadError > 0) {
       LogMsg(0,LogLevel,LogFile, "ERROR RESOLVED: MoveFile(%s,%s,%s) ==> Successfully opened file %s after %d open failures.", filename, hereDir, thereDir, hereFileName, ReadError);
       LogMsg(0,LogLevel,EventFile, "movefile: open %s ==> problem resolved after %d failures",hereFileName, ReadError);
       ReadError = 0;
    }

    // Open the file we will copy TO
    thereFile = open(thereFileName, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if (thereFile < 0) {
	if (WriteError == 0) {
	    LogMsg(0,LogLevel,LogFile, "ERROR: MoveFile(%s,%s,%s) ==> Cannot open file %s", filename, hereDir, thereDir, thereFileName);
            LogMsg(0,LogLevel,LogFile, "       This error will not be reported again until the situation is resolved.  At that time, we will report the total number of failures.");
            LogMsg(0,LogLevel,EventFile, "movefile: open %s",thereFileName);
        }
	WriteError++;
	close(hereFile);
	return(1);
    } else if ( /* OPEN SUCCEEDED and */ WriteError > 0) {
       LogMsg(0,LogLevel,LogFile, "ERROR RESOLVED: MoveFile(%s,%s,%s) ==> Successfully opened file %s after %d open failures", filename, hereDir, thereDir, thereFileName, WriteError);
       LogMsg(0,LogLevel,EventFile, "movefile: open %s ==> problem resolved after %d failures",thereFileName, WriteError);
       WriteError = 0;
    }
    
    // Copy the file
    LogMsg(1000,LogLevel,LogFile, "Moving %s to %s", hereFileName, thereFileName);
    assert(hereFile >= 0);
    assert(thereFile >= 0);

    code = myfilecopy(hereFile, thereFile);
    close(hereFile);
    close(thereFile);

    if (code == 0)
	unlink(hereFileName);
    else
	unlink(thereFileName);
    return(code);
}


char *GetDataFile(char *here) {
    DIR *hereDIR;
    struct direct *dirent;
    static char filename[MAXPATHLEN];

    LogMsg(100,LogLevel,LogFile, "E GetDataFile(%s)", here);
    hereDIR = opendir(here);
    if ( hereDIR == NULL) {
        LogMsg(0,LogLevel,LogFile, "Error:  GetDataFile(%s) -- cannot open directory", here);
	printf("Error!!  GetDataFile cannot open %s", here);
	printf("Please try notify Maria and send her a listing of this directory (if possible).");
	fflush(stdout);
    }
    assert(hereDIR != NULL);
    for (dirent = readdir(hereDIR); dirent != NULL; dirent = readdir(hereDIR))
	if ((dirent->d_name[0] != '#') &&
	    (dirent->d_name[0] != '.')) {
		LogMsg(1000,LogLevel,LogFile, "GetDataFile: %s",dirent->d_name);
		(void) strncpy(filename, dirent->d_name, MAXPATHLEN);
		closedir(hereDIR);
		LogMsg(1000,LogLevel,LogFile, "L GetDataFile() -> %s", filename);
		return(filename);
		}
    closedir(hereDIR);
    LogMsg(100,LogLevel,LogFile, "L GetDataFile() -> NULL");
    return(NULL);
}


/*********************************************************
 *******************  Signal Handlers  *******************
 *********************************************************/

// Handler for SIGTERM
void Shutdown(int code) {
  LogMsg(100,LogLevel,LogFile,"Received a SIGTERM to Shutdown, code = %d", code);
  LWP_NoYieldSignal(&shutdownSync);
}

void UserDrivenEvent(int code) {
  LogMsg(100,LogLevel,LogFile,"Received a SIGUSR1 event, code = %d", code);
  LWP_NoYieldSignal(&userSync);
}

// Handler for SIGCHLD (to collect result of ASRs)
void Child(int code) { 
  union wait status;            /* so we can explain what happend */
  int pid;                      /* process id */
  int doneone = 0;

  LogMsg(1000,LogLevel,LogFile,"Child: SIGCHLD event, code = %d", code);
  
  do {                          /* in case > 1 kid died  */
#ifdef __BSD44__
    pid = wait3(&status.w_status, WNOHANG,(struct rusage *)0);
#else
    pid = wait3(&status, WNOHANG,(struct rusage *)0);
#endif
    if (pid>0) {                        /* was a child to reap  */
      LogMsg(100,LogLevel,LogFile,"Child: Child %d died, rc=%d, coredump=%d, termsig=%d",
	     pid, status.w_retcode, status.w_coredump, status.w_termsig);

      doneone=1;
      CHILDresult = status.w_retcode;
      if (pid == CHILDpid) {
	if (ReconnectionQuestionnaireInProgress == 1) {
	  LogMsg(100,LogLevel,LogFile, "Child Signal Handler: Reconnection Questionnaire finished.\n");
	  ReconnectionQuestionnaireInProgress = 0;
	  CHILDpid = 0;
	  if (CHILDresult != 0) {
	      LogMsg(0,LogLevel,LogFile, "Reconnection: ERROR ==> tcl script returned %d", CHILDresult);
              LogMsg(0,LogLevel,EventFile, "childreturned: reconn %d",CHILDresult);
          }
	  else
	      // Move tmpFileName over to FileName (still in /tmp)
	      rename(tmpReconnectionFileName, ReconnectionFileName);
	}
	else {
	  LogMsg(1000,LogLevel,LogFile,"Child Signal Handler: Return result=%d of pid=%d to waiting process", CHILDresult, pid);
	  LWP_NoYieldSignal((char *)&CHILDpid);
	}
      }
      else {
	LogMsg(0,LogLevel,LogFile,"Child Signal Handler: Unrecognized child");
      }
    }
    else {
      if (!doneone) {
	LogMsg(0,LogLevel,LogFile,"Child Signal Handler: SIGCHLD event, but no child died");
      }
    }
  } while (pid > 0);
}

/*********************************************************
 *************  Procedure Bodies for Vprocs  *************
 *********************************************************/

// responsible for handling keep alive messages
void KeepAliveHandler(char *c) {
  long code;
  int max, used;

  LogMsg(100,LogLevel,LogFile, "KeepAliveHandler: Initializing...30, %x",&keepaliveSync);
  RegisterDaemon(30, &keepaliveSync);

  // Wait for and handle user events 
  while (1) {

    assert(LWP_WaitProcess(&keepaliveSync) == LWP_SUCCESS);
    LogMsg(100,LogLevel,LogFile, "KeepAliveHandler: pinging venus...");

    code = ConnectionAlive(VenusCID, uid);
    if (code != RPC2_SUCCESS) {
      LogMsg(0,LogLevel,LogFile, "KeepAliveHandler: connected dead --> restarting");
      (void) RPC2_Unbind(VenusCID);
      (void) RPC2_Unbind(rpc2_LocalPortal.Value.InetPortNumber);
      InformVenusOfOurExistance(HostName);
    } 
    else 
      LogMsg(100,LogLevel,LogFile, "KeepAliveHandler: connection alive");

  }
  
}

// responsible for moving data from /tmp into /coda
void DataHandler() {
    int max, used;

    LogMsg(100,LogLevel,LogFile, "DataHandler: Initializing...150, %x", &dataSync);
    RegisterDaemon(150, &dataSync);

    while (1) {
        char *component = NULL;

        assert(LWP_WaitProcess(&dataSync) == LWP_SUCCESS);

        LogMsg(100,LogLevel,LogFile, "DataHandler: Checking for data...");

        while (component = GetDataFile(tmpDir)) {
	    (void) MoveFile(component, tmpDir, WorkingDir);
            LWP_DispatchProcess();
        }
    }
}

// responsible for handling user-driven events
void UserEventHandler(char *c) {
  FILE *commandFile;
  int rc;
  char command[LINELENGTH];
  int max, used;

  LogMsg(100,LogLevel,LogFile, "UserEventHandler: Initializing...");

  assert(LWP_WaitProcess(&initialUserSync) == LWP_SUCCESS);
  SetupDefaultAdviceRequests();

  // Start the user control panel
  {
    char arg[smallStringLength];
    char *args[3];

    args[0] = USERINITIATED;
    snprintf(arg, smallStringLength, "%d", thisPID);
    args[1] = arg;
    args[2] = NULL;

    int rc = fork_tcl(USERINITIATED, args);
    if (rc == -1) {
      LogMsg(0,LogLevel,LogFile, "UserEventHandler: fork_tcl ERROR");
      LogMsg(0,LogLevel,EventFile, "UserEventHandler: fork_tcl ERROR");
      exit(-1);
    }
  }

  LogMsg(100,LogLevel,LogFile, "UserEventHandler: Waiting...");

  while (1) {
    // Wait for and handle user events 
    assert(LWP_WaitProcess(&userSync) == LWP_SUCCESS);


    // Handle user event
    LogMsg(100,LogLevel,LogFile, "UserEventHandler:  User requested event...");

    /* Open the command file */
    commandFile = fopen(CommandFileName, "r");
    if (commandFile == NULL) {
      fprintf(stderr, "Command file (%s) initialization failed (errno=%d)\n\n", 
	      CommandFileName, errno);
      fflush(stderr);
      continue;
    }

    /* Read the command line */
    if (fgets(command, LINELENGTH, commandFile) == NULL) {
      fprintf(stderr, "Command file empty\n\n");
      fflush(stderr);
      continue;
    }

    /* Close the command file */
    fclose(commandFile);

    switch (command[0]) {
      case SolicitHoardAdvice:
        SolicitAdviceOnNextHoardWalk();
        break;
      case UnsolicitHoardAdvice:
        UnsolicitAdviceOnNextHoardWalk();
        break;
      case BeginStoplight:
	BeginStopLightMonitor();
	break;
       case EndStoplight:
	EndStopLightMonitor();
	break;
      case RequestMissList:
        HandleWeakAdvice(); 
        break;
      case RequestLongFetchQuery:
	SolicitWeakMissAdvice();
        break;
      case UnrequestLongFetchQuery:
        UnsolicitWeakMissAdvice();
        break;
      case RequestDiscoMissQs:
	SolicitDiscoMissQs();
	break;
      case UnrequestDiscoMissQs:
	UnsolicitDiscoMissQs();
	break;
      default:
	break;
    }

   unlink(CommandFileName);
  }
}

// responsible for shutting down the AdviceMonitor after a SIGTERM.
void ShutdownHandler(char *c) {
  int max, used;

  LogMsg(100,LogLevel,LogFile, "ShutdownHandler: Waiting...");

  assert(LWP_WaitProcess(&shutdownSync) == LWP_SUCCESS);

  LogMsg(100,LogLevel,LogFile, "ShutdownHandler: Shutdown imminent...");

  /* Shutdown user_initiated menubar and monitors */
  if (CurrentStoplightState != SLquit)
      StoplightStateChange(SLquit);

  if (!AwaitingUserResponse) 
    /* Inform Venus of our untimely demise. */
    (void) ImminentDeath(VenusCID, (RPC2_String) HostName, uid, rpc2_LocalPortal.Value.InetPortNumber);

  (void) RPC2_Unbind(VenusCID);
  (void) RPC2_Unbind(rpc2_LocalPortal.Value.InetPortNumber);

  fclose(LogFile);
  fclose(EventFile);
  exit(0);
}

// responsible for informing venus about the results of the ASR.
void EndASREventHandler(char *c) {
  int max, used;

  // there might be some race condition in the code below with other
  // users/objects needing ASRs to execute.
  while (1) {
    if (CHILDpid && ASRinProgress) {
      long code = ResultOfASR(VenusCID, NumASRStarted, CHILDresult);
      CHILDresult = -1;
      CHILDpid = 0; 

      if (code != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile,"EndASREventHandler:  ERROR ==> ResultOfASR failed (%s)", RPC2_ErrorMsg((int)code));
        LogMsg(0,LogLevel,EventFile, "EndASRevent: failed %s",RPC2_ErrorMsg((int)code));
      }
      else
	LogMsg(100,LogLevel,LogFile, "EndASREventHandler:  Venus knows the result of the ASR.");
    }
    else {
	assert(LWP_WaitProcess((char *)&CHILDpid) == LWP_SUCCESS);
    }
  }
}

#if defined(__linux__) || defined(__BSD44__)

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

#endif /* __linux__ || __BSD44__ */



