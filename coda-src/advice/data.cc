#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef __BSD44__
#include <sys/dir.h>
#else
#include <dirent.h>
#endif
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __MACH__
extern int bzero(char *, int);
#endif /* __MACH__ */

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>

#include "advice_srv.h"
#include "data.h"
#include "filecopy.h"
#include "globals.h"

const int timeLength = 26;

/* Error checking variables */
int ReadError = 0;
int WriteError = 0;



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

void InitializeDirectory(char *dirname, char *username) {
  CreateDataDirectory(dirname);
  CreateREADMEFile(dirname);
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
    struct dirent *dirent;
    static char filename[MAXPATHLEN];

    LogMsg(100,LogLevel,LogFile, "E GetDataFile(%s)", here);
    hereDIR = opendir(here);
    if ( hereDIR == NULL) {
        LogMsg(0,LogLevel,LogFile, "Error:  GetDataFile(%s) -- cannot open directory", here);
	printf("Error!!  GetDataFile cannot open %s\n", here);
	printf("Please try notify Maria and send her a listing of this directory (if possible).\n");
	printf("errno = %d\n", errno);
	fflush(stdout);
	return(NULL);
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

