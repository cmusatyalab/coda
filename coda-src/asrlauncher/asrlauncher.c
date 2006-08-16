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
 * This file contains all of the code that makes up the ASRLauncher binary.
 *
 * Author: Adam Wolbach
 */


/* System includes */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


/* Venus includes */

#include <coda_config.h>


/* Definitions */

#ifndef NCARGS
#define NCARGS 8192 
#endif

/* Globally-defined ASR rules file name. */
#define ASR_RULES_FILENAME ".asr"

/* ASR timeout period (milliseconds). Currently this is identical to RPC2's. */
#define ASR_TIMEOUT 60000
#define TRIGGER_TIMEOUT ASR_TIMEOUT


/* Environment variable names */
#define CONFLICT_BASENAME             '>' /* Puneet's original 3 vars */
#define CONFLICT_PARENT               '<'
#define SYSTYPE_VAR                   '@'

#define CONFLICT_PATH                 '=' /* Additional variables */
#define CONFLICT_VOLUME               ':'
#define CONFLICT_TYPE                 '!'

/* Conflict type enumerations.
 * Note: these values are standard also in Venus. */

#define SERVER_SERVER        1
#define SERVER_SERVER_STR   "1"
#define SERVER_SERVER_CHAR  'S'
#define LOCAL_GLOBAL         2
#define LOCAL_GLOBAL_STR    "2"
#define LOCAL_GLOBAL_CHAR   'L'
#define MIXED_CONFLICT       3
#define MIXED_CONFLICT_STR  "3"
#define MIXED_CONFLICT_CHAR 'M'


/* Global Variables */

/* ASRLauncher-related Globals */

int My_Pid;                               /* for logging purposes */
char Local_Policy_Path[MAXPATHLEN];       /* list of enabled ASR rules files */

/* Conflict Information Globals */

int Conflict_Type;                     
char Conflict_Path[MAXPATHLEN];        /* Full path to the conflict. */
char Conflict_Parent[MAXPATHLEN];      /* Parent directory of conflict. */
char *Conflict_Basename;               /* Just the filename to the conflict. */
char Conflict_Volume_Path[MAXPATHLEN]; /* Path to conflict's volume root. */

/* Lexical Scoping/Rule Parsing Globals */

FILE *Rules_File;
char Rules_File_Path[MAXPATHLEN];
char Rules_File_Parent[MAXPATHLEN];



/*
 * escapeString
 *
 * Replaces all instances of special metacharacters in a string with their
 * 'escaped' counterparts, such that a shell will read them for their
 * character data and not as a control symbol.
 *
 * The list of metacharacters comes from the WWW Security FAQ, referenced by
 * http://www.dwheeler.com/secure-programs/Secure-Programs-HOWTO/handle-metacharacters.html
 * Other characters may also be appropriate, but tend to be shell-specific.
 *
 * Returns 0 on success, 1 on failure
 */

int escapeString(char *str, int maxlen) {
  int len, i;
  char *tempstr;

  if(str == NULL || maxlen <= 0 ) {
	fprintf(stderr, "ASRLauncher(%d): Corrupt pathname.\n", My_Pid);
	return 1;
  }

  tempstr = (char *) malloc(maxlen * sizeof(char));
  if(tempstr == NULL) {	perror("malloc"); exit(EXIT_FAILURE); }
   
  strncpy(tempstr, str, maxlen-1);
  str[maxlen-1] = '\0';

  len = strlen(str);
  for(i = 0; (tempstr[i] != '\0') && (len < (maxlen - 1)); i++) {

	switch(tempstr[i]) {

	case '\n': /* special case: see man BASH(1) */

	  if((len + 2) < (maxlen - 1)) {
		memmove(&(tempstr[i+2]), &(tempstr[i]), strlen(&(tempstr[i])));
		tempstr[i] = '\\';
		tempstr[i+1] = '\\';
		i++;
	  }
	  else {
		fprintf(stderr, "ASRLauncher(%d): Pathname buffer overflow.\n", 
				My_Pid);
		free(tempstr);
		return 1;
	  }
	  break;

	case '`':
	case '\"':
	case '\'':
	case '[':
	case ']':
	case '{':
	case '}':
	case '(':
	case ')':
	case '~':
	case '^':
	case '<':
	case '>':
	case '&':
	case '|':
	case '?':
	case '*':
	case '$':
	case ';':
	case '\r':
	case '\\':

	  if((len + 1) < (maxlen - 1)) {
		memmove(&(tempstr[i+1]), &(tempstr[i]), strlen(&(tempstr[i])));
		tempstr[i++] = '\\';
	  }
	  else {
		fprintf(stderr, "ASRLauncher(%d): Pathname buffer overflow.\n", 
				My_Pid);
		free(tempstr);
		return 1;
	  }
	  break;

	default:
	  continue;
	}

	len = strlen(tempstr);
  }

  strcpy(str, tempstr);
  free(tempstr);

  return 0;
}


