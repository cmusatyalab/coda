

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <struct.h>

#include <lwp.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <vcrcommon.h>
#include <venusioctl.h>

#include "advice_srv.h"
#include "mybstree.h"
#include "programspy.h"

char ProfileDir[MAXPATHLEN];

int IsProgramUnderWatch(char *);



char *GetFullPath(char *, char *, int);

char *GetFullPath(char *fidStr, char *path, int length) {
    struct ViceIoctl vio;
    ViceFid fid;
    char relativePath[MAXPATHLEN];
    char rootPath[MAXPATHLEN];
    int rc;

    rc = sscanf(fidStr, "<%lx.%lx.%lx>", &fid.Volume, &fid.Vnode, &fid.Unique);
    if (rc != 3) {
       fprintf(stderr, "ERROR!\n");
       return(NULL);
    }

    /* Get its path */
    vio.in = (char *)&fid;
    vio.in_size = (int) sizeof(ViceFid);
    vio.out = relativePath;
    vio.out_size = MAXPATHLEN;
    rc = pioctl("/coda", VIOC_GETPATH, &vio, 0);
    if (rc < 0) {
      perror("  VIOC_GETPATH");
      fprintf(stderr, "Path for fid=<%x.%x.%x>\n", fid.Volume, fid.Vnode, fid.Unique);
      return(NULL);
    }

    vio.in = (char *)&fid.Volume;
    vio.in_size = (int) sizeof(VolumeId);
    vio.out = rootPath;
    vio.out_size = MAXPATHLEN;

    /* Do the pioctl */
    rc = pioctl("/coda", VIOC_GET_MT_PT, &vio, 1);
    if (rc < 0) {
      perror("  VIOC_GET_MT_PT");
      fprintf(stderr, "Mount point for vid=%x\n", fid.Volume);
      return(NULL);
    }

    if (snprintf(path, length, "%s%s", rootPath, &(relativePath[1])) >= length) {
      fprintf(stderr, "path too long\n");
      return(NULL);
    }
    return(path);
}

#if 0

int IncludeInProgramProfile(char *program, char *path, char *profileDir) {
    char profileName[MAXPATHLEN];
    FILE *profile;
    char *returnValue;
    char line[MAXPATHLEN];
    int rc;

    snprintf(profileName, MAXPATHLEN, "%s/%s", profileDir, program);

    profile = fopen(profileName, "a+");
    if (profile == NULL) {
        fprintf(stderr, "ERROR!  Can't open %s\n", profileName);
	return(-1);
    }

    rewind(profile);

    returnValue = fgets(line, MAXPATHLEN, profile);
    while (returnValue != NULL) {
	if (strncmp(line, path, strlen(path)) == 0) {
	    if (fclose(profile) != 0) {
	        perror("  fclose program profile");
		exit(-1);
	    }
	    return(1);
	}

	returnValue = fgets(line, MAXPATHLEN, profile);
    }

    // Note:  We didn't find path in this program file.  
    //        We should now be at the end of the file so
    //          we can just add this path to the list.
    rc = fprintf(profile, "%s\n", path);
    if (rc-1 != strlen(path)) {
        perror("ERROR!  Didn't write out the correct number of characters.\n");
	exit(-1);
    }
    
    if (fflush(profile) != 0) {
        perror("  fflush program profile");
	exit(-1);
    }

    if (fclose(profile) != 0) {
        perror("  fclose program profile");
	exit(-1);
    }
}

#endif

int IncludeInProgramProfile(char *program, char *path, char *profileDir) {
    char profileName[MAXPATHLEN];
    FILE *profile;
    char *returnValue;
    char line[MAXPATHLEN];
    int rc;

    snprintf(profileName, MAXPATHLEN, "%s/%s", profileDir, program);

    profile = fopen(profileName, "r");
    if (profile == NULL) {
        fprintf(stderr, "ERROR!  Can't open %s\n", profileName);
	return(-1);
    }

    returnValue = fgets(line, MAXPATHLEN, profile);
    while (returnValue != NULL) {
	if (strncmp(line, path, strlen(path)) == 0) {
	    if (fclose(profile) != 0) {
	        perror("  fclose program profile");
		exit(-1);
	    }
	    return(1);
	}

	returnValue = fgets(line, MAXPATHLEN, profile);
    }

    // Note:  We didn't find path in this program file so we must add it.

    profile = freopen(profileName, "a+", profile);

    rc = fprintf(profile, "%s\n", path);
    if (rc-1 != strlen(path)) {
        perror("ERROR!  Didn't write out the correct number of characters.\n");
	exit(-1);
    }
    
    if (fflush(profile) != 0) {
        perror("  fflush program profile");
	exit(-1);
    }

    if (fclose(profile) != 0) {
        perror("  fclose program profile");
	exit(-1);
    }
    return 0;
}



