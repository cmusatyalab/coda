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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/advice/advice_srv.cc,v 4.7 98/01/26 21:38:17 mre Exp $";
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

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <assert.h> 
#include <struct.h>
#include <sys/wait.h> 
#include <sys/stat.h>
#include <sys/file.h>
#ifdef __BSD44__
#include <sys/dir.h>
#endif
#include <pwd.h>
#include <errno.h>
#include <strings.h>
#include <lock.h>
#include <lwp.h>
#include <rpc2.h>


extern int table(int id, int index, char *addr, int nel, int lel);
extern int ffilecopy(FILE*, FILE*);

#ifdef __cplusplus
}
#endif __cplusplus

extern int myfilecopy(int,int);
extern void DispatchDaemons();
extern void DaemonInit();
extern void RegisterDaemon(unsigned long, char *);

/* from util */
#include <util.h>
#include <proc.h>
#include <bstree.h>

/* from vicedep */
#include "admon.h"
#include "adsrv.h"

/* from venus */
#include <advice.h>

/* local */
#include "advice_srv.h"
#include "console_handler.h"
#include "counters.h"
#include "data.h"
#include "globals.h"
#include "helpers.h"
#include "miss.h"
#include "programspy.h"
#include "replacementlog.h"
#include "rpc_setup.h"
#include "volent.h"

extern int NumASRStarted;
int ASRinProgress = 0;
int ReconnectionQuestionnaireInProgress = 0;

PROCESS lwpPID;
int CHILDresult = -1;

/***** Function Definitions *****/
void PrintMissList(char *filename);
void InitMissQueue();
void InitVDB();
int GetAdvice(char *);
void Shutdown(int);
void Child(int);
void UserDrivenEvent(int);
void CreateLWPs();
void WorkerHandler();
void DataHandler();
void CodaConsoleHandler(char *);
void ShutdownHandler(char *);
void EndASREventHandler(char *);
void ProgramLogHandler();
void ReplacementLogHandler();

extern char **environ;

/* RPC Variables */
extern RPC2_PortalIdent rpc2_LocalPortal;

RPC2_Handle cid;
RPC2_RequestFilter reqfilter;
RPC2_PacketBuffer *reqbuffer;

/* Lock for whether or not we can issue an advice request */
struct Lock VenusLock;

/* Internal Variables */
char tmpReconnectionFileName[MAXPATHLEN];
char ReconnectionFileName[MAXPATHLEN];

int AwaitingUserResponse = FALSE;

int consolepid, asrendpid, shutdownpid, keepalivepid;
int datapid, mainpid, workerpid, programlogpid, replacementlogpid;

/* Interest and Monitor Variables */
int StoplightInterest = FALSE;
StoplightStates CurrentStoplightState = SLquit;



/******************************************************************
 *************************  MAIN Routine  *************************
 ******************************************************************/

main(int argc, char *argv[])
{
  long rc;
  int sig;

  // Initialization
  InitializeCodaConsole();          // Start the user control panel
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

  Init_RPC(&mainpid);                          // Initialize RPC...
  assert(IOMGR_Initialize() == LWP_SUCCESS);  // and IOMGR
  DaemonInit();		                      // and the daemon package

  Lock_Init(&VenusLock);            // This lock controls access to Venus

  CreateLWPs();

  // Inform Venus of the availability of an AdviceMonitor for this user
  InformVenusOfOurExistance(HostName, uid, thisPGID);

  assert(LWP_NoYieldSignal(&initialUserSync) == LWP_SUCCESS);

  // Set up the request filter:  we only service ADMON subsystem requests.
  reqfilter.FromWhom = ANY;
  reqfilter.OldOrNew = OLDORNEW;
  reqfilter.ConnOrSubsys.SubsysId = ADMONSUBSYSID;

  // Loop forever, waiting for Venus to ask us for advice
  LogMsg(100,LogLevel,LogFile, "main:  Entering Request Loop");
  struct timeval DaemonExpiry;
  DaemonExpiry.tv_sec = 2;
  DaemonExpiry.tv_usec = 0;
  if (LogLevel==1000) RPC2_DebugLevel = 1000;
  for ( ; ; ) {

    // Wait for a request
    rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, &DaemonExpiry, NULL, (long)NULL, NULL) ;
    if (rc == RPC2_TIMEOUT) {
      // Fire daemons that are ready to run. 
      DispatchDaemons();

      // Check for input on the Console pipe
      sig = LWP_SignalProcess(&userSync);
      assert((sig == LWP_SUCCESS) || (sig == LWP_ENOWAIT));
      continue;
    }
    else if (rc != RPC2_SUCCESS) {
      LogMsg(0,LogLevel,LogFile, "main: ERROR ==> GetRequest (%s)", RPC2_ErrorMsg((int)rc));
      LogMsg(0,LogLevel,EventFile, "GetRequest: %s", RPC2_ErrorMsg((int)rc));
    }

    assert(LWP_SignalProcess(&workerSync) == LWP_SUCCESS);
  }
}