/*
 * nameNextRulesFile
 *
 * Takes the pathname of an ASR rules file (or any file),
 * creates the pathname of the rules file scoped up one level in the directory
 * hierarchy, and writes it back into the storage parameter. If the empty 
 * or null string is sent in as the path, then we use Conflict_Path as 
 * our pathname.
 *
 * Returns 0 on success, nonzero on failure.
 */

int nameNextRulesFile(char *path) {
  char *parent, temp[MAXPATHLEN];

  if((path != NULL) && (strlen(path) > 0)) { /* Scope one level higher. */
	strncpy(temp, path, MAXPATHLEN);

	parent = strrchr(temp, '/');
	if(parent == NULL)
	  return 1;
	
	parent[0] = '\0';
  }
  else                  /* Use current directory level's rules file. */
	strncpy(temp, Conflict_Path, MAXPATHLEN);

  parent = strrchr(temp, '/');
  if(parent == NULL)
	return 1;
  
  parent[1] = '\0';

  strncpy(Rules_File_Parent, temp, MAXPATHLEN);

  if(strcat(temp, ASR_RULES_FILENAME) == NULL)
	return 1;

  strncpy(Rules_File_Path, temp, MAXPATHLEN);

  return 0;
}


/* checkRulesFile
 *
 * Check whether the ASR rules file pathname sent in is enabled
 * locally on this system. The list of enabled rules files is stored in a text
 * file named in venus.conf. If this file is not named or doesn't exist, no
 * ASRs can be launched for security reasons.
 */

int checkRulesFile(char *pathname) {
  int ret = 1;
  FILE *allow_list;
  char allow_path[MAXPATHLEN], rules_path[MAXPATHLEN], *error, *last_slash;

  if(Local_Policy_Path == NULL)
	return 1;

  if(pathname == NULL)
	return 1;

  if((allow_list = fopen(Local_Policy_Path, "r")) == NULL) {
	fprintf(stderr, "ASRLauncher(%d): Couldn't open local ASR policy!\n",
			My_Pid);
	return 1;
  }
  
  strcpy(rules_path, pathname);
  last_slash = strrchr(rules_path, '/');
  last_slash[1] = '\0';

  /* Check paths one-by-one, line-by-line. */
  while((error = fgets(allow_path, MAXPATHLEN, allow_list)) != NULL) {
	int len = strlen(allow_path);
	
	if(allow_path[len-1] == '\n') {
	  allow_path[len-1] = '\0';
	  len--;
	}

	if(allow_path[len-1] != '/' && len < (MAXPATHLEN - 1)) {
	  allow_path[len] = '/';
	  allow_path[len+1] = '\0';
	  len++;
	}

	/* Double slashes indicate the entire directory hierarchy is allowed. */
	if((allow_path[len-1] == '/') && (allow_path[len-2] == '/')) {
	  char c = rules_path[len-2];

	  allow_path[len-2] = '\0';
	  rules_path[len-2] = '\0';

	  if(strcmp(allow_path, rules_path) == 0) {
		ret = 0;
		break;
	  }

	  rules_path[len-2] = c;	  
	  continue;
	}

	if(strcmp(allow_path, rules_path) == 0) {
	  ret = 0;
	  break;
	}
  }
  
  fclose(allow_list);

  if(ret)
	fprintf(stderr, "ASRLauncher(%d): Security policy disallows %s\n",
			My_Pid, rules_path);

  return ret;
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
  if(temp == NULL) { perror("malloc");	exit(EXIT_FAILURE); }

  strncpy(temp, string, maxlen-1);

  trav = temp;
  while(*trav != '\0') {
	int varlen = 2;

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

		case SYSTYPE_VAR:
		  replace = SYSTYPE;
		  break;
		  
		case CONFLICT_BASENAME:
		  replace = Conflict_Basename;
		  break;
		  
		case CONFLICT_PATH:
		  replace = Conflict_Path;
		  break;
		  
		case CONFLICT_PARENT:
		  replace = Conflict_Parent;
		  break;
		  
		case CONFLICT_VOLUME:
		  replace = Conflict_Volume_Path;
		  break;
		  
		case CONFLICT_TYPE:

		  switch(*(trav+2)) {
			
		  case SERVER_SERVER_CHAR:
			varlen = 3;
			replace = SERVER_SERVER_STR;
			break;
			
		  case LOCAL_GLOBAL_CHAR:
			varlen = 3;
			replace = LOCAL_GLOBAL_STR;
			break;
			
		  case MIXED_CONFLICT_CHAR:
			varlen = 3;
			replace = MIXED_CONFLICT_STR;
			break;
			
		  default:
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
		  }
		  break;
		  
		default:
		  break;
		}
		
		if(replace == NULL) { /* Unknown environment variable name. */
		  trav++;
		  continue;
		}
		
		{
		  char *src, *dest, *holder;
		  
		  /* See if the replacement string will fit in the buffer. */
		  if((strlen(temp) + strlen(replace)) > maxlen) {
			fprintf(stderr, "ASRLauncher(%d): Replacement does not fit!\n",
					My_Pid);
			free(temp);
			return 1;
		  }
		  
		  src = trav + varlen;                  /* skip environment var name */
		  dest = trav + strlen(replace);
		  
		  holder = (char *)malloc(maxlen * sizeof(char));
		  if(holder == NULL) { 
			perror("malloc");	
			free(temp);
			exit(EXIT_FAILURE); 
		  }
		  
		  strcpy(holder, src);
		  strcpy(dest, holder);
		  strncpy(trav, replace, strlen(replace));
		  free(holder);
		}
		
		break;
      }
	  
    default:
      trav++;
      break;
    }
  }
  
  if(strcmp(string, temp))
	strncpy(string, temp, maxlen-1);

  free(temp);
  
  return 0;
}

