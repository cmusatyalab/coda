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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/norton/parser.h,v 4.1 1997/01/08 21:49:53 rvb Exp $";
#endif /*_BLURB_*/




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


#ifdef __cplusplus
extern "C" {
#endif
void Parser_init(char *, command_t *);	/* Set prompt and load command list */
void Parser_commands();			/* Start the command parser */
void Parser_qhelp(int, char **);	/* Quick help routine */
void Parser_help(int, char **);		/* Detailed help routine */
void Parser_exit(int, char **);		/* Shuts down command parser */

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
