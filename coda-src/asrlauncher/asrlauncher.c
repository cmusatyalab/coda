/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2006 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

*/

/*						
 * asrlauncher.c
 *
 * This file contains all of the code that makes up the ASRLauncher.
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* Definitions */

/* ASR timeout period (milliseconds). Currently this is identical to RPC2's. */
#define ASR_TIMEOUT 60000

/* Character array lengths */

#define MAXPATHLEN     256
#define MAXCOMMANDLEN  1024
#define MAXNUMDPND     20
#define MAXDPNDLEN     (MAXPATHLEN * MAXNUMDPND)
#define MAXENVLEN      30  /* Max length of environment variable name */
#define MAXPARMS       20

/* Environment variable names */

#define CONFLICT_FILENAME             '>' /* Puneet's original 3 vars */
#define CONFLICT_WILDCARD             '*'
#define CONFLICT_PARENT               '<'

#define CONFLICT_PATH                 '=' /* Additional variables */
#define CONFLICT_VOLUME               '@'
#define CONFLICT_TYPE                 '?'

/* Conflict type enumerations.
 * Note: these values are standard also in Venus. */

#define SERVER_SERVER       1
#define SERVER_SERVER_STR  "1"
#define LOCAL_GLOBAL        2
#define LOCAL_GLOBAL_STR   "2"
#define MIXED_CONFLICT      3
#define MIXED_CONFLICT_STR "3"


/* Global Variables */

int My_Pid;                 /* getpid() result. Only for logging purposes. */

int Conflict_Type;          /* 0 = S/S, 1 = L/G, 2 = Mixed */
char *Conflict_Path;        /* Full path to the conflict. */
char Conflict_Parent[MAXPATHLEN]; /* Parent directory of conflict. */
char *Conflict_Filename;    /* Just the filename to the conflict. */
char *Conflict_Volume_Path; /* Full path to the conflict's volume's root. */
char *Conflict_Wildcard;    /* Holds the part of the pathname that matched
			     * an asterisk in a rule, for convenience.
			     * Example: *.dvi matching foo.dvi puts "foo" in
			     * this variable. */

/*
 * openRulesFile
 *
 * Takes in a pathname (with or without a filename at the end), strips
 * it down to the final '/', and concatenates the universal rules filename.
 *
 * Returns the FILE handle or NULL on failure.
 */
FILE* openRulesFile(char *conflict) {
  FILE *rules;
  char parentPath[MAXPATHLEN], *end;
  
  rules = NULL;

  strncpy(parentPath, conflict, MAXPATHLEN);

  end = strrchr(parentPath, '/');
  if(end == NULL)
    return NULL;

  *(end + 1)='\0';            /* Terminate string just after final '/'. */

  strcat(parentPath, ".asr"); /* Concatenate filename of rules file. */

  fprintf(stderr, "ASRLauncher(%d): Trying rules file: %s\n", 
	  My_Pid, parentPath);
  rules = fopen(parentPath, "r"); /* Could fail with ENOENT or EACCES
				   * at which point we loop to a
				   * higher directory to try again. */
  return rules;
}

/*
 * Perform wildcard matching between strings.
 */
int compareWithWilds(char *str, char *wildstr) {
  char *s, *w;
  int asterisk = 0;

begin:
   for (s = str, w = wildstr; *s != '\0'; s++, w++) {
      switch (*w) {

      case '?':
	/* Skip one letter. */
	break;
	
      case '*':
	asterisk = 1;
	str = s;
	wildstr = w + 1;
	
	/* Skip multiple *'s */
	while (*wildstr == '*')
	  wildstr++;
	
	/* Ending on asterisk indicates success. */
	if (*wildstr == '\0') 
	  return 0;
	
	goto begin;
	break;

      default:
	
	/* On character mismatch, check if we had a previous asterisk. */
	if(*s != *w)
	  goto mismatch;
	
	/* Otherwise, the characters match. */
	break;
      }
   }
   
   while (*w == '*') w++;

   /* If there's no string left to match wildstr chars after the final 
    * asterisk, return failure. Otherwise, success. */
   return (*w != '\0');

mismatch:
   if (asterisk == 0) return -1;
   str++;
   goto begin;
}