void Yield() {
    IOMGR_Poll();
    LWP_DispatchProcess();
}

void CreateLWPs() {
  char c, s;

  /****************************************************
   ******   Create LWPs to handle signal events  ******
   ****************************************************/

  // SIGCHLD signals the completion of an ASR.
  assert((LWP_CreateProcess((PFIC)EndASREventHandler,
			   DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "ASR End Signal Handler",
			   (PROCESS *)&asrendpid)) == (LWP_SUCCESS));

  // SIGTERM signals the AdviceMonitor to shut down
  //This one was normal+1
  assert(LWP_CreateProcess((PFIC)ShutdownHandler,
			   DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&s, "Shutdown Handler",
			   (PROCESS *)&shutdownpid) == LWP_SUCCESS);

  // Initialize signal handlers
  signal(SIGTERM, Shutdown);
  assert(!signal(SIGCHLD, Child));


  /*******************************************************
   ******  Create LWPs to handle daemon activities  ******
   *******************************************************/

  // These three were all NORMAL-1

  assert((LWP_CreateProcess((PFIC)DataHandler,
			    DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "Data Handler",
			   (PROCESS *)&datapid)) == (LWP_SUCCESS));

  assert((LWP_CreateProcess((PFIC)ProgramLogHandler,
			    DFTSTACKSIZE*1024*4, LWP_NORMAL_PRIORITY,
			    (char *)&c, "Program Log Handler",
			    (PROCESS *)&programlogpid)) == (LWP_SUCCESS));

  assert((LWP_CreateProcess((PFIC)ReplacementLogHandler,
			    DFTSTACKSIZE*1024*4, LWP_NORMAL_PRIORITY,
			    (char *)&c, "Replacement Log Handler",
			    (PROCESS *)&replacementlogpid)) == (LWP_SUCCESS));


  /****************************************************
   ****    Create LWP to handle interface events   ****
   ****************************************************/

  assert((LWP_CreateProcess((PFIC)CodaConsoleHandler,
			    DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "CodaConsole Interface Handler",
			   (PROCESS *)&consolepid)) == (LWP_SUCCESS));

  /****************************************************
   ******    Create LWP to handle venus events   ******
   ****************************************************/

  assert((LWP_CreateProcess((PFIC)WorkerHandler,
			    DFTSTACKSIZE*2*1024, LWP_NORMAL_PRIORITY,
			   (char *)&c, "Worker Handler",
			   (PROCESS *)&workerpid)) == (LWP_SUCCESS));
}