/* executeTriggers
 *
 * Run the triggers in the specified rules file, stopping when one succeeds
 * or we run out of them. Also, as a side effect, fills in cmds with a
 * pointer to the beginning of the command-list, if a trigger succeeds.
 *
 * @param long *cmds A pointer to where the correct command-list begins
 * @returns 0 on successful trigger, nonzero on trigger failure or error
 */

int executeTriggers(long *cmds) {
  int trigger_not_hit = 1, trigger_num = 0;
  long line;
  char trigger[NCARGS];

  if((Rules_File == NULL) || (cmds == NULL))
    return 1;

  while(trigger_not_hit) { /* Each iteration assumes the start of a line. */
    char c;
	int i;

	line = ftell(Rules_File);


	/* Get first character from the current line. */

	c = fgetc(Rules_File);

	if(c == '\n')
	  continue;
	else if(c == EOF) {
	  fprintf(stderr, "ASRLauncher(%d): No rule was triggered in %s\n",
			  My_Pid, Rules_File_Path);
	  return 1;
	}
	else if(c != '`') {
	  /* Ignore the rest of this line if the first char isn't ` */
	  do { c = fgetc(Rules_File); }
	  while((c != '\n') && (c != EOF));
			 
      continue;
	}

	trigger_num++; /* Just useful information for debugging. */

	for(i = 0; i < NCARGS; i++) {

	  trigger[i] = fgetc(Rules_File);

	  if(trigger[i] == '`') {
		if((i > 0) && (trigger[i-1] == '\\')) { /* escaped ` */
		  trigger[i-1] = trigger[i];
		  i--;
		}
		else{
		  trigger[i] = '\0';
		  break;
		}
	  }
	  else if(trigger[i] == EOF) {
		fprintf(stderr, "ASRLauncher(%d): Trigger %d: syntax error(file %s)\n",
				My_Pid, trigger_num, Rules_File_Path);
		return 1;
	  }
	}

	if(i >= NCARGS) {
	  fprintf(stderr, "ASRLauncher(%d): Trigger %d: Length exceeds NCARGS "
			  "(file %s)\n", My_Pid, trigger_num, Rules_File_Path);
	  return 1;
	}

	/* Execute trigger here. */

	{
	  int status = 0, options = WNOHANG, error, pfd[2];
	  pid_t pid;
	  char *asr_argv[4];
	  
	  if(pipe(pfd) == -1) { perror("pipe"); exit(EXIT_FAILURE); }
	  
	  pid = fork();
	  if(pid == -1) { perror("fork"); exit(EXIT_FAILURE); }
	  
	  if(pid == 0) {
		
		/* Make the child shell think that pfd[0] is stdin. */
		
		close(pfd[1]);
		dup2(pfd[0], STDIN_FILENO);
		
		asr_argv[0] = "/bin/sh"; 
		asr_argv[1] = NULL;
		
		if(execvp(asr_argv[0], asr_argv) < 0) {
		  perror("exec");
		  close(pfd[0]);
		  exit(EXIT_FAILURE); 
		}
	  }
	  else {
		int ms;
		
		close(pfd[0]);
		
		/* Now, pipe data to the child shell. */
		
		/* Expand any environment variables in our 
		 * command to their appropriate strings. */		
	  
		error = replaceEnvVars(trigger, NCARGS);
		if(error) {
		  fprintf(stderr, "ASRLauncher(%d): Failed replacing trigger %d's "
				  "environment variables!\n", My_Pid, trigger_num);
		  return 1;
		}

		fprintf(stderr, "\nASRLauncher(%d): Trigger to execute:\n\n", My_Pid);
		fprintf(stderr, "%s\n\n", trigger);
		  
		if(write(pfd[1], (void *) trigger, strlen(trigger)) < 0) 
		  { perror("write"); exit(EXIT_FAILURE); }
		
		close(pfd[1]);
	
		for(ms = 0; ms < TRIGGER_TIMEOUT; ms += 10) {
		  int newpid;
		  struct timeval timeout;
		  
		  timeout.tv_sec = 0;
		  timeout.tv_usec = 10000; 
		  
		  select(0, NULL, NULL, NULL, &timeout);		/* wait 10 ms */
		  
		  newpid = waitpid(pid, &status, options);
		  if(newpid == 0) continue;
		  else if(newpid > 0) break;
		  else if(newpid < 0) { perror("waitpid"); exit(EXIT_FAILURE); }
		}
		
		if(ms >= TRIGGER_TIMEOUT) {
		  
		  fprintf(stderr, "ASRLauncher(%d): Trigger timed out!\n", My_Pid);
		  
		  /* Destroy ASR process group. */
		  kill(pid * -1, SIGKILL);
		  
		  /* The aborting of partial changes should be done in Venus, not here.
		   * Return failure to tell Venus to abort any writes. */
		  return 1;
		}
		
		/* Evaluate the status of the ASR. */
		if(WIFEXITED(status)) {
		  int ret = WEXITSTATUS(status);

		  fprintf(stderr, "ASRLauncher(%d): Trigger terminated normally with "
				  "return code %d.\n", My_Pid, ret);

		  if(ret == 0) { /* Trigger has hit. */
			trigger_not_hit = 0;
			break;
		  }
		}
		else {
		  fprintf(stderr, "ASRLauncher(%d): Trigger terminated abnormally! "
				  "Not continuing the ASRLaunch. Check your rules file!\n",
				  My_Pid);
		  return 1;
		}
	  }
	}

	/* Advance to next line. */
	{
	  char c;
	  do { c = fgetc(Rules_File); }
	  while((c != '\n') && (c != EOF));
	}

  } /* while(trigger_not_hit) */
  
  if(trigger_not_hit == 0) {
	char c;

	/* Mark data to be used later in the launch. */
	
	if(fseek(Rules_File, line, SEEK_SET) < 0) 
	  { perror("fseek"); exit(EXIT_FAILURE); }
	
	/* Mark command list. */
	
	do { c = fgetc(Rules_File); }
	while((c != '\n') && (c != EOF));
	
	if((*cmds = ftell(Rules_File)) < 0) 
	  { perror("ftell"); exit(EXIT_FAILURE); }
  }

  return trigger_not_hit;
} /* executeTriggers */


