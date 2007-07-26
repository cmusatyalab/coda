/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "asr.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "coda_string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>    
#include <sys/ioctl.h>    
#include <vcrcommon.h>
#include <time.h>
#include <coda.h>
extern int yylineno;
extern char yytext[];
extern int yylex();
extern int path(char *, char *, char *);

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include "ruletypes.h"

extern int yyparse();

// globals 
char cwd[MAXPATHLEN];
// moved into main because of bug with lex and yacc
//char incfname[MAXNAMLEN];		// the last component of path name of inc file
//char incdname[MAXPATHLEN];		// abs. path of directory where inc file exists
olist rules;				// list of rules parsed from RESOLVE file

int IsAbsPath(char *c) {
    if (c[0] == '/') return(1);
    else return(0);
}

FILE *FindResolveFile(char *dname) {
    char resfname[MAXPATHLEN];
    strcpy(resfname, dname);
    int foundresfile = 0;
    while (!foundresfile && strcmp("/coda", resfname) ) {
	struct stat statbuf;
	strcat(resfname, "/RESOLVE");
	DEBUG((stdout, "Looking for %s\n", resfname));
	if (!stat(resfname, &statbuf)) {
	    foundresfile = 1;
	    break;
	}
	char *c = strrchr(resfname, '/');	// finds the /RESOLVE
	*c = '\0';
	c = strrchr(resfname, '/');
	*c = '\0';
    }
    if (foundresfile) {
	if (debug)
	    fprintf(stderr, "Using resolve file %s\n", resfname);
	return(fopen(resfname, "r"));
    }
    else return(NULL);
}

// Find the rule that matches a filename 
rule_t *FindRule(olist *RuleList, char *dname, char *fname) {
    olist_iterator next(*RuleList);
    rule_t *r;
    while ((r = (rule_t *)next())) {
	if (r->match(dname, fname)) {
	    r->expand();
	    if (debug) {
		fprintf(stderr, "Found rule that matches\n");
		r->print(stderr);
	    }
	    return(r);
	}
    }
    return(NULL);
}

int ParseArgs(int argc, char **argv, char *incdname, char *incfname) {
    if ((argc < 2) || (argc > 3))  {
	fprintf(stderr, "Usage: %s [-d] <inc-filename>\n", argv[0]);
	return(-1);
    }

    if (argc > 2) {
	if (!strcmp(argv[1], "-d")) 
	    debug = 1;
	else {
	    fprintf(stderr, "Usage: %s [-d] <inc-filename>\n", argv[0]);
	    return(-1);
	}
    }
    // split inc file path  into dir and fname 
    getcwd(cwd, MAXPATHLEN);
    if (IsAbsPath(argv[argc - 1])) {
	path(argv[argc - 1], incdname, incfname);
    }
    else {
	char incname[MAXPATHLEN];
	strcpy(incname, cwd);
	strcat(incname, "/");
	strcat(incname, argv[argc - 1]);
	path(incname, incdname, incfname);
    }
    
    // look for the RESOLVE file 
    extern FILE *yyin;
    if (!(yyin = FindResolveFile(incdname))) {
	fprintf(stderr, "No RESOLVE file found\n");
	return(-1);
    }
    return(0);
}

#ifdef TIMING 
#define NSC_GET_COUNTER         _IOR('c', 1, long)
#endif

int main(int argc, char **argv)
{
    if (debug)
	fprintf(stderr, "Uid is %u Euid is %u\n", getuid(), geteuid());

    char incfname[CODA_MAXNAMLEN];		// the last component of path name of inc file
    char incdname[CODA_MAXPATHLEN];		// abs. path of directory where inc file exists

#ifdef TIMING 
    int clockFD = open("/dev/cntr0", O_RDONLY, 0666); 
    if (clockFD < 0) clockFD = 0; 
    unsigned long startparsertime = 0;
    unsigned long endparsertime = 0;
    unsigned long endyyparsetime = 0;
    if (clockFD)
	ioctl(clockFD, NSC_GET_COUNTER, &startparsertime);
#endif /* TIMING */
    
    // get file name 
    if (ParseArgs(argc, argv, incdname, incfname)) {
	fprintf(stderr, "Error in Parsing args\n");
	exit(-1);
    }
    yyparse();
#ifdef TIMING
    if (clockFD)
	ioctl(clockFD, NSC_GET_COUNTER, &endyyparsetime);
#endif /* TIMING */

    rule_t *r;
    if (debug) {
	fprintf(stderr, "Parsed rules are: \n");
	olist_iterator nextr(rules);
	while ((r = (rule_t *)nextr()))
	    r->print(stderr);
    }
    
    r = FindRule(&rules, incdname, incfname);
    if (r) {
	// expand all the arg names if necessary
	r->expand();
	
	// enable repair 
	if (r->enablerepair()) {
	    fprintf(stderr, "Couldn't do the begin repair\n");
	    exit(-1);
	}
	    
	// invoke commands
	if (!r->execute()) {
	    if (debug)
	    fprintf(stderr, "asr execution was successful\n");
	} else 
	    fprintf(stderr, "asr execution was unsuccessful\n");

 	// disable repair 
	r->disablerepair();
    }
    else 
	fprintf(stderr, "Couldn't find matching rule\n");
#ifdef TIMING     
    if (clockFD) {
	ioctl(clockFD, NSC_GET_COUNTER, &endparsertime);
	printf("startparsertime = %u endyyparsetime = %u endparsetime = %u parsingtime = %u totaltime = %u\n",
	       startparsertime, endyyparsetime, endparsertime,
	       (endyyparsetime > startparsertime) ?	
	       (endyyparsetime - startparsertime)/25 :
	       (171798691 - ((startparsertime  - endyyparsetime )/25)), 
	       (endparsertime > startparsertime) ?	
	       (endparsertime - startparsertime)/25 :
	       (171798691 - ((startparsertime  - endparsertime )/25)));
    }
#endif /* TIMING */
    
    exit(0);
}