void WorkerHandler() {
    int max, before, after;
    long rc;
    static char initialRock = 'W';
    char *rock;

    LogMsg(100,LogLevel,LogFile, "WorkerHandler: Initializing...");
    assert((LWP_NewRock(42, &initialRock)) == LWP_SUCCESS);

    // Wait for and handle user events 
    while (1) {
        assert(LWP_WaitProcess(&workerSync) == LWP_SUCCESS);

	if (StackChecking) {
	    LWP_StackUsed((PROCESS)workerpid, &max, &before);
	    LogMsg(100,LogLevel,LogFile, "Worker:  Before call StackUsed returned max=%d used=%d\n",
		   max,before);
	    assert(before < 12*1024);
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
	    assert(after < 12*1024);
        }
    }
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
#endif
#ifdef __linux__
    pid = wait3(&status, WNOHANG,(struct rusage *)0);
#endif
#ifdef __CYGWIN32__
    pid = wait(&status.w_status);
#endif
    if (pid>0) {                        /* was a child to reap  */
      LogMsg(100,LogLevel,LogFile,"Child: Child %d died, rc=%d, coredump=%d, termsig=%d",
	     pid, status.w_retcode, status.w_coredump, status.w_termsig);

      doneone=1;
      CHILDresult = status.w_retcode;
      if (pid == childPID) {
	if (ReconnectionQuestionnaireInProgress == 1) {
	  LogMsg(100,LogLevel,LogFile, "Child Signal Handler: Reconnection Questionnaire finished.\n");
	  ReconnectionQuestionnaireInProgress = 0;
	  childPID = 0;
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
	  LWP_NoYieldSignal((char *)&childPID);
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
	    IOMGR_Poll();
            LWP_DispatchProcess();
        }
    }
}

void ProgramLogHandler() {
    long rc;
    char pathname[MAXPATHLEN];
    char DataFilename[MAXPATHLEN];
    char ProgramFilename[MAXPATHLEN];
    char ProfileDir[MAXPATHLEN];
    
    LogMsg(100, LogLevel,LogFile, "ProgramLogHandler: Initializing");

    snprintf(DataFilename, MAXPATHLEN, "%s/%s/%s/%s", 
	     UserVolume, UserName, HoardDir,DataFile);
    snprintf(ProgramFilename, MAXPATHLEN, "%s/%s/%s/%s", 
	     UserVolume, UserName, HoardDir,ProgramFile);
    snprintf(ProfileDir, MAXPATHLEN, "%s/%s/%s/%s", 
	     UserVolume, UserName, HoardDir,ProfileDirectoryName);

    InitPWDB();
    InitUADB();

    while (1) {
        rc = LWP_WaitProcess(&programlogSync);
	assert(rc == LWP_SUCCESS);

	ParseDataDefinitions(DataFilename);
	ParseProgramDefinitions(ProgramFilename);
	ProcessProgramAccessLog(ProgramAccessLog, ProfileDir);
	SendToConsole("Hoard");
    }
}


void ReplacementLogHandler() {
    long rc;

    LogMsg(100, LogLevel, LogFile, "ReplacementLogHandler: Initializing");
    snprintf(GhostDB, MAXPATHLEN, "%s/%d/%s", 
	     CodaSpoolingArea, uid, GhostDBName);

    Lock_Init(&GhostLock);

    while (1) {
        rc = LWP_WaitProcess(&replacementlogSync);
	assert(rc == LWP_SUCCESS);

	LogMsg(100, LogLevel, LogFile, "ReplacementLogHandler: woke up\n");
	ParseReplacementLog(ReplacementLog);
    }
}

// responsible for handling user-driven events
void CodaConsoleHandler(char *c) {
  FILE *commandFile;
  int rc = -42;
  char *input;
  int max, used;
  static char initialRock = 'C';
  char *rock;
  static int count = 0;
  char msg[128];

  LogMsg(100,LogLevel,LogFile, "UserEventHandler: Initializing...");
  assert((LWP_NewRock(42, &initialRock)) == LWP_SUCCESS);

  assert(LWP_WaitProcess(&initialUserSync) == LWP_SUCCESS);

  sprintf(msg, "\tsource %s\n", CODACONSOLE);
  SendToConsole(msg);
  printf("Sending: %s\n", msg);
  fflush(stdout);

  LogMsg(100,LogLevel,LogFile, "UserEventHandler: Waiting...");

  while (1) {
    // Wait for and handle user events 
    assert(LWP_GetRock(42, &rock) == LWP_SUCCESS);
    fprintf(stderr, "\tCodaConsole: Waiting for userSync\n");
    assert(LWP_WaitProcess(&userSync) == LWP_SUCCESS);
    fprintf(stderr, "\tCodaConsole: Received userSync\n");

    // Handle user event

    LogMsg(200,LogLevel,LogFile, "UserEventHandler:  User requested event...");

    /* Read the command line */
    ProcessInputFromConsole();
  }
}

// responsible for shutting down the AdviceMonitor after a SIGTERM.
void ShutdownHandler(char *c) {
  char msg[MAXPATHLEN];
  int max, used;

  LogMsg(100,LogLevel,LogFile, "ShutdownHandler: Waiting...");

  assert(LWP_WaitProcess(&shutdownSync) == LWP_SUCCESS);

  LogMsg(100,LogLevel,LogFile, "ShutdownHandler: Shutdown imminent...");

  /* Invalidate Indicator Lights */
  sprintf(msg, "InvalidateIndicators\n");
  SendToConsole(msg);

  if (!AwaitingUserResponse) 
    /* Inform Venus of our untimely demise. */
    ObtainWriteLock(&VenusLock);
    (void) C_ImminentDeath(VenusCID, (RPC2_String) HostName, uid, rpc2_LocalPortal.Value.InetPortNumber);
    ReleaseWriteLock(&VenusLock);

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
    if (childPID && ASRinProgress) {
      ObtainWriteLock(&VenusLock);
      long code = C_ResultOfASR(VenusCID, NumASRStarted, CHILDresult);
      ReleaseWriteLock(&VenusLock);
      CHILDresult = -1;
      childPID = 0; 

      if (code != RPC2_SUCCESS) {
	LogMsg(0,LogLevel,LogFile,"EndASREventHandler:  ERROR ==> ResultOfASR failed (%s)", RPC2_ErrorMsg((int)code));
        LogMsg(0,LogLevel,EventFile, "EndASRevent: failed %s",RPC2_ErrorMsg((int)code));
      }
      else
	LogMsg(100,LogLevel,LogFile, "EndASREventHandler:  Venus knows the result of the ASR.");
    }
    else {
	assert(LWP_WaitProcess((char *)&childPID) == LWP_SUCCESS);
    }
  }
}