/*
 * findRule
 *
 * Fills in a rule struct with the data associated with a matching rule,
 * lexically scoping up the conflict's pathname if necessary.
 *
 * @param long *cmds File position of beginning of command-list
 * @returns 0 on success, nonzero on failure
 *
 */

int findRule(long *cmds) {
  int scope;

  if(cmds == NULL)
	return 1;

  Rules_File_Path[0] = '\0';

  while((scope = strcmp(Conflict_Volume_Path, Rules_File_Parent)) != 0) {   

	/* Piece together the name of the next potential rules file. */
	if(nameNextRulesFile(Rules_File_Path))
	  return 1;

	/* Check if the local allow/deny policy allows execution of this file. */
	if(checkRulesFile(Rules_File_Path))
	  continue;

	/* Open the ASR rules file in Codaland. This could fail with ENOENT or 
	 * EACCES at which point we scope to a higher directory and try again. */
	if((Rules_File = fopen(Rules_File_Path, "r")) == NULL)
	  continue;

	/* Start running triggers, looking for one that indicates success. */
	if(executeTriggers(cmds)) {
	  if(fclose(Rules_File))
		perror("fclose");
	  Rules_File = NULL;
	  continue;
	}

	/* If we made it here, we have found a matching rule. */
	break;   
  }
  
  if(scope == 0) /* Stop after volume root. */
	return 1;

  return 0;
} /* findRule */

