/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _PARSER_H_
#define _PARSER_H_

#define HISTORY	100		/* Don't let history grow unbounded    */
#define MAXARGS 100

typedef struct cmd {
    char 	* name;
    void 	(* func)(int, char **);
    struct cmd 	* sub_cmd;
    char *help;
} command_t;

typedef struct argcmd {
	char    *ac_name;
	int      (*ac_func)(int, char **);
	char     *ac_help;
} argcmd_t;

#ifdef __cplusplus
extern "C" {
#endif
void Parser_init(char *, command_t *);	/* Set prompt and load command list */
void Parser_commands();			/* Start the command parser */
void Parser_qhelp(int, char **);	/* Quick help routine */
void Parser_help(int, char **);		/* Detailed help routine */
void Parser_exit(int, char **);		/* Shuts down command parser */
int Parser_execarg(int argc, char **argv, argcmd_t cmds[]);
void execute_line(char * line);

/* Converts a string to an integer */
int Parser_int(char *, int *);

/* Prompts for a string, with default values and a maximum length */
char *Parser_getstr(const char *prompt, const char *deft, char *res, 
		    size_t len);

/* Prompts for an integer, with minimum, maximum and default values and base */
int Parser_getint(const char *prompt, long min, long max, long deft,
		  int base);

/* Prompts for a yes/no, with default */
int Parser_getbool(const char *prompt, int deft);

/* Extracts an integer from a string, or prompts if it cannot get one */
long Parser_intarg(const char *inp, const char *prompt, int deft,
		   int min, int max, int base);

/* Extracts a word from the input, or propmts if it cannot get one */
char *Parser_strarg(char *inp, const char *prompt, const char *deft,
		    char *answer, int len);

/* Extracts an integer from a string  with a base */
int Parser_arg2int(const char *inp, long *result, int base);
#ifdef __cplusplus
}
#endif


#endif _PARSER_H_
