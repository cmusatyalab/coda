#ifdef __cplusplus
extern "C" {
#endif __cplusplus

/* System-wide include files */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __MACH__
extern int getopt(int, char **, char *);
#endif /* __MACH__ */

#ifdef __cplusplus
}
#endif __cplusplus


/* Coda include files */
#include <util.h>

/* Local include files */
#include "advice_srv.h"
#include "data.h"
#include "globals.h"



/* Venus Version Information */
int VenusMajorVersionNumber;
int VenusMinorVersionNumber;

/* Command Line Arguments */
int debug = 0;
int verbose = 0;
int AutoReply = 0;               /* -a will set (or unset) this value */ 
int LogLevel = 0;                /* -d <int> */
int StackChecking = 0;		 /* -s */


/* Logging Information */
FILE *LogFile;
char LogFileName[MAXPATHLEN];       /* DEFAULT: /coda/usr/<username>/.questionnaires/advice.log */
FILE *EventFile;
char EventFileName[MAXPATHLEN];     /* DEFAULT: /coda/usr/<username>/.questionnaires/advice.event */

/* Log Levels */
int EnterLeaveMsgs = 100;

/* Storage Information */
char BaseDir[MAXPATHLEN];
char WorkingDir[MAXPATHLEN];
char tmpDir[MAXPATHLEN];

/* Error Information */
char error_msg[BUFSIZ];

/* Process Information */
int thisPGID;
int thisPID;
int childPID;
int interfacePID = -1;

/* User Information */
char UserName[16];
int uid = -1;

/* Host Information */
char ShortHostName[MAXHOSTNAMELEN];
char HostName[MAXHOSTNAMELEN];

/* Connection Information */
int WeLostTheConnection = FALSE;

/* Synchronization Variables */
char shutdownSync;
char dataSync;
char programlogSync;
char replacementlogSync;
char initialUserSync;
char userSync;
char workerSync;

char discomissSync;
char weakmissSync;
char readmissSync;
char miscSync;
char hoardwalkSync;

/* Return Answer Variables */
int weakmissAnswer;
int readmissAnswer;

/* RPC Parameter Values */
char ProgramAccessLog[MAXPATHLEN];
char ReplacementLog[MAXPATHLEN];

/* Command-Line Argument Routines */
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


/* Initialization Routines */

void InitLogFile() {
  // LogFileName: 
  //    /coda/usr/<username>/.questionnaires/<HOSTNAME>_<PID>_<DATE:TIME>/advice.log  
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
  // EventFileName:
  //   /coda/usr/<username>/.questionnaires/<HOSTNAME>_<PID>_<DATE:TIME>/advice.event  
    snprintf(EventFileName, MAXPATHLEN, "%s/advice.event", WorkingDir);

    EventFile = fopen(EventFileName, "a");
    if (LogFile == NULL) 
        LogMsg(0,LogLevel,LogFile, "EVENTFILE (%s) initialization failed (errno=%d)", EventFileName, errno);
}

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
    error += setenv("TIX_LIBRARY", TIX, 1);
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