void ProcessAccessRecordLine(char *program, char *fidString, char *profileDir) {
    char Path[MAXPATHLEN];
    int rc;
    int vol, vnode, unique;

    rc = sscanf(fidString, "<%x.%x.%x>", &vol, &vnode, &unique);
    if (rc != 3) {
      fprintf(stderr, "fidString is screwy: %s\n", fidString);
      fflush(stderr);
      exit(-1);
    }
    if (IsProgramAccessingUserArea((VolumeId)vol))
      return;

    GetFullPath(fidString, Path, MAXPATHLEN);
    IncludeInProgramProfile(program, Path, profileDir);
}

void ProcessProgramAccessLog(char *logFile, char *profileDir) {
    FILE *accessLog;
    char line[MAXPATHLEN];
    char thing1[MAXPATHLEN], thing2[MAXPATHLEN], thing3[MAXPATHLEN];
    char *returnValue;
    int rc;
    int lineCount = 0;

    int watching = 0;
    char activeProgram[MAXPATHLEN];

    accessLog = fopen(logFile, "r");
    if (accessLog == NULL) return;
   
    returnValue = fgets(line, MAXPATHLEN, accessLog);
    while (returnValue != NULL) {
        lineCount++;
        rc = sscanf(line, "%s %s %s\n", thing1, thing2, thing3);
	if (rc == 3) {
	    if (watching)
	        ProcessAccessRecordLine(activeProgram, thing3, profileDir); 
	} else if (rc == 2) {
	    if (IsProgramUnderWatch(thing2)) {
	        watching = 1;
		strncpy(activeProgram, thing2, MAXPATHLEN);
	    } else {
	        watching = 0;
	    }
	} else {
	    fprintf(stderr, "ProcessProgramAccessLog: bogus input line (incorrect number of elements -- should be 2 or 3:\n");
	    fprintf(stderr, "\tLine=*%s*\n", line);
	    fprintf(stderr, "\tIgnoring one line\n");
	    fflush(stderr);
	}
	
	returnValue = fgets(line, MAXPATHLEN, accessLog);

        if ((lineCount % 50) == 0) 
	    Yield();
    }

    fclose(accessLog);
}


/****************************************************************************
 ***                                                                      ***
 ***  This section of code deals with parsing the program definitions to  ***
 ***  build a data structure that can be used to determine which programs ***
 ***  we should be watching for.                                          ***
 ***                                                                      ***
 ***  N.B. This code is somewhat brain-dead.  I'm using a binary search   ***
 ***       tree that does not implement a binary search.  If it did, then ***
 ***       the IsMember function ought to use it.  If this code becomes   ***
 ***       too inefficient for production use, this is a good place to    ***
 ***       start!                                                         ***
 ***                                                                      ***
 ***                                                                      ***
 ***                                                       mre, 11/30/97  ***
 ***                                                                      ***
 ****************************************************************************/

bstree *PWDB;

progent::progent(char *Program) {
    bsnode *existing_program;
    program = new char[strlen(Program)+1];

    strcpy(program, Program);

    CODA_ASSERT(PWDB != NULL);
    existing_program = PWDB->get(&queue_handle);
    if (existing_program != NULL) {
      delete this;
    } else {
      CODA_ASSERT(PWDB != NULL);
      PWDB->insert(&queue_handle);
    }
}

progent::progent(progent& i) {
    abort();
}

progent::operator=(progent& i) {
    abort();
    return(0);
}

progent::~progent() {
    CODA_ASSERT(PWDB != NULL);
    PWDB->remove(&queue_handle);
    delete[] program;
}

void progent::print(FILE *outfile) {
    fprintf(outfile, "%s\n", program);
}

