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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/norton/Attic/parser.cc,v 4.1 1997/01/08 21:49:52 rvb Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#ifndef	__MACH__
#include <libc.h>
#endif
#include <libcs.h>
#include <ctype.h>
#include <string.h>

#include <readline/readline.h>	/* this lives in /usr/misc/.gnu-comp/include */
/* Keep C++ happy */
extern void using_history();
extern void stifle_history(int);
extern void add_history(char *);

#ifdef __cplusplus
}
#endif __cplusplus


#include "parser.h"

static command_t * top_level;	   /* Top level of commands, initialized by
				    * InitParser  			    */
static command_t * match_tbl;	   /* Command completion against this table */
static char * parser_prompt = NULL;/* Parser prompt, set by InitParser      */
static int done;		   /* Set to 1 if user types exit or quit   */



static char * skipwhitespace(char * s) {
    char * t;
    int    len;

    len = (int)strlen(s);
    for (t = s; t <= s + len && isspace(*t); t++);
    return(t);
}


static char * skiptowhitespace(char * s) {
    char * t;

    for (t = s; *t && !isspace(*t); t++);
    return(t);
}

    

    
static command_t * find_cmd(char * name, command_t cmds[], char ** next) {
    int    i,
	   len;

    if (!cmds) return NULL;
    
    name = skipwhitespace(name);
    *next = skiptowhitespace(name);
    len = *next - name;
    if (len == 0) return NULL;

    for (i = 0; cmds[i].name; i++) {
	if (strncasecmp(name, cmds[i].name, len) == 0) {
	    *next = skipwhitespace(*next);
	    return(&cmds[i]);
	}
    }
    return NULL;
}


static char * command_generator(char * text, int state) {
    static int index,
	       len;
    char       *name;

    /* Do we have a match table? */
    if (!match_tbl) return NULL;
    
    /* If this is the first time called on this word, state is 0 */
    if (!state) {
	index = 0;
	len = (int)strlen(text);
    }

    /* Return the next name in the command list that paritally matches test */
    while (name = (match_tbl + index)->name) {
	index++;

	if (strncasecmp(name, text, len) == 0) {
	    return(strdup(name));
	}
    }

    /* No more matches */
    return NULL;
}


static char **command_completion(char * text, int start, int end) {
    command_t 	* table;
    char	* pos;

    match_tbl = top_level;
    for (table = find_cmd(rl_line_buffer, match_tbl, &pos);
	 table;
	 table = find_cmd(pos, match_tbl, &pos)) {

	if (*(pos - 1) == ' ') match_tbl = table->sub_cmd;
    }

    return(completion_matches(text, command_generator));
}


static void execute_line(char * line) {
    char 	* args,
		* prev,
		* tmp,
		  buf[256],
		* argv[MAXARGS];
    command_t 	* cmd,
		* last_match = NULL,
		* ambig;
    int		  i;

    buf[0] = '\0';
    
    prev = line;
    for (cmd = find_cmd(line, top_level, &args);
	 cmd;
	 cmd = find_cmd(args, cmd->sub_cmd, &args)) {

	strcat(buf, cmd->name);
	
	last_match = cmd;
	
	tmp = skipwhitespace(prev);
	ambig = find_cmd(prev, cmd + 1, &prev);
	if (ambig) {
	    *prev = '\0';
	    fprintf(stderr, "'%s' ambiguous. Possible completions are:\n\t", tmp);
	    while((ambig = find_cmd(tmp, cmd, &prev))) {
		fprintf(stderr, "%s ", ambig->name);
		cmd++;
	    }
	    fprintf(stderr, "\n");
	    return;
	}

	if (!cmd->sub_cmd) break;
    }
	
    if (last_match) {
	/* If the "previous" token is at the end of 'line' then we
	   have an incomplete command. Otherwise we have an unknown
	   command.  Skip any whitespace just to be sure we are at the end
	   of the line. */
	prev = skipwhitespace(prev);
	if (last_match->func) {
	    /* We have a command with arguments, tokenize the line and
	     * execute it.
	     */

	    args = skipwhitespace(line);
	    i = 0;
	    while(*args) {
		argv[i] = nxtarg(&args, NULL);
		args = skipwhitespace(args);
		i++;
	    }
	    (last_match->func)(i, argv);
	    return;
	} else if (!cmd && !*prev) {
	    fprintf(stderr,
		    "'%s' incomplete command.  Use '%s x' where x is one of:\n",
		    buf, buf);
	    fprintf(stderr, "\t");
	    for (i = 0; last_match->sub_cmd[i].name; i++) {
		fprintf(stderr, "%s ", last_match->sub_cmd[i].name);
	    }
	    fprintf(stderr, "\n");
	} else {
	    fprintf(stderr, "Unknown command '%s %s'.\n", buf, prev);
	}
    } else {
	fprintf(stderr, "Unknown command '%s'.\n", line);
    }

    return;
}




void parse_commands() {
    char *line,
	 *s;

    using_history();
    stifle_history(HISTORY);

    rl_attempted_completion_function =(CPPFunction *)command_completion;
    rl_completion_entry_function = (Function *)command_generator;
    
    while(!done) {
	line = readline(parser_prompt);

	if (!line) break;

	s = skipwhitespace(line);

	if (*s) {
	    add_history(s);
	    execute_line(s);
	}

	free(line);
    }
}


void init_parser(char * prompt, command_t * cmds) {
    done = 0;
    top_level = cmds;
    if (parser_prompt) free(parser_prompt);
    parser_prompt = strdup(prompt);
}


void exit_parser(int argc, char *argv[]) {
    done = 1;
    free(parser_prompt);
    parser_prompt = NULL;
}



int parse_int(char *s, int *val)
{
    int ret;

    if (*s != '0')
	ret = sscanf(s, "%ld", val);
    else if (*(s+1) != 'x')
	ret = sscanf(s, "%lo", val);
    else {
	s++;
	ret = sscanf(++s, "%lx", val);
    }

    return(ret);
}


/*************************************************************************
 * COMMANDS								 *
 *************************************************************************/ 


static void print_commands(char * str, command_t * table) {
    command_t * cmds;
    char 	buf[80];

    for (cmds = table; cmds->name; cmds++) {
	if (cmds->func) {
	    if (str) printf("\t%s %s\n", str, cmds->name);
	    else printf("\t%s\n", cmds->name);
	}
	if (cmds->sub_cmd) {
	    if (str) {
		sprintf(buf, "%s %s", str, cmds->name);
		print_commands(buf, cmds->sub_cmd);
	    } else {
		print_commands(cmds->name, cmds->sub_cmd);
	    }
	}
    }
}

    
void quick_help(int argc, char *argv[]) {
    printf("Available commands are:\n");

    print_commands(NULL, top_level);
}