int executeCommands(long cmds) {

  int status = 0, options = WNOHANG, error, pfd[2];
  pid_t pid;
  char *asr_argv[4];

  if(cmds < 0) {
    fprintf(stderr, "ASRLauncher(%d): Empty/bad command list!\n", My_Pid);
    return 1;
  }
  
  if(pipe(pfd) == -1) { perror("pipe"); exit(EXIT_FAILURE); }
  
  pid = fork();
  if(pid == -1) { perror("fork"); exit(EXIT_FAILURE); }
  
  if(pid == 0) {

	/* Make the child shell think that pfd[0] is stdin. */

	close(pfd[1]);
	dup2(pfd[0], STDIN_FILENO);

	asr_argv[0] = "/bin/sh"; 
	asr_argv[1] = NULL;
	
	if(execvp(asr_argv[0], asr_argv) < 0) {
	  perror("exec");
	  close(pfd[0]);
	  exit(EXIT_FAILURE); 
	}
  }
  else {
	int ms;
	char command[NCARGS], fmt[10], firstword[MAXPATHLEN];

	close(pfd[0]);
	
	if(fseek(Rules_File, cmds, SEEK_SET) < 0)   /* Go to command list. */
	  { perror("fseek"); exit(EXIT_FAILURE); }

	/* Now, pipe data to the child shell. */

	fprintf(stderr, "\nASRLauncher(%d): Commands to execute:\n\n", My_Pid);

	do {
	  long line;

	  if((line = ftell(Rules_File)) < 0) 
		{ perror("ftell"); exit(EXIT_FAILURE); }
	  
	  /* Get first word from the current line. */
	  sprintf(fmt, "%%%ds", MAXPATHLEN-1);
	  if(fscanf(Rules_File, fmt, firstword) == EOF)
		break;
	  
	  if(strchr(firstword, ':') != NULL) /* New rule. */
		break;
	  
	  if(fseek(Rules_File, line, SEEK_SET) < 0) /* Rewind to start of line. */
		{ perror("fseek"); exit(EXIT_FAILURE); }
	  
	  if(fgets(command, NCARGS, Rules_File) == NULL) /* EOF */
		break;
	  
	  /* Expand any environment variables in our 
	   * command to their appropriate strings. */		
	  
	  error = replaceEnvVars(command, NCARGS);
	  if(error) {
		fprintf(stderr, "ASRLauncher(%d): Failed replacing environment "
				"variables!\n", My_Pid);
		return 1;
	  }

	  fprintf(stderr, "%s", command);

	  if(write(pfd[1], (void *) command, strlen(command)) < 0) 
		{ perror("write"); exit(EXIT_FAILURE); }
	} while(1);
	
	fprintf(stderr, "\n\n");
	  
	close(pfd[1]);

	for(ms = 0; ms < ASR_TIMEOUT; ms += 10) {
	  int newpid;
	  struct timeval timeout;

	  timeout.tv_sec = 0;
	  timeout.tv_usec = 10000; 

	  select(0, NULL, NULL, NULL, &timeout);		/* wait 10 ms */

	  newpid = waitpid(pid, &status, options);
	  if(newpid == 0) continue;
	  else if(newpid > 0) break;
	  else if(newpid < 0) { perror("waitpid"); exit(EXIT_FAILURE); }
	}
	
	if(ms >= ASR_TIMEOUT) {
	  
	  fprintf(stderr, "ASRLauncher(%d): ASR timed out!\n", My_Pid);
	  
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
  
  return 0;
}

/*
 * Arguments passed by Venus:
 */

int
main(int argc, char *argv[])
{
  long cmds;
  int error, len;

  My_Pid = getpid();

  if(argc != 5) {
    fprintf(stderr, "ASRLauncher(%d): Invalid number of arguments!\n", My_Pid);
    return 1;
  }


  /* Conflict_Path init */

  strncpy(Conflict_Path, argv[1], MAXPATHLEN-1);


  /* Conflict_Volume_Path init */

  strncpy(Conflict_Volume_Path, argv[2], MAXPATHLEN-2);
  len = strlen(Conflict_Volume_Path);
  if(Conflict_Volume_Path[len-1] != '/') { /* add a trailing slash */
	Conflict_Volume_Path[len] = '/';
	Conflict_Volume_Path[len+1] = '\0';
  }


  /* Conflict_Type init */

  Conflict_Type = atoi(argv[3]); 

  if((Conflict_Type > MIXED_CONFLICT) || (Conflict_Type < SERVER_SERVER)) {
    fprintf(stderr, "ASRLauncher(%d): Unknown conflict type %d (%s)!\n", 
	    My_Pid, Conflict_Type, argv[3]);
    return 1;
  }


  /* Local_Policy_Path init */

  if(argv[4] == NULL) {
	fprintf(stderr, "ASRLauncher(%d): No local ASR policy defined "
			"in venus.conf!\n", My_Pid);
	return 1;
  }

  strncpy(Local_Policy_Path, argv[4], MAXPATHLEN-1);
  fprintf(stderr, "ASRLauncher(%d): Using local ASR policy in %s\n", 
		  My_Pid, Local_Policy_Path);


  /* Conflict_Basename init */

  Conflict_Basename = strrchr(Conflict_Path, '/');
  if(*(Conflict_Basename + 1) == '\0') { /* Directory conflicts end in '/' */
	char *temp = Conflict_Basename;
	Conflict_Basename[0]='\0';
	Conflict_Basename = strrchr(Conflict_Path, '/');
	temp[0] = '/';
  }
  Conflict_Basename++;


  /* Conflict_Parent init */
  {
    char *temp;
	strcpy(Conflict_Parent, Conflict_Path);
  
    temp = strrchr(Conflict_Parent, '/');
    if(temp != NULL)
      *temp = '\0';
	else {
	  fprintf(stderr, "ASRLauncher(%d): %s has no parent!\n", 
			  My_Pid, Conflict_Path);
	  return 1;
	}
  }

  /* 
   * Escaping of filename, pathname characters done here. This is necessary
   * because it is possible to hide shell commands within a filename, i.e.
   * a rule that launches for *.odt could run into a conflict named
   * "foo; foo.sh; .odt" which clearly cannot be piped directly into /bin/sh.
   */
  
  escapeString(Conflict_Path, MAXPATHLEN);
  escapeString(Conflict_Parent, MAXPATHLEN);
  escapeString(Conflict_Volume_Path, MAXPATHLEN);

  fprintf(stderr, "ASRLauncher(%d): Conflict received: %s\n", 
	  My_Pid, Conflict_Path);
  fprintf(stderr, "ASRLauncher(%d): uid=%d, pid=%d, pgid=%d\n", 
	  My_Pid, getuid(), My_Pid, getpgrp());
  fprintf(stderr, "ASRLauncher(%d): Conflict's volume root: %s\n", 
	  My_Pid, Conflict_Volume_Path);


  /*
   * Steps
   * 1.) Discover associated rule by executing triggers until one succeeds.
   * 2.) Fork and execute appropriate ASR commands.
   * 3.) Wait for ASR signal, timing out as appropriate.
   * 4.) Die, telling Venus that the ASR is complete or has failed.
   */

  /* Find a triggered rule and parse its information. */
  error = findRule(&cmds);
  if(error) {
	fprintf(stderr, "ASRLauncher(%d): Failed finding valid associated "
			"rule!\n", My_Pid);
	goto endLaunch;
  }

  /* Execute all commands indented below the rule name. */
  error = executeCommands(cmds);
  if(error) {
	fprintf(stderr, "ASRLauncher(%d): Failed executing related commands!\n",
			My_Pid);
	goto endLaunch;
  }

 endLaunch:
  
  if(Rules_File != NULL)
	fclose(Rules_File);

  return (error ? 1 : 0);
}