int parseRulesFile(FILE *rulesFile, char *asrDependency, char *asrCommand) {
  char rule[MAXPATHLEN];
  int notfound = 1;

  if((rulesFile == NULL) || (asrDependency == NULL) || (asrCommand == NULL))
    return -1;

  while(notfound > 0) {
    char *end;
    char fmt[10];

    sprintf(fmt, "%%%ds", MAXPATHLEN-1);
    if(fscanf(rulesFile, fmt, rule) == EOF) {
      fprintf(stderr, "ASRLauncher(%d): No matching rule exists in this "
	      "file.\n", My_Pid);
      return 0;
    }

    if((end = strchr(rule, ':')) == NULL) /* it's not a rule without ':' */
      continue;
    
    end[0] = '\0'; /* isolate the rule */

    /* match rule against conflict filename */
    if(compareWithWilds(Conflict_Filename, rule))
      continue;

    fprintf(stderr, "ASRLauncher(%d): Found matching (conflict name, rule): "
	    "(%s, %s)\n",  My_Pid, Conflict_Filename, rule);
 
    notfound = 0; /* don't needlessly loop anymore */

    /* Store Conflict_Wildcard match for environment variables. */
    if(Conflict_Wildcard != NULL)
      free(Conflict_Wildcard);

    Conflict_Wildcard = NULL;

    /* Store what matched that '*', if the rule led off with one. */
    if(rule[0] == '*') 
    {
      int diff; /* how many bytes to cut off of the end of Conflict_Path */

      diff = strlen(Conflict_Filename) - strlen(rule + 1);
      
      Conflict_Wildcard = strdup(Conflict_Filename);
      if(Conflict_Wildcard == NULL) {
	fprintf(stderr, "ASRLauncher(%d): Malloc error!\n", My_Pid);
	exit(1);
      }
      
      *(Conflict_Wildcard + diff) = '\0';
      
      fprintf(stderr, "ASRLauncher(%d): Wildcard variable $* = %s\n",
	      My_Pid, Conflict_Wildcard);
    }

    {
      int len;
      /* Read in dependencies */
      if(fgets(asrDependency, MAXDPNDLEN, rulesFile) == NULL) {
	fprintf(stderr, "ASRLauncher(%d): Malformed rule: %s\n",
		My_Pid, rule);
	return -1;
      }
      len = strlen(asrDependency);
      if(asrDependency[len-1] == '\n')
	asrDependency[len-1] = '\0';
      
      /* Read in command */
      if(fgets(asrCommand, MAXCOMMANDLEN, rulesFile) == NULL) {
	fprintf(stderr, "ASRLauncher(%d): Malformed rule: %s\n",
		My_Pid, rule);
	return -1;
      }
      len = strlen(asrCommand);
      if(asrCommand[len-1] == '\n')
	asrCommand[len-1] = '\0';
    }
  }

  return notfound;
}

int isConflict(char *path) {
  struct stat buf;
  char link[MAXPATHLEN];

  if(path == NULL)
    return 0;

  if(path[0] == '\0')
    return 0;

  /* Get stat() info. */
  if(lstat(path, &buf) < 0) {
    int error = errno;
    fprintf(stderr, "ASRLauncher(%d): lstat() failed on dependency %s: %s\n",
	    My_Pid, path, strerror(error));
    return 1;
  }

  /* Conflict defined as a dangling symlink. */
  if(!S_ISLNK(buf.st_mode))
    return 0;

  /* Read symlink contents. */
  if(readlink(path, link, MAXPATHLEN) < 0) {
    int error = errno;
    fprintf(stderr, "ASRLauncher(%d): readlink() failed: %s\n",
	    My_Pid, strerror(error));
    return 0;
  }
  
  /* The dangling symlink contents must begin with a '@', '#', or '$' */
  if((link[0] != '@') && (link[0] != '#') && (link[0] != '$'))
    return 0;

  return 1; /* passed all tests, considered a 'conflict' */
}

/*
 * replaceEnvVars
 *
 * Takes a string of text and replaces all instances of special
 * ASRLauncher environment variables with the appropriate filenames,
 * pathnames, or conflict types. Performed in place to avoid memory
 * allocations.
 *
 * Returns 0 on success and -1 on failure.
 */