// Friend of class progent
void PrintPWDB(char *filename) {
    FILE *outfile;
    bsnode *b;
    bstree_iterator next(*PWDB, BstDescending);

    outfile = fopen(filename, "a+");
    if (outfile == NULL) {
      fprintf(stderr, "ERROR:  Cannot open %s\n", filename);
      return;
    }

    fprintf(outfile, "Printing PWDB elements:\n");
    while (b = next()) {
        progent *prog;
	CODA_ASSERT(b != NULL);
	prog = strbase(progent, b, queue_handle);
	CODA_ASSERT(prog != NULL);
	prog->print(outfile);
    }
    fprintf(outfile, "Done.\n\n");
    fflush(outfile);
    fclose(outfile);
}

int ProgramPriorityFN(bsnode *b1, bsnode *b2) {
    progent *p1;
    progent *p2;

    CODA_ASSERT(b1 != NULL);
    CODA_ASSERT(b2 != NULL);

    p1 = strbase(progent, b1, queue_handle);
    p2 = strbase(progent, b2, queue_handle);

    CODA_ASSERT(p1 != NULL);
    CODA_ASSERT(p2 != NULL);

    return(strcmp(p1->program, p2->program));
}

void InitPWDB() {
    PWDB = new bstree(ProgramPriorityFN);
}

// N.B. Brain-dead.  Using binary search tree without efficient search method,
//      given just the name of a string to search for...  If production code 
//      too slow, create such a method.  For now, just iterate!
int IsProgramUnderWatch(char *program) {
  bsnode *b;
  bstree_iterator next(*PWDB, BstDescending);

  while (b = next()) {
      progent *p = strbase(progent, b, queue_handle);
      CODA_ASSERT(p != NULL);
      if (strcmp(p->program, program) == 0) {
	  return(1);
      }
  }
  return(0);
}

// Now for the code to parse the program definition file
void ParseProgramDefinitions(char *filename) {
    char line[MAXPATHLEN];
    FILE *programDefs;
    char *returnValue;
    int rc = 0;
    int lookingForProgramName = 1;
    char programName[MAXPATHLEN];
    char *beginOfProgramName;
    progent *p;

    if (PWDB != NULL) {
        PWDB->clear();
    }
    programDefs = fopen(filename, "r");
    if (programDefs == NULL) return;

    returnValue = fgets(line, MAXPATHLEN, programDefs);
    while (returnValue != NULL) {

        if (strcmp(line, "\n") == 0) {
	    returnValue = fgets(line, MAXPATHLEN, programDefs);
	    lookingForProgramName = 1;
	    continue;
	}

	if (lookingForProgramName) {
	    CODA_ASSERT(line[strlen(line)-2] == ':');
	    lookingForProgramName = 0;
	    returnValue = fgets(line, MAXPATHLEN, programDefs);
	    continue;
	} else {
	    beginOfProgramName = rindex(line, (int)'/');
	    line[strlen(line)-1] = '\0';
	    if (beginOfProgramName)
	        beginOfProgramName++;
	    else
	        beginOfProgramName = line;
	    p = new progent(beginOfProgramName);
	    returnValue = fgets(line, MAXPATHLEN, programDefs);
	    continue;
	}
    }

    fclose(programDefs);
}

/****************************************************************************
 ***                                                                      ***
 *** End of program definition parsing section.                           ***
 ***                                                                      ***
 ****************************************************************************/



/****************************************************************************
 ***                                                                      ***
 ***  This section of code deals with parsing the data definitions to     ***
 ***  build a data structure that can be used to determine when programs  ***
 ***  venture into user data areas.                                       ***
 ***                                                                      ***
 ***  N.B. This code is somewhat brain-dead.  I'm using a binary search   ***
 ***       tree that does not implement a binary search.  If it did, then ***
 ***       the IsMember function ought to use it.  If this code becomes   ***
 ***       too inefficient for production use, this is a good place to    ***
 ***       start!                                                         ***
 ***                                                                      ***
 ***                                                                      ***
 ***                                                       mre, 11/30/97  ***
 ***                                                                      ***
 ****************************************************************************/

bstree *UADB;

dataent::dataent(VolumeId vol) {
    bsnode *existing_dataarea;
    
    volume = vol;
    CODA_ASSERT(UADB != NULL);
    if (!UADB->IsMember(&queue_handle)) {
        UADB->insert(&queue_handle);
    } else {
      delete this;
    }
}

dataent::dataent(dataent& i) {
    abort();
}

dataent::operator=(dataent& i) {
    abort();
    return(0);
}