int replaceEnvVars(char *string, int maxlen) {
  int len;
  char *trav;
  char *temp;

  if((string == NULL) || maxlen <= 0 )
    return -1;

  len = strlen(string);

  /* Allocate temporary buffer to perform copies into. */
  temp = (char *) malloc((maxlen)*sizeof(char));
  if(temp == NULL) {
    fprintf(stderr, "ASRLauncher(%d): Malloc error!\n", My_Pid);
    exit(1);
  }

  strncpy(temp, string, maxlen-1);
  fprintf(stderr, "ASRLauncher(%d): replaceEnvVars: Before: %s\n", 
	  My_Pid, string);

  trav = temp;
  while(*trav != '\0') {

    switch(*trav) {
      
    case '$':                /* marks beginning of a env variable name */
      {
	char *replace;        /* points to replacement str for env variable */

	/* Compare against known environment variables. */
	
	/* These are the important environment variables that make the
	 * .asr files much more useful. It's possible that user-defined 
	 * variables could be set within the .asr file itself and checked 
	 * here for convenience -- however, I haven't come across a case where
	 * this would be useful yet. */
	
	replace = NULL;

	switch(*(trav+1)) {

	case CONFLICT_FILENAME:
	  replace = Conflict_Filename;
	  break;
	
	case CONFLICT_PATH:
	  replace = Conflict_Path;
	  break;

	case CONFLICT_PARENT:
	  replace = Conflict_Parent;
	  break;
	
	case CONFLICT_WILDCARD:
	  replace = Conflict_Wildcard;
	  break;

	case CONFLICT_VOLUME:
	  replace = Conflict_Volume_Path;
	  break;

	case CONFLICT_TYPE:

	  switch(Conflict_Type) {

	  case SERVER_SERVER:
	    replace = SERVER_SERVER_STR;
	    break;
	    
	  case LOCAL_GLOBAL:
	    replace = LOCAL_GLOBAL_STR;
	    break;
	    
	  case MIXED_CONFLICT:
	    replace = MIXED_CONFLICT_STR;
	    break;
	    
	  default: /* Unknown conflict type, leave it untouched. */
	    break;
	  }

	  break;

	default:
	  break;
	}
	
	if(replace == NULL) { /* Unknown environment variable name. */
	  fprintf(stderr, "ASRLauncher(%d): Unknown environment variable!\n",
		  My_Pid);
	  trav++;
	  continue;
	}
	
	fprintf(stderr, "ASRLauncher(%d): Replacing environment "
		"variable with: %s\n", My_Pid, replace);
	
	{
	  char *src, *dest, *holder;
	  
	  /* See if the replacement string will fit in the buffer. */
	  if((strlen(temp) + strlen(replace)) > maxlen) {
	    fprintf(stderr, "ASRLauncher(%d): Replacement does not fit!\n",
		    My_Pid);
	    free(temp);
	    return -1;
	  }
	  
	  src = trav + 2;                     /* skip environment var name */
	  dest = trav + strlen(replace);
	  
	  if((holder = (char *)malloc(maxlen * sizeof(char))) == NULL) {
	    fprintf(stderr, "ASRLauncher(%d): malloc failed!\n", My_Pid);
	    free(temp);
	    return -1;
	  }

	  strcpy(holder, src);
	  strcpy(dest, holder);
	  strncpy(trav, replace, strlen(replace));
	  free(holder);

	  fprintf(stderr, "ASRLauncher(%d): During: %s\n", 
		  My_Pid, temp);
	}
	
	/* Set trav to the next unchecked character. */
	//trav += strlen(replace);
	break;
      }

    default:
      trav++;
      break;
    }
  }

  strncpy(string, temp, maxlen);
  free(temp);

  fprintf(stderr, "ASRLauncher(%d): After: %s\n", 
	  My_Pid, string);

  return 0;
}

int evaluateDependencies(char *dpndstr) {
  char *temp;
  char dpnd[MAXPATHLEN];

  if(dpndstr == NULL)
    return -1;

  /* Tokenize dependency line. */
    
  for(temp = strtok(dpndstr, " ");
      (temp = strtok(NULL, " ")) != NULL; ) {

    strcpy(dpnd, temp);

    /* Do environment variable expansion. */
    if(replaceEnvVars(dpnd, MAXPATHLEN) < 0) {
      fprintf(stderr, "ASRLauncher(%d): Failed replacing env vars on %s!\n",
	      My_Pid, dpnd);
      continue;
    }

    fprintf(stderr, "ASRLauncher(%d): Dependency: %s\n",
	    My_Pid, dpnd);

    /* Check if it is a conflict. */
    if(isConflict(dpnd)) {
      fprintf(stderr, "ASRLauncher(%d): Conflict detected!\n", My_Pid);
      return -1;
    }
  }

  return 0;
}


/*
 * Arguments passed by Venus:
 */