dataent::~dataent() {
    CODA_ASSERT(UADB != NULL);
    UADB->remove(&queue_handle);
}

void dataent::print(FILE *outfile) {
    fprintf(outfile, "%x\n", volume);
}

void PrintUADB(char *filename) {
    FILE *outfile;
    bsnode *b;
    bstree_iterator next(*UADB, BstDescending);

    outfile = fopen(filename, "a+");
    if (outfile == NULL) {
      fprintf(stderr, "ERROR:  Cannot open %s\n", filename);
      return;
    }

    fprintf(outfile, "Printing UADB elements:\n");
    while (b = next()) {
        dataent *dataarea;
	CODA_ASSERT(b != NULL);
	dataarea = strbase(dataent, b, queue_handle);
	CODA_ASSERT(dataarea != NULL);
	dataarea->print(outfile);
    }
    fprintf(outfile, "Done.\n\n");
    fflush(outfile);
    fclose(outfile);
}

int DataAreaPriorityFN(bsnode *b1, bsnode *b2) {
    dataent *d1;
    dataent *d2;

    CODA_ASSERT(b1 != NULL);
    CODA_ASSERT(b2 != NULL);

    d1 = strbase(dataent, b1, queue_handle);
    d2 = strbase(dataent, b2, queue_handle);

    CODA_ASSERT(d1 != NULL);
    CODA_ASSERT(d2 != NULL);

    if (d1->volume == d2->volume)
      return(0);
    else if (d1->volume < d2->volume)
      return(-1);
    else
      return(1);
}

void InitUADB() {
    UADB = new bstree(DataAreaPriorityFN);
}

// N.B. Brain-dead.  Using binary search tree without efficient search method,
//      given just the volume id to search for...  If production code too slow,
//      create such a method.  For now, just iterate!
int IsProgramAccessingUserArea(VolumeId vol) {
  bsnode *b;
  bstree_iterator next(*UADB, BstDescending);

  while (b = next()) {
      dataent *d = strbase(dataent, b, queue_handle);
      CODA_ASSERT(d != NULL);
      if (d->volume == vol)
          return(1);
  }
  return(0);  
}

void ParseDataDefinitions(char *filename) {
    char line[MAXPATHLEN];
    FILE *dataDefs;
    char *returnValue;
    int rc = 0;
    char pathname[MAXPATHLEN];
    int lookingForDefinitionName = 1;
    char *beginOfDefinitionName;
    dataent *d;

    if (UADB != NULL) {
        UADB->clear();
    }
    dataDefs = fopen(filename, "r");
    if (dataDefs == NULL) return;

    returnValue = fgets(line, MAXPATHLEN, dataDefs);
    while (returnValue != NULL) {

        if (strcmp(line, "\n") == 0) {
	    returnValue = fgets(line, MAXPATHLEN, dataDefs);
	    lookingForDefinitionName = 1;
	    continue;
	}

	if (lookingForDefinitionName) {
	    CODA_ASSERT(line[strlen(line)-2] == ':');
	    lookingForDefinitionName = 0;
	    returnValue = fgets(line, MAXPATHLEN, dataDefs);
	    continue;
	} else {
  	    char pioctlbuf[BUFSIZ];
	    struct ViceIoctl vio;
	    ViceFid fid;

	    rc = sscanf(line, "%s ", pathname);

	    /* Get its path */
	    vio.in = 0;
	    vio.in_size = 0;
	    vio.out = pioctlbuf;
	    vio.out_size = BUFSIZ;
	    bzero(pioctlbuf, BUFSIZ);

	    rc = pioctl(pathname, VIOC_GETFID, &vio, 0);
	    if (rc < 0) {
	        fprintf(stderr, "Here\n"); fflush(stderr);
		perror("VIOC_GETFID"); 
		returnValue = fgets(line, MAXPATHLEN, dataDefs);
		continue;
	    }

	    bcopy((const void *)pioctlbuf, (void *)&fid, (int) sizeof(ViceFid));
	    d = new dataent(fid.Volume);
	    returnValue = fgets(line, MAXPATHLEN, dataDefs);
	    continue;
	}
    }

    fclose(dataDefs);
}

/****************************************************************************
 ***                                                                      ***
 *** End of data definition parsing section.                              ***
 ***                                                                      ***
 ****************************************************************************/