int
main(int argc, char *argv[])
{
  char asrCommand[MAXCOMMANDLEN];
  char asrDependency[MAXDPNDLEN];
  char *rulesPath;
  FILE *rules = NULL;
  int error;
  int len;

  My_Pid = getpid();

  if(argc != 4) {
    fprintf(stderr, "ASRLauncher(%d): Invalid number of arguments!\n", My_Pid);
    return 1;
  }

  Conflict_Path = argv[1];
  Conflict_Volume_Path = argv[2];
  Conflict_Type = atoi(argv[3]); 
  Conflict_Wildcard = NULL;

  if(Conflict_Path == NULL) {
    fprintf(stderr, "ASRLauncher(%d): Conflict pathname null!\n", My_Pid);
    return 1;
  }

  if(Conflict_Volume_Path == NULL) {
    fprintf(stderr, "ASRLauncher(%d): Volume pathname null!\n", My_Pid);
    return 1;
  }
  else {
    len = strlen(Conflict_Volume_Path);
    if(Conflict_Volume_Path[len-1] == '/') /* remove any trailing slash */
      Conflict_Volume_Path[len-1] = '\0';
  }

  if((Conflict_Type > MIXED_CONFLICT) || (Conflict_Type < SERVER_SERVER)) {
    fprintf(stderr, "ASRLauncher(%d): Unknown conflict type %d (%s)!\n", 
	    My_Pid, Conflict_Type, argv[3]);
    return 1;
  }

  Conflict_Filename = strrchr(Conflict_Path, '/');
  Conflict_Filename++;

  {					   /* Set up Conflict_Parent */
    char *temp;
    if(strlen(Conflict_Path) < MAXPATHLEN -1 )
       strcpy(Conflict_Parent, Conflict_Path);
  
    temp = strrchr(Conflict_Parent, '/');
    if(temp != NULL)
      *temp = '\0';
  }

  rulesPath = strdup(Conflict_Path);
  if(rulesPath == NULL)
    return 1;

  /*
   * Steps
   * 1.) Discover ASR rules file.
   * 2.) Parse ASR rules file.
   * 3.) Fork and execute appropriate ASR
   * 4.) Wait for ASR signal, timing out as appropriate
   * 5.) Die, telling Venus repair is complete or has failed.
   */

  *asrCommand = *asrDependency = '\0';

  fprintf(stderr, "ASRLauncher(%d): Conflict received: %s\n", 
	  My_Pid, Conflict_Path);
  fprintf(stderr, "ASRLauncher(%d): uid=%d, pid=%d, pgid=%d\n", 
	  My_Pid, getuid(), My_Pid, getpgrp());
  fprintf(stderr, "ASRLauncher(%d): Conflict's volume root: %s\n", 
	  My_Pid, Conflict_Volume_Path);
  fprintf(stderr, "ASRLauncher(%d): Conflict's parent dir: %s\n", 
	  My_Pid, Conflict_Parent);


 lexicalScope:
  
  *asrCommand = '\0';

  rules = openRulesFile(rulesPath);
  if(rules == NULL) {
    fprintf(stderr, "ASRLauncher(%d): Rules file doesn't exist!\n", My_Pid);
    goto skipParse;
  }

  error = parseRulesFile(rules, asrDependency, asrCommand);
  if(error) {
    fprintf(stderr, "ASRLauncher(%d): Failed parsing rules file!\n", My_Pid);
    return 1;
  }

  error = parseRulesFile(rules, conflictPath, asrCommand);

  if(*asrCommand == '\0') { /* Indicates no matching rule was found. */
    if(rules != NULL)
      fclose(rules);

    {
      char *temp;
      /* Scope one directory level higher. */
      temp = strrchr(rulesPath, '/');
      *temp = '\0';               /* Remove final slash. Parent directory
				   * then looks like a filename and is
				   * dropped in opening the new rules file. */

      /* Stop after volume root. */
      if(strcmp(Conflict_Volume_Path, rulesPath) == 0) { 
	fprintf(stderr, "ASRLauncher(%d): Failed to lexically scope the "
		"rules file!\n", My_Pid);
	return 1;
      }
    }
    
    goto lexicalScope;
  }
  fclose(rules); /* At this point, we have found an acceptable rule. */

  /* Ensure that no rule dependency is in conflict. */
  error = evaluateDependencies(asrDependency);
  if(error) {
    fprintf(stderr, "ASRLauncher(%d): Failed evaluating dependencies!\n",
	    My_Pid);
    return 1;
  }

  /* Expand any environment variables in our command to appropriate values. */
  error = replaceEnvVars(asrCommand, MAXCOMMANDLEN);
  if(error) {
    fprintf(stderr, "ASRLauncher(%d): Failed replacing environ vars!\n",
	    My_Pid);
    return 1;
  }


  if(Conflict_Wildcard != NULL) /* Old string no longer needed. */
    free(Conflict_Wildcard);

  {
    int status = 0, options = WNOHANG, i;
    pid_t pid;
    char *asr_argv[MAXPARMS], *trav;
    
    pid = fork();
    if(pid < 0) {
      fprintf(stderr, "ASRLauncher(%d): Failed forking ASR!\n",
	      My_Pid);
      return 1;
    }
    
    if(pid == 0) {      /* prepare the arguments to the ASR */
      int parms = 0, quotes = 0;
      char *parm_start;
      
      fprintf(stderr, "\nASRLauncher(%d): Command to execute: %s\n", 
	      My_Pid, asrCommand);

      /* mutilate the command string, making it an argv[] */
      
      trav = parm_start = asrCommand;
      while(parms < MAXPARMS-1) {
	
	fprintf(stderr, "%c", *trav);

	switch(*trav) {
	
	case ' ':		 		/* end of token? */
	case '\t':
	  if(parm_start == trav ) {
	    parm_start++;
	    trav++;
	    break;
	  }

	  if(!quotes) {
	    *trav = '\0';			/* isolate parameter */
	    asr_argv[parms] = parm_start;
	    parms++;
	    
	    trav++;
	    parm_start = trav;

	    fprintf(stderr, "ASRLauncher(%d): Param: %s\n", 
		    My_Pid, asr_argv[parms-1]);
	  }
	  else {
	    trav++;
	  }

	  break;	  

	case '\'':				/* beginning/end quote */
	case '"':

	  if(quotes) {				/* end quote (isolate parm) */
	    *trav = '\0';
	    asr_argv[parms] = parm_start;
	    parms++;
	    quotes = 0;
	  }
	  else { 				/* beginning quote */
	    quotes = 1;
	  }

	  trav++;
	  parm_start = trav;
	  break;

	case '\0':
  	  if(*parm_start != ' ') {		/* edge case */
	    asr_argv[parms] = parm_start;
	    parms++;
	  }
	  goto noMoreParms;	  

	default:
	  trav++;
	  break;
	}
      }

    noMoreParms:
      
      /* START DEBUG CODE */
      //      fprintf(log, "\n%d parameters\n", parms);
      //      for(i = 0;i < parms; i++)
	//	fprintf(log, "%s ", asr_argv[i]);
      //      fprintf(log, "\n");
      /* END DEBUG CODE */
      
      asr_argv[parms] = '\0'; /* must be null-terminated */

      fprintf(stderr, "ASRLauncher(%d): Params: %d\n", My_Pid, parms);
      
      for(i = 0; i < parms; i++)
	fprintf(stderr, "ASRLauncher(%d): Param %d: %s\n",
		My_Pid, i, asr_argv[i]);

      if(execvp(asr_argv[0], asr_argv) < 0) {
	int error = errno;
	fprintf(stderr, "ASRLauncher(%d): ASR exec() failed: %s\n", 
		My_Pid, strerror(error));
	exit(1);
      }
    }
    else {
      int runtime;

      for(runtime = 0;
	  (waitpid(pid, &status, options) == 0) && (runtime < ASR_TIMEOUT);
	  runtime++) {
	struct timeval timeout;

	timeout.tv_usec = 1000; 
	select(1, NULL, NULL, NULL, &timeout);		/* wait 10 ms */
      }
	
      if(runtime >= ASR_TIMEOUT) {

	fprintf(stderr, "ASRLauncher(%d): ASR Timed Out!\n", My_Pid);

	/* Destroy ASR process group. */
	kill(pid * -1, SIGKILL);

	/* The aborting of partial changes should be done in Venus, not here.
	 * Return failure to tell Venus to abort changes. */
	return 1;
      }

      /* Evaluate the status of the ASR. */
      if(WIFEXITED(status)) {
	int ret = WEXITSTATUS(status);
	fprintf(stderr, "ASRLauncher(%d): ASR terminated normally with "
		"return code %d.\n", My_Pid, ret);
      }
      else {
	fprintf(stderr, "ASRLauncher(%d): ASR terminated abnormally!\n",
		My_Pid);
	return 1;
      }
    }
  }

  //  fprintf(log, "asr succeeded\n");
  //  fclose(log);
  return 0;
}
